#include "pipewireaudio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "chain.h"
#include "take.h"

PipeWireAudio::PipeWireAudio(QObject *parent) : AudioSystem(parent) {
    auto emitLevels = [this] { emit levels(m_micMeter.level(), m_voxMeter.level()); };
    connect(&m_micMeter, &Meter::levelChanged, this, emitLevels);
    connect(&m_voxMeter, &Meter::levelChanged, this, emitLevels);
    connect(&m_loopback, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        emit listeningChanged(state != QProcess::NotRunning);
    });
}

PipeWireAudio::~PipeWireAudio() {
    for (const auto &playing : std::as_const(m_playing))
        for (QProcess *process : {playing.mix, playing.monitor})
            if (process)
                process->kill();
    for (auto *process : findChildren<QProcess *>(Qt::FindDirectChildrenOnly)) {
        if (process == &m_loopback || process->state() == QProcess::NotRunning)
            continue;
        process->kill();
        process->waitForFinished(200);
    }
    stopMetering();
    setListening(false);
}

void PipeWireAudio::run(const QString &program, const QStringList &arguments,
                        std::function<void(int, const QByteArray &, const QByteArray &)> done) {
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [process, done](int code, QProcess::ExitStatus status) {
        done(status == QProcess::NormalExit ? code : -1, process->readAllStandardOutput(),
             process->readAllStandardError());
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [process, program, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        done(-1, {}, tr("%1 is not installed.").arg(program).toUtf8());
        process->deleteLater();
    });
    process->start(program, arguments);
}

AudioStatus PipeWireAudio::parseStatus(const QByteArray &show, const QByteArray &defaultSource,
                                       const QByteArray &sourcesJson) {
    AudioStatus status;
    for (const auto &line : QString::fromUtf8(show).split('\n')) {
        const QString key = line.section('=', 0, 0), value = line.section('=', 1).trimmed();
        if (key == "LoadState")
            status.serviceInstalled = value == "loaded";
        else if (key == "ActiveState") {
            status.running = value == "active" || value == "activating" || value == "reloading";
            status.failed = value == "failed";
        } else if (key == "UnitFileState")
            status.enabled = value.startsWith("enabled");
    }
    status.defaultSource = QString::fromUtf8(defaultSource).trimmed();
    for (const auto &value : QJsonDocument::fromJson(sourcesJson).array()) {
        const QJsonObject source = value.toObject();
        const QString name = source.value("name").toString();
        const bool monitor = !source.value("monitor_of_sink").isNull()
                          && !source.value("monitor_of_sink").isUndefined()
                          && source.value("monitor_of_sink").toString() != "n/a";
        const QString deviceClass = source.value("properties").toObject().value("device.class").toString();
        if (name == Chain::SourceName)
            status.sourcePresent = true;
        if (name.isEmpty() || name == Chain::SourceName || monitor || deviceClass == "monitor")
            continue;
        status.microphones.append({name, source.value("description").toString(name)});
    }
    return status;
}

void PipeWireAudio::refresh() {
    if (m_refreshing)
        return;
    m_refreshing = true;
    run("systemctl", {"--user", "show", Chain::ServiceName, "-p", "LoadState", "-p", "ActiveState", "-p",
                      "UnitFileState"},
        [this](int, const QByteArray &show, const QByteArray &) {
            run("pactl", {"get-default-source"}, [this, show](int code, const QByteArray &source, const QByteArray &err) {
                if (code != 0) {
                    m_refreshing = false;
                    emit failed(QString::fromUtf8(err).trimmed());
                    return;
                }
                run("pactl", {"-f", "json", "list", "sources"},
                    [this, show, source](int, const QByteArray &sources, const QByteArray &) {
                        m_refreshing = false;
                        emit statusChanged(parseStatus(show, source, sources));
                    });
            });
        });
}

void PipeWireAudio::setRunning(bool running) {
    run("systemctl", {"--user", running ? "enable" : "disable", "--now", Chain::ServiceName},
        [this](int code, const QByteArray &, const QByteArray &err) {
            if (code != 0)
                emit failed(QString::fromUtf8(err).trimmed());
            refresh();
        });
}

void PipeWireAudio::restart() {
    run("systemctl", {"--user", "try-restart", Chain::ServiceName},
        [this](int code, const QByteArray &, const QByteArray &err) {
            if (code != 0)
                emit failed(QString::fromUtf8(err).trimmed());
            refresh();
        });
}

QStringList PipeWireAudio::applyArguments(const QString &pod) {
    return {"set-param", Chain::CaptureName, "Props", pod};
}

// Slider drags produce far more values than pw-cli calls can keep up with, so
// only one runs at a time and the newest values wait for it.
void PipeWireAudio::apply(const QString &pod) {
    m_pendingPod = pod;
    if (!m_applying)
        applyNext();
}

void PipeWireAudio::applyNext() {
    if (m_pendingPod.isEmpty()) {
        m_applying = false;
        return;
    }
    m_applying = true;
    const QString pod = std::exchange(m_pendingPod, {});
    // Fails harmlessly when Vox isn't running; the engine reads the saved
    // profile when it starts.
    run("pw-cli", applyArguments(pod), [this](int, const QByteArray &, const QByteArray &) { applyNext(); });
}

void PipeWireAudio::finish() {
    for (auto *process : findChildren<QProcess *>(Qt::FindDirectChildrenOnly))
        if (process != &m_loopback && process->program() == "pw-cli")
            process->waitForFinished(1000);
    if (!m_pendingPod.isEmpty())
        QProcess::execute("pw-cli", applyArguments(std::exchange(m_pendingPod, {})));
}

void PipeWireAudio::setDefaultSource(const QString &name) {
    run("pactl", {"set-default-source", name}, [this](int code, const QByteArray &, const QByteArray &err) {
        if (code != 0)
            emit failed(QString::fromUtf8(err).trimmed());
        refresh();
    });
}

void PipeWireAudio::setListening(bool listening) {
    if (!listening) {
        if (m_loopback.state() != QProcess::NotRunning) {
            m_loopback.terminate();
            if (!m_loopback.waitForFinished(500))
                m_loopback.kill();
        }
        return;
    }
    if (m_loopback.state() != QProcess::NotRunning)
        return;
    m_loopback.start("pw-loopback",
                     {"-C", Chain::SourceName, "-n", "vox-listen",
                      "--capture-props", "{ node.dont-fallback = true node.description = \"Vox listen\" }",
                      "--playback-props", "{ node.description = \"Vox listen\" media.role = \"Communication\" }"});
}

void PipeWireAudio::setMetering(const QString &microphone, bool voxRunning) {
    m_micMeter.start(microphone);
    if (voxRunning)
        m_voxMeter.start(Chain::SourceName);
    else
        m_voxMeter.stop();
}

void PipeWireAudio::stopMetering() {
    m_micMeter.stop();
    m_voxMeter.stop();
}

void PipeWireAudio::startCapture(const QStringList &targets) {
    for (auto *process : std::exchange(m_captures, {})) {
        process->kill();
        process->deleteLater();
    }
    m_captureData.clear();
    for (int i = 0; i < targets.size(); ++i) {
        auto *process = new QProcess(this);
        m_captures << process;
        m_captureData << QByteArray();
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process, i] {
            if (i < m_captureData.size())
                m_captureData[i] += process->readAllStandardOutput();
        });
        // Both recordings start together; Take::align removes what's left of
        // the difference, and the latency of Vox's own processing.
        process->start("pw-record", {"--target", targets[i], "--rate", QString::number(Take::Rate), "--channels",
                                     "1", "--format", "f32", "--raw",
                                     "-P", "{ node.dont-fallback = true node.dont-reconnect = true "
                                           "node.description = \"Vox test recording\" }",
                                     "-"});
    }
}

void PipeWireAudio::stopCapture() {
    QList<QByteArray> buffers;
    for (int i = 0; i < m_captures.size(); ++i) {
        QProcess *process = m_captures[i];
        process->terminate();
        if (!process->waitForFinished(1000))
            process->kill();
        m_captureData[i] += process->readAllStandardOutput();
        // pw-record writes whole samples, but a kill can cut one short.
        m_captureData[i].truncate(m_captureData[i].size() / 4 * 4);
        buffers << m_captureData[i];
        process->deleteLater();
    }
    m_captures.clear();
    m_captureData.clear();
    emit captured(buffers);
}

void PipeWireAudio::importSound(const QString &id, const QString &source, const QString &target) {
    // Written beside the target and renamed into place, so a failed or
    // half-finished conversion never leaves a broken sound behind.
    const QString partial = target + ".part";
    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process, id](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        emit soundImported(id, false, tr("ffmpeg is not installed."));
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process, id, partial, target](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            QFile::remove(partial);
            const QStringList lines = QString::fromUtf8(process->readAllStandardError()).trimmed().split('\n');
            emit soundImported(id, false, lines.isEmpty() ? QString() : lines.last().trimmed());
            return;
        }
        QFile::remove(target);
        if (!QFile::rename(partial, target)) {
            QFile::remove(partial);
            emit soundImported(id, false, tr("Couldn't save the sound."));
            return;
        }
        emit soundImported(id, true, {});
    });
    // Loudness-normalised to the starter sounds' level, so a horn found on
    // the web isn't ten times louder than the voice it plays over. loudnorm
    // emits one huge block on short input, which the FLAC encoder refuses.
    process->start("ffmpeg", {"-hide_banner", "-loglevel", "error", "-y", "-i", source, "-vn", "-ac", "1",
                              "-af", "loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000,asetnsamples=n=4096",
                              "-ar", "48000", "-c:a", "flac", "-f", "flac", partial});
}

void PipeWireAudio::playSound(const QString &id, const QString &file, double volume, bool monitor) {
    stopSound(id);
    const QString level = QString::number(std::clamp(volume, 0.0, 2.0), 'f', 3);
    Playing playing;
    playing.mix = new QProcess(this);
    // No fallback: without the mix, the sound must fail rather than play
    // out of the speakers instead of into the call.
    playing.mix->start("pw-play", {"--target", Chain::MixName, "--volume", level,
                                   "-P", "{ node.dont-fallback = true node.dont-reconnect = true "
                                         "node.description = \"Vox soundboard\" }",
                                   file});
    if (monitor) {
        playing.monitor = new QProcess(this);
        playing.monitor->start("pw-play", {"--volume", level, "-P", "{ node.description = \"Vox soundboard (you)\" }", file});
    }
    connect(playing.mix, &QProcess::finished, this, [this, id, process = playing.mix](int code, QProcess::ExitStatus) {
        process->deleteLater();
        const auto it = m_playing.find(id);
        if (it == m_playing.end() || it->mix != process)
            return;
        const bool ok = it->stopped || code == 0;
        // A sound that never reached the mix shouldn't keep playing to the
        // user alone, as if it worked.
        if (!ok && it->monitor)
            it->monitor->kill();
        m_playing.erase(it);
        emit soundFinished(id, ok);
    });
    connect(playing.mix, &QProcess::errorOccurred, this, [this, id, process = playing.mix](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        if (m_playing.value(id).mix == process) {
            if (QProcess *other = m_playing.value(id).monitor)
                other->kill();
            m_playing.remove(id);
        }
        emit failed(tr("%1 is not installed.").arg("pw-play"));
        emit soundFinished(id, false);
    });
    if (playing.monitor)
        connect(playing.monitor, &QProcess::finished, playing.monitor, &QObject::deleteLater);
    m_playing.insert(id, playing);
}

void PipeWireAudio::stopSound(const QString &id) {
    auto it = m_playing.find(id);
    if (it == m_playing.end())
        return;
    it->stopped = true;
    for (QProcess *process : {it->mix, it->monitor})
        if (process && process->state() != QProcess::NotRunning)
            process->terminate();
}

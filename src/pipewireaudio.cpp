#include "pipewireaudio.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "chain.h"

PipeWireAudio::PipeWireAudio(QObject *parent) : AudioSystem(parent) {
    auto emitLevels = [this] { emit levels(m_micMeter.level(), m_voxMeter.level()); };
    connect(&m_micMeter, &Meter::levelChanged, this, emitLevels);
    connect(&m_voxMeter, &Meter::levelChanged, this, emitLevels);
    connect(&m_loopback, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        emit listeningChanged(state != QProcess::NotRunning);
    });
}

PipeWireAudio::~PipeWireAudio() {
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

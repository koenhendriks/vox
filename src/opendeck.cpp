#include "opendeck.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include "chain.h"
#include "instance.h"

CliVoxControl::CliVoxControl(QObject *parent) : VoxControl(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, &CliVoxControl::reload);
    // Vox saves by renaming a new file over the old one, which drops a watch
    // on the file, so the directory is watched too and both re-armed.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    reload();
}

void CliVoxControl::reload() {
    const QString path = Config::defaultPath();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    if (QFileInfo::exists(QFileInfo(path).absolutePath()))
        m_watcher.addPath(QFileInfo(path).absolutePath());
    if (QFileInfo::exists(path))
        m_watcher.addPath(path);
    QString error;
    const Config config = Config::load(path, &error);
    // A file caught halfway through a save reads as broken; the next change
    // brings the real one.
    if (!error.isEmpty() || config == m_config)
        return;
    m_config = config;
    emit configChanged();
}

// The running binary, so a development build drives its own commands.
QProcess *CliVoxControl::vox(const QStringList &arguments) {
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    process->start(QCoreApplication::applicationFilePath(), arguments);
    return process;
}

void CliVoxControl::useProfile(const QString &name) {
    vox({"use", name});
}

void CliVoxControl::toggleEffects() {
    vox({"effects", "toggle"});
}

void CliVoxControl::setRunning(bool running) {
    QProcess *process = vox({running ? "on" : "off"});
    connect(process, &QProcess::finished, this, &CliVoxControl::refreshRunning);
}

void CliVoxControl::refreshRunning() {
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        emit runningChanged(QString::fromUtf8(process->readAllStandardOutput()).trimmed() == "active");
        process->deleteLater();
    });
    process->start("systemctl", {"--user", "is-active", Chain::ServiceName});
}

void CliVoxControl::playSound(const QString &name) {
    if (Instance().send("play " + name))
        return;
    // No Vox window or tray running: play it the way `vox play` does.
    QProcess *process = vox({"play", name});
    m_sounds.append(process);
    connect(process, &QProcess::finished, this, [this, process] { m_sounds.removeAll(process); });
}

void CliVoxControl::stopSounds() {
    Instance().send("stop");
    for (QProcess *process : std::as_const(m_sounds))
        process->terminate();
}

OpenDeckPlugin::OpenDeckPlugin(VoxControl *vox, QObject *parent) : QObject(parent), m_vox(vox) {
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        send({{"event", m_registerEvent}, {"uuid", m_uuid}});
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &OpenDeckPlugin::received);
    connect(&m_socket, &QWebSocket::disconnected, this, &OpenDeckPlugin::disconnected);
    connect(m_vox, &VoxControl::configChanged, this, [this] {
        updateAll();
        if (!m_inspected.isEmpty())
            sendLists(m_keys.value(m_inspected).action, m_inspected);
    });
    connect(m_vox, &VoxControl::runningChanged, this, [this](bool running) {
        if (m_runningKnown && running == m_running)
            return;
        m_running = running;
        m_runningKnown = true;
        updateAll();
    });
    // The Vox Microphone can be switched elsewhere: the window, the tray,
    // `vox off`. Checked only while a key shows it.
    m_poll.setInterval(3000);
    connect(&m_poll, &QTimer::timeout, m_vox, &VoxControl::refreshRunning);
}

void OpenDeckPlugin::connectTo(int port, const QString &registerEvent, const QString &uuid) {
    m_registerEvent = registerEvent;
    m_uuid = uuid;
    m_socket.open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));
}

void OpenDeckPlugin::send(const QJsonObject &message) {
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void OpenDeckPlugin::received(const QString &text) {
    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString event = message.value("event").toString();
    const QString context = message.value("context").toString();
    const QString action = message.value("action").toString();
    const QJsonObject payload = message.value("payload").toObject();

    if (event == "willAppear" || event == "didReceiveSettings") {
        m_keys.insert(context, {action, payload.value("settings").toObject()});
        update(context);
        if (action == PowerAction) {
            m_vox->refreshRunning();
            m_poll.start();
        }
    } else if (event == "willDisappear") {
        m_keys.remove(context);
        bool power = false;
        for (const auto &key : std::as_const(m_keys))
            power = power || key.action == PowerAction;
        if (!power)
            m_poll.stop();
    } else if (event == "propertyInspectorDidAppear") {
        m_inspected = context;
        sendLists(action, context);
    } else if (event == "propertyInspectorDidDisappear") {
        if (m_inspected == context)
            m_inspected.clear();
    } else if (event == "sendToPlugin") {
        if (payload.value("request").toString() == "lists")
            sendLists(action, context);
    } else if (event == "keyDown") {
        const QJsonObject settings = payload.value("settings").toObject();
        if (m_keys.contains(context))
            m_keys[context].settings = settings;
        if (action == ProfileAction || action == SoundAction) {
            const QString name = settings.value(action == ProfileAction ? "profile" : "sound").toString();
            // A key nobody has set up yet says so rather than doing nothing.
            if (name.isEmpty()) {
                send({{"event", "showAlert"}, {"context", context}});
                return;
            }
            if (action == ProfileAction)
                m_vox->useProfile(name);
            else
                m_vox->playSound(name);
        } else if (action == EffectsAction) {
            m_vox->toggleEffects();
        } else if (action == PowerAction) {
            m_vox->setRunning(!m_running);
        } else if (action == StopAction) {
            m_vox->stopSounds();
        }
    }
}

void OpenDeckPlugin::update(const QString &context) {
    const Key key = m_keys.value(context);
    const Config config = m_vox->config();
    auto state = [&](bool on) { send({{"event", "setState"}, {"context", context}, {"payload", QJsonObject{{"state", on ? 1 : 0}}}}); };
    auto title = [&](const QString &text) {
        send({{"event", "setTitle"}, {"context", context}, {"payload", QJsonObject{{"title", text}, {"target", 0}}}});
    };
    if (key.action == ProfileAction) {
        const QString name = key.settings.value("profile").toString();
        title(name);
        state(!name.isEmpty() && config.current().name.compare(name, Qt::CaseInsensitive) == 0);
    } else if (key.action == SoundAction) {
        title(key.settings.value("sound").toString());
    } else if (key.action == EffectsAction) {
        state(config.current().effects);
    } else if (key.action == PowerAction && m_runningKnown) {
        state(m_running);
    }
}

void OpenDeckPlugin::updateAll() {
    for (const auto &context : m_keys.keys())
        update(context);
}

void OpenDeckPlugin::sendLists(const QString &action, const QString &context) {
    const Config config = m_vox->config();
    QJsonArray profiles, sounds;
    for (const auto &profile : config.profiles)
        profiles.append(profile.name);
    for (const auto &sound : config.sounds)
        sounds.append(sound.name);
    send({{"event", "sendToPropertyInspector"},
          {"action", action},
          {"context", context},
          {"payload", QJsonObject{{"event", "lists"}, {"profiles", profiles}, {"sounds", sounds}}}});
}

#include "backend.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include "chain.h"

static constexpr int UndoLimit = 100;

Backend::Backend(AudioSystem *audio, const QString &configPath, const QStringList &pluginPath, QObject *parent)
    : QObject(parent), m_audio(audio), m_path(configPath) {
    m_missingPackages = Chain::missingPackages(pluginPath);

    const bool firstRun = !QFileInfo::exists(m_path);
    m_config = Config::load(m_path, &m_loadError);
    // Presets are named in the user's language the first time; after that
    // they're the user's profiles, named whatever they were renamed to.
    if (firstRun)
        for (auto &profile : m_config.profiles)
            profile.name = QCoreApplication::translate("Profile", profile.name.toUtf8().constData());
    m_saved = firstRun ? Config{} : m_config;

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(300);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::save);

    connect(m_audio, &AudioSystem::statusChanged, this, &Backend::setStatus);
    connect(m_audio, &AudioSystem::failed, this, &Backend::setError);
    connect(m_audio, &AudioSystem::listeningChanged, this, [this](bool listening) {
        m_listening = listening;
        emit listeningChanged();
    });
    connect(m_audio, &AudioSystem::levels, this, [this](double mic, double vox) {
        m_micLevel = mic;
        m_voxLevel = vox;
        emit levelsChanged();
    });

    // `vox use` and hand edits change the file underneath a running window.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Backend::reloadFromDisk);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Backend::reloadFromDisk);
    watch();

    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refresh);
    m_poll.start();
    refresh();
}

Backend::~Backend() {
    shutdown();
}

void Backend::shutdown() {
    save();
    if (m_bypass) {
        m_bypass = false;
        emit bypassChanged();
        applyLive();
    }
    m_audio->finish();
    m_audio->stopMetering();
    m_audio->setListening(false);
}

QStringList Backend::profileNames() const {
    QStringList names;
    for (const auto &profile : m_config.profiles)
        names << profile.name;
    return names;
}

QVariantList Backend::controls() const {
    QVariantList list;
    for (const auto &control : Profile::controls())
        list << QVariantMap{{"key", control.key}, {"min", control.min}, {"max", control.max}, {"step", control.step}};
    return list;
}

QVariantList Backend::microphones() const {
    QVariantList list;
    for (const auto &microphone : m_status.microphones)
        list << QVariantMap{{"name", microphone.name}, {"description", microphone.description}};
    return list;
}

bool Backend::microphoneConnected() const {
    for (const auto &microphone : m_status.microphones)
        if (microphone.name == m_config.microphone)
            return true;
    return false;
}

bool Backend::isDefault() const {
    return m_status.defaultSource == Chain::SourceName;
}

QString Backend::problem() const {
    if (!m_missingPackages.isEmpty())
        return tr("Vox needs %1 to process your voice. Install it with: omarchy-pkg-add %1")
            .arg(m_missingPackages.join(' '));
    if (!m_loadError.isEmpty())
        return tr("Your profiles in %1 couldn't be read (%2). Vox won't overwrite them until that's fixed.")
            .arg(m_path, m_loadError);
    if (m_ready && !m_status.serviceInstalled)
        return tr("The Vox service isn't installed, so Vox can't run in the background. Install the Vox package.");
    if (m_status.failed)
        return tr("Vox couldn't start. See what went wrong with: journalctl --user -u vox");
    if (m_ready && !m_config.microphone.isEmpty() && !microphoneConnected())
        return tr("Your microphone isn't connected. Plug it in, or pick another one.");
    return m_error;
}

void Backend::setError(const QString &message) {
    if (message == m_error)
        return;
    m_error = message;
    emit problemChanged();
}

void Backend::dismissProblem() {
    setError({});
}

void Backend::refresh() {
    m_audio->refresh();
}

void Backend::setStatus(const AudioStatus &status) {
    const bool wasRunning = m_status.running;
    const bool changedStatus = !m_ready || !(status == m_status);
    m_status = status;
    m_ready = true;

    // Until a microphone is picked, use the desktop's default one, unless
    // that is already Vox: capturing from itself would only make silence.
    if (m_config.microphone.isEmpty() && !status.microphones.isEmpty()) {
        QString pick = status.microphones.first().name;
        for (const auto &microphone : status.microphones)
            if (microphone.name == status.defaultSource)
                pick = microphone.name;
        m_config.microphone = pick;
        emit configChanged();
        m_saveTimer.start();
    }

    if (changedStatus) {
        emit statusChanged();
        emit problemChanged();
    }
    if (wasRunning != m_status.running) {
        // The engine started from the saved profile; bring compare mode along.
        if (m_status.running && m_bypass)
            applyLive();
        updateMetering();
    }
}

void Backend::changed() {
    emit configChanged();
    m_saveTimer.start();
    applyLive();
}

void Backend::applyLive() {
    m_audio->apply(Chain::pod(Chain::params(m_config.current(), m_bypass)));
}

void Backend::save() {
    m_saveTimer.stop();
    // A file that didn't parse holds the user's profiles; leave it for them.
    if (!m_loadError.isEmpty() || m_config == m_saved)
        return;
    QString error;
    if (m_config.save(m_path, &error))
        m_saved = m_config;
    else
        setError(tr("Couldn't save your profiles: %1").arg(error));
    watch();
}

void Backend::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    // Saving renames a new file over the old one, which drops the file watch,
    // so the directory is watched as well and the watch re-armed each time.
    const QString directory = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(directory))
        m_watcher.addPath(directory);
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
}

void Backend::reloadFromDisk() {
    watch();
    if (!QFileInfo::exists(m_path))
        return;
    QString error;
    const Config disk = Config::load(m_path, &error);
    if (!error.isEmpty() || disk == m_saved)
        return;
    const bool hadLoadError = !m_loadError.isEmpty();
    m_loadError.clear();
    m_saved = disk;
    if (disk == m_config) {
        if (hadLoadError)
            emit problemChanged();
        return;
    }
    const QString microphone = m_config.microphone;
    m_config = disk;
    emit configChanged();
    if (hadLoadError)
        emit problemChanged();
    applyLive();
    if (microphone != m_config.microphone && m_status.running)
        m_audio->restart();
}

void Backend::beginEdit() {
    m_undo.append(m_config);
    if (m_undo.size() > UndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    emit historyChanged();
}

void Backend::setValue(const QString &key, const QVariant &value) {
    if (m_config.current().set(key, value))
        changed();
}

void Backend::edit(const QString &key, const QVariant &value) {
    beginEdit();
    setValue(key, value);
}

void Backend::selectProfile(int index) {
    if (index < 0 || index >= m_config.profiles.size() || index == m_config.active)
        return;
    m_config.active = index;
    changed();
}

int Backend::newProfile() {
    beginEdit();
    Profile profile = m_config.current();
    profile.name = m_config.uniqueName(tr("%1 copy").arg(profile.name));
    m_config.profiles.insert(m_config.active + 1, profile);
    m_config.active += 1;
    changed();
    return m_config.active;
}

QString Backend::renameProfile(int index, const QString &name) {
    if (index < 0 || index >= m_config.profiles.size())
        return tr("That profile doesn't exist.");
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return tr("Give the profile a name.");
    bool isNumber = false;
    trimmed.toInt(&isNumber);
    // `vox use 2` picks the second profile, so a name can't be a number.
    if (isNumber)
        return tr("A profile name can't be just a number.");
    const int existing = m_config.find(trimmed);
    if (existing >= 0 && existing != index)
        return tr("There's already a profile called %1.").arg(trimmed);
    if (m_config.profiles[index].name == trimmed)
        return {};
    beginEdit();
    m_config.profiles[index].name = trimmed;
    changed();
    return {};
}

bool Backend::deleteProfile(int index) {
    if (m_config.profiles.size() <= 1 || index < 0 || index >= m_config.profiles.size())
        return false;
    beginEdit();
    m_config.profiles.removeAt(index);
    if (m_config.active > index || m_config.active >= m_config.profiles.size())
        m_config.active = std::max(0, m_config.active - 1);
    changed();
    return true;
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_config);
    m_config = m_undo.takeLast();
    emit historyChanged();
    changed();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_config);
    m_config = m_redo.takeLast();
    emit historyChanged();
    changed();
}

void Backend::setMicrophone(const QString &name) {
    if (name.isEmpty() || name == m_config.microphone)
        return;
    m_config.microphone = name;
    emit configChanged();
    emit problemChanged();
    // The engine reads the microphone when it starts, so it has to be saved
    // before the restart, not after the usual delay.
    save();
    if (m_status.running)
        m_audio->restart();
    updateMetering();
}

void Backend::setRunning(bool running) {
    save();
    m_audio->setRunning(running);
}

void Backend::makeDefault() {
    m_audio->setDefaultSource(Chain::SourceName);
}

void Backend::toggleBypass() {
    setBypass(!m_bypass);
}

void Backend::setBypass(bool bypass) {
    if (bypass == m_bypass)
        return;
    m_bypass = bypass;
    emit bypassChanged();
    applyLive();
}

void Backend::toggleListening() {
    m_audio->setListening(!m_listening);
}

void Backend::setMetering(bool on) {
    m_metering = on;
    updateMetering();
}

void Backend::updateMetering() {
    if (m_metering)
        m_audio->setMetering(m_config.microphone, m_status.running);
    else
        m_audio->stopMetering();
}

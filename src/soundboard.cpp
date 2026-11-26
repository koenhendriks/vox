#include "soundboard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <utility>

#include "audiosystem.h"
#include "backend.h"
#include "filepicker.h"

namespace {

struct Starter {
    const char *file;
    const char *name;
};

// Synthesised by tools/make-starter-sounds.py and compiled in.
const Starter Starters[] = {
    {"air-horn.flac", QT_TRANSLATE_NOOP("Sound", "Air horn")},
    {"applause.flac", QT_TRANSLATE_NOOP("Sound", "Applause")},
    {"rimshot.flac", QT_TRANSLATE_NOOP("Sound", "Rimshot")},
    {"sad-trombone.flac", QT_TRANSLATE_NOOP("Sound", "Sad trombone")},
};

}

Soundboard::Soundboard(AudioSystem *audio, Backend *backend, FilePicker *picker, const QString &directory,
                       QObject *parent)
    : QObject(parent), m_audio(audio), m_backend(backend), m_picker(picker),
      m_directory(directory.isEmpty() ? Config::defaultSoundsDirectory() : directory) {
    connect(m_audio, &AudioSystem::soundImported, this, &Soundboard::imported);
    connect(m_audio, &AudioSystem::soundFinished, this, &Soundboard::finished);
    connect(m_backend, &Backend::configChanged, this, &Soundboard::soundsChanged);
    if (m_picker) {
        connect(m_picker, &FilePicker::picked, this, [this](const QStringList &paths) {
            const QString replaces = std::exchange(m_choosingFor, {});
            if (!replaces.isEmpty()) {
                import(paths.value(0), replaces);
                return;
            }
            QVariantList files;
            for (const auto &path : paths)
                files << path;
            add(files);
        });
        connect(m_picker, &FilePicker::failed, m_backend, &Backend::reportError);
    }
    setUp();
}

void Soundboard::setUp() {
    QDir().mkpath(m_directory);
    // An unreadable config looks like one without sounds; setting up from
    // it would replace, and then clean away, the user's own.
    if (!m_backend->configReadable())
        return;

    if (!m_backend->config().soundsKnown) {
        QList<Sound> starters;
        for (const auto &starter : Starters) {
            const QString target = m_directory + "/" + starter.file;
            QFile::remove(target);
            // Copies out of the resources are read-only; the user owns these.
            if (QFile::copy(QString(":/sounds/") + starter.file, target)) {
                QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
                starters.append({QCoreApplication::translate("Sound", starter.name), starter.file});
            }
        }
        m_backend->changeConfig([&](Config &config) {
            config.sounds = starters;
            config.soundsKnown = true;
        }, false);
    }

    // Removing a sound keeps its file so undo can bring it back; a new run
    // has no undo, so files no sound uses can go.
    QSet<QString> used;
    for (const auto &sound : m_backend->config().sounds)
        used.insert(sound.file);
    for (const auto &file : QDir(m_directory).entryList({"*.flac", "*.part"}, QDir::Files))
        if (!used.contains(file))
            QFile::remove(m_directory + "/" + file);
}

QVariantList Soundboard::sounds() const {
    QVariantList list;
    const auto &sounds = m_backend->config().sounds;
    for (int i = 0; i < sounds.size(); ++i)
        list << QVariantMap{{"name", sounds[i].name}, {"file", sounds[i].file},
                            {"playing", m_playing.contains(sounds[i].file)},
                            {"copies", int(m_playing.value(sounds[i].file).size())}};
    return list;
}

double Soundboard::volume() const {
    return m_backend->config().soundVolume;
}

bool Soundboard::monitor() const {
    return m_backend->config().soundMonitor;
}

void Soundboard::play(int index) {
    const auto &sounds = m_backend->config().sounds;
    if (index < 0 || index >= sounds.size())
        return;
    const Sound &sound = sounds[index];
    if (!m_backend->running() || m_backend->sourceMissing()) {
        m_backend->reportError(tr("Turn on the Vox Microphone to play sounds into your calls."));
        return;
    }
    QStringList &copies = m_playing[sound.file];
    if (copies.size() >= MaxCopies) {
        // Forgotten before it ends, so the new copy fits at once.
        const QString oldest = copies.takeFirst();
        m_copyFile.remove(oldest);
        m_audio->stopSound(oldest);
    }
    const QString copy = QString("%1#%2").arg(sound.file).arg(++m_copies);
    copies.append(copy);
    m_copyFile.insert(copy, sound.file);
    emit soundsChanged();
    m_audio->playSound(copy, m_directory + "/" + sound.file, volume() / 100.0, monitor());
}

void Soundboard::stop(int index) {
    const auto &sounds = m_backend->config().sounds;
    if (index >= 0 && index < sounds.size())
        stopFile(sounds[index].file);
}

void Soundboard::stopFile(const QString &file) {
    for (const auto &copy : m_playing.value(file))
        m_audio->stopSound(copy);
}

void Soundboard::stopAll() {
    for (const auto &file : m_playing.keys())
        stopFile(file);
}

void Soundboard::finished(const QString &id, bool ok) {
    if (!m_copyFile.contains(id))
        return;
    const QString file = m_copyFile.take(id);
    auto it = m_playing.find(file);
    if (it != m_playing.end()) {
        it->removeAll(id);
        if (it->isEmpty())
            m_playing.erase(it);
    }
    emit soundsChanged();
    if (ok)
        return;
    QString name = file;
    for (const auto &sound : m_backend->config().sounds)
        if (sound.file == file)
            name = sound.name;
    m_backend->reportError(tr("%1 couldn't play into the Vox Microphone. Is Vox on?").arg(name));
}

QString Soundboard::nameFromFile(const QString &path) {
    QString name = QFileInfo(path).completeBaseName();
    name.replace(QRegularExpression("[-_.]+"), " ");
    name = name.simplified();
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name.isEmpty() ? tr("Sound") : name;
}

static QString localPath(const QVariant &value) {
    const QUrl url = value.toUrl();
    return url.isLocalFile() ? url.toLocalFile() : value.toString();
}

void Soundboard::add(const QVariantList &files) {
    for (const auto &value : files)
        import(localPath(value), {});
}

void Soundboard::replace(int index, const QVariant &file) {
    const auto &sounds = m_backend->config().sounds;
    if (index >= 0 && index < sounds.size())
        import(localPath(file), sounds[index].file);
}

void Soundboard::chooseReplacement(int index) {
    const auto &sounds = m_backend->config().sounds;
    if (!m_picker || index < 0 || index >= sounds.size())
        return;
    m_choosingFor = sounds[index].file;
    m_picker->openFiles(tr("Replace %1").arg(sounds[index].name), "audio/*");
}

void Soundboard::import(const QString &path, const QString &replaces) {
    if (!QFileInfo(path).isFile())
        return;
    // A fresh name for every import, so two files with the same name, or the
    // same file twice, never overwrite each other, and a replaced sound's old
    // file stays for undo.
    QString stem = QFileInfo(path).completeBaseName().toLower();
    stem.replace(QRegularExpression("[^a-z0-9]+"), "-");
    stem = stem.left(40);
    const QString file = QString("%1-%2.flac").arg(stem.isEmpty() ? "sound" : stem)
                             .arg(QRandomGenerator::global()->generate() & 0xffffff, 6, 16, QChar('0'));
    m_imports.insert(file, {nameFromFile(path), replaces});
    emit importingChanged();
    m_audio->importSound(file, path, m_directory + "/" + file);
}

void Soundboard::imported(const QString &id, bool ok, const QString &error) {
    if (!m_imports.contains(id))
        return;
    const Import pending = m_imports.take(id);
    const QString &name = pending.name;
    emit importingChanged();
    if (!ok) {
        m_backend->reportError(error.isEmpty() ? tr("Couldn't add %1 as a sound.").arg(name)
                                               : tr("Couldn't add %1 as a sound: %2").arg(name, error));
        return;
    }
    m_backend->changeConfig([&](Config &config) {
        config.soundsKnown = true;
        for (auto &sound : config.sounds) {
            if (!pending.replaces.isEmpty() && sound.file == pending.replaces) {
                stopFile(sound.file);
                sound.file = id;
                return;
            }
        }
        // Not replacing, or the sound was removed while its new file was
        // being converted: it becomes a sound of its own.
        config.sounds.append({name, id});
    });
}

void Soundboard::choose() {
    // A replacement the user cancelled must not turn this into one.
    m_choosingFor.clear();
    if (m_picker)
        m_picker->openFiles(tr("Add sounds"), "audio/*");
}

QString Soundboard::rename(int index, const QString &name) {
    const auto &sounds = m_backend->config().sounds;
    if (index < 0 || index >= sounds.size())
        return tr("That sound doesn't exist.");
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return tr("Give the sound a name.");
    bool isNumber = false;
    trimmed.toInt(&isNumber);
    // `vox play 2` plays the second sound, so a name can't be a number.
    if (isNumber)
        return tr("A sound name can't be just a number.");
    if (sounds[index].name == trimmed)
        return {};
    m_backend->changeConfig([&](Config &config) { config.sounds[index].name = trimmed; });
    return {};
}

void Soundboard::remove(int index) {
    const auto &sounds = m_backend->config().sounds;
    if (index < 0 || index >= sounds.size())
        return;
    stopFile(sounds[index].file);
    m_backend->changeConfig([&](Config &config) { config.sounds.removeAt(index); });
}

void Soundboard::setVolume(double volume) {
    m_backend->changeConfig([&](Config &config) { config.soundVolume = std::clamp(volume, 0.0, 100.0); }, false);
}

void Soundboard::setMonitor(bool monitor) {
    m_backend->changeConfig([&](Config &config) { config.soundMonitor = monitor; }, false);
}

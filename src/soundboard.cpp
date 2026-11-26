#include "soundboard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>

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
                            {"playing", m_playing.contains(sounds[i].file)}};
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
    if (m_playing.contains(sound.file)) {
        m_audio->stopSound(sound.file);
        return;
    }
    if (!m_backend->running() || m_backend->sourceMissing()) {
        m_backend->reportError(tr("Turn on the Vox Microphone to play sounds into your calls."));
        return;
    }
    m_playing.insert(sound.file);
    emit soundsChanged();
    m_audio->playSound(sound.file, m_directory + "/" + sound.file, volume() / 100.0, monitor());
}

void Soundboard::stopAll() {
    for (const auto &file : QSet<QString>(m_playing))
        m_audio->stopSound(file);
}

void Soundboard::finished(const QString &id, bool ok) {
    if (!m_playing.remove(id))
        return;
    emit soundsChanged();
    if (ok)
        return;
    QString name = id;
    for (const auto &sound : m_backend->config().sounds)
        if (sound.file == id)
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

void Soundboard::add(const QVariantList &files) {
    for (const auto &value : files) {
        const QUrl url = value.toUrl();
        const QString path = url.isLocalFile() ? url.toLocalFile() : value.toString();
        if (!QFileInfo(path).isFile())
            continue;
        // A fresh name for every import, so two files with the same name, or
        // the same file twice, never overwrite each other.
        QString stem = QFileInfo(path).completeBaseName().toLower();
        stem.replace(QRegularExpression("[^a-z0-9]+"), "-");
        stem = stem.left(40);
        const QString file = QString("%1-%2.flac").arg(stem.isEmpty() ? "sound" : stem)
                                 .arg(QRandomGenerator::global()->generate() & 0xffffff, 6, 16, QChar('0'));
        m_imports.insert(file, nameFromFile(path));
        emit importingChanged();
        m_audio->importSound(file, path, m_directory + "/" + file);
    }
}

void Soundboard::imported(const QString &id, bool ok, const QString &error) {
    const QString name = m_imports.take(id);
    emit importingChanged();
    if (name.isEmpty())
        return;
    if (!ok) {
        m_backend->reportError(error.isEmpty() ? tr("Couldn't add %1 as a sound.").arg(name)
                                               : tr("Couldn't add %1 as a sound: %2").arg(name, error));
        return;
    }
    m_backend->changeConfig([&](Config &config) {
        config.sounds.append({name, id});
        config.soundsKnown = true;
    });
}

void Soundboard::choose() {
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
    if (m_playing.contains(sounds[index].file))
        m_audio->stopSound(sounds[index].file);
    m_backend->changeConfig([&](Config &config) { config.sounds.removeAt(index); });
}

void Soundboard::setVolume(double volume) {
    m_backend->changeConfig([&](Config &config) { config.soundVolume = std::clamp(volume, 0.0, 100.0); }, false);
}

void Soundboard::setMonitor(bool monitor) {
    m_backend->changeConfig([&](Config &config) { config.soundMonitor = monitor; }, false);
}

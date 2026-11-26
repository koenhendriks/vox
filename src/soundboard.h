#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>

class AudioSystem;
class Backend;
class FilePicker;

// Sounds the user plays into the Vox Microphone, after the voice processing,
// so a call hears the horn as a horn. Imported sounds are converted into
// Vox's own sounds directory; the user's files are never touched.
class Soundboard : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sounds READ sounds NOTIFY soundsChanged)
    Q_PROPERTY(double volume READ volume NOTIFY soundsChanged)
    Q_PROPERTY(bool monitor READ monitor NOTIFY soundsChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY soundsChanged)
    Q_PROPERTY(int importing READ importing NOTIFY importingChanged)

public:
    Soundboard(AudioSystem *audio, Backend *backend, FilePicker *picker, const QString &directory = {},
               QObject *parent = nullptr);

    QVariantList sounds() const;
    double volume() const;
    bool monitor() const;
    bool playing() const { return !m_playing.isEmpty(); }
    int importing() const { return int(m_imports.size()); }
    QString directory() const { return m_directory; }

    // Plays a sound, or stops it when it is already playing.
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void stopAll();
    // File paths or file:// URLs, as a drop or the file chooser gives them.
    Q_INVOKABLE void add(const QVariantList &files);
    Q_INVOKABLE void choose();
    // Gives a sound new audio, keeping its name, place and shortcut.
    Q_INVOKABLE void replace(int index, const QVariant &file);
    Q_INVOKABLE void chooseReplacement(int index);
    // Returns an empty string on success, or why the name can't be used.
    Q_INVOKABLE QString rename(int index, const QString &name);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setMonitor(bool monitor);

    static QString nameFromFile(const QString &path);

signals:
    void soundsChanged();
    void importingChanged();

private:
    void setUp();
    void imported(const QString &id, bool ok, const QString &error);
    void import(const QString &path, const QString &replaces);
    void finished(const QString &id, bool ok);

    AudioSystem *m_audio;
    Backend *m_backend;
    FilePicker *m_picker;
    QString m_directory;
    struct Import {
        QString name;
        QString replaces;  // the file of the sound it replaces, if any
    };
    QSet<QString> m_playing;           // by file
    QHash<QString, Import> m_imports;  // by the file being written
    // What the open file chooser is for: a sound's file to replace, or
    // empty to add new sounds.
    QString m_choosingFor;
};

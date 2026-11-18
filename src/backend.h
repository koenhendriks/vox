#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "audiosystem.h"
#include "config.h"

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY configChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY configChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY configChanged)
    Q_PROPERTY(QVariantList controls READ controls CONSTANT)
    Q_PROPERTY(QString microphone READ microphone NOTIFY configChanged)
    Q_PROPERTY(QVariantList microphones READ microphones NOTIFY statusChanged)
    Q_PROPERTY(bool microphoneConnected READ microphoneConnected NOTIFY statusChanged)
    Q_PROPERTY(bool running READ running NOTIFY statusChanged)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY problemChanged)
    Q_PROPERTY(bool bypass READ bypass NOTIFY bypassChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(double micLevel READ micLevel NOTIFY levelsChanged)
    Q_PROPERTY(double voxLevel READ voxLevel NOTIFY levelsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    // `pluginPath` overrides where LADSPA plugins are looked for, for tests.
    explicit Backend(AudioSystem *audio, const QString &configPath = Config::defaultPath(),
                     const QStringList &pluginPath = {}, QObject *parent = nullptr);
    ~Backend() override;

    QStringList profileNames() const;
    int activeIndex() const { return m_config.active; }
    QVariantMap current() const { return m_config.current().toMap(); }
    QVariantList controls() const;
    QString microphone() const { return m_config.microphone; }
    QVariantList microphones() const;
    bool microphoneConnected() const;
    bool running() const { return m_status.running; }
    bool isDefault() const;
    bool ready() const { return m_ready; }
    QString problem() const;
    bool bypass() const { return m_bypass; }
    bool listening() const { return m_listening; }
    double micLevel() const { return m_micLevel; }
    double voxLevel() const { return m_voxLevel; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    const Config &config() const { return m_config; }

    // Marks the start of one undoable change, such as pressing a slider.
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void edit(const QString &key, const QVariant &value);
    Q_INVOKABLE void selectProfile(int index);
    Q_INVOKABLE int newProfile();
    // Returns an empty string on success, or why the name can't be used.
    Q_INVOKABLE QString renameProfile(int index, const QString &name);
    Q_INVOKABLE bool deleteProfile(int index);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE void setMicrophone(const QString &name);
    Q_INVOKABLE void setRunning(bool running);
    Q_INVOKABLE void makeDefault();
    Q_INVOKABLE void toggleBypass();
    void setBypass(bool bypass);
    Q_INVOKABLE void toggleListening();
    Q_INVOKABLE void setMetering(bool on);
    Q_INVOKABLE void dismissProblem();
    Q_INVOKABLE void refresh();
    // With no window open, the status only feeds the tray and is checked less
    // often; the tray menu refreshes it before it opens.
    Q_INVOKABLE void setForeground(bool foreground);

    // Saves and takes the graph out of compare mode, before the app quits.
    void shutdown();

signals:
    void configChanged();
    void statusChanged();
    void problemChanged();
    void bypassChanged();
    void listeningChanged();
    void levelsChanged();
    void historyChanged();

private:
    void changed();
    void applyLive();
    void save();
    void reloadFromDisk();
    void watch();
    void updateMetering();
    void setStatus(const AudioStatus &status);
    void setError(const QString &message);

    AudioSystem *m_audio;
    QString m_path;
    QStringList m_missingPackages;
    Config m_config, m_saved;
    QList<Config> m_undo, m_redo;
    AudioStatus m_status;
    bool m_ready = false;
    QString m_loadError, m_error;
    bool m_bypass = false, m_listening = false, m_metering = false;
    double m_micLevel = -90, m_voxLevel = -90;
    QTimer m_saveTimer, m_poll;
    QFileSystemWatcher m_watcher;
};

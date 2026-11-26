#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QWebSocket>

#include "config.h"

// What a Stream Deck key asks of Vox, behind an interface so tests can drive
// the plugin without a running Vox.
class VoxControl : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    virtual Config config() const = 0;
    virtual void useProfile(const QString &name) = 0;
    virtual void toggleEffects() = 0;
    virtual void setRunning(bool running) = 0;
    // Answers with runningChanged().
    virtual void refreshRunning() = 0;
    virtual void playSound(const QString &name) = 0;
    virtual void stopSounds() = 0;

signals:
    void configChanged();
    void runningChanged(bool running);
};

// The real one: vox's own commands, and the running Vox for sounds, so a key
// plays them as if clicked in the window.
class CliVoxControl : public VoxControl {
    Q_OBJECT

public:
    explicit CliVoxControl(QObject *parent = nullptr);
    Config config() const override { return m_config; }
    void useProfile(const QString &name) override;
    void toggleEffects() override;
    void setRunning(bool running) override;
    void refreshRunning() override;
    void playSound(const QString &name) override;
    void stopSounds() override;

private:
    void reload();
    QProcess *vox(const QStringList &arguments);

    Config m_config;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QList<QProcess *> m_sounds;  // played without a running Vox
};

// An OpenDeck (Stream Deck SDK 2) plugin: connects to OpenDeck, runs the
// keys' actions and keeps each key's title and lit state in step with Vox.
class OpenDeckPlugin : public QObject {
    Q_OBJECT

public:
    static constexpr auto Uuid = "com.koenhendriks.vox";
    static constexpr auto ProfileAction = "com.koenhendriks.vox.profile";
    static constexpr auto SoundAction = "com.koenhendriks.vox.sound";
    static constexpr auto EffectsAction = "com.koenhendriks.vox.effects";
    static constexpr auto PowerAction = "com.koenhendriks.vox.power";
    static constexpr auto StopAction = "com.koenhendriks.vox.stop";

    explicit OpenDeckPlugin(VoxControl *vox, QObject *parent = nullptr);
    void connectTo(int port, const QString &registerEvent, const QString &uuid);

signals:
    // OpenDeck closed the connection, as it does when it quits.
    void disconnected();

private:
    struct Key {
        QString action;
        QJsonObject settings;
    };

    void received(const QString &message);
    void send(const QJsonObject &message);
    void update(const QString &context);
    void updateAll();
    void sendLists(const QString &action, const QString &context);

    VoxControl *m_vox;
    QWebSocket m_socket;
    QString m_registerEvent, m_uuid;
    QHash<QString, Key> m_keys;  // by context
    QString m_inspected;         // the key whose settings page is open
    bool m_running = false;
    // Until systemd has answered, the Vox Microphone key shows no state,
    // rather than flashing off before it turns on.
    bool m_runningKnown = false;
    QTimer m_poll;
};

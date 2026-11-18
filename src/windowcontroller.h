#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class QDBusServiceWatcher;
class QQmlApplicationEngine;
class QQuickWindow;

// Owns the window's life beside the tray icon: the window is loaded the first
// time it's asked for, hides instead of quitting while there's a tray, and
// the app quits when the last way to reach it goes away.
class WindowController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable READ trayAvailable WRITE setTrayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    explicit WindowController(QQmlApplicationEngine *engine = nullptr, QObject *parent = nullptr);

    // Set after construction: the controller must outlive the engine, whose
    // bindings read it while the engine tears down.
    void setEngine(QQmlApplicationEngine *engine) { m_engine = engine; }

    bool trayAvailable() const { return m_trayAvailable; }
    void setTrayAvailable(bool available);
    bool windowVisible() const;

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void toggleWindow();
    Q_INVOKABLE void quit();

    // Creates the tray icon once a tray host is up, and again after it
    // restarts. Qt checks for a host once per process and remembers the
    // answer, so the icon must not exist before the host does: at login Vox
    // can start ahead of the shell that hosts the tray.
    void watchTray();
    void destroyTray();
    static bool trayHostRegistered();

signals:
    void trayAvailableChanged();
    void windowVisibleChanged();

private:
    QQmlApplicationEngine *m_engine;
    void createTray();

    QPointer<QQuickWindow> m_window;
    QPointer<QObject> m_tray;
    QDBusServiceWatcher *m_trayWatcher = nullptr;
    QTimer m_hostPoll;
    bool m_trayAvailable = false;
};

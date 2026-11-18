#pragma once

#include <QObject>
#include <QPointer>

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

signals:
    void trayAvailableChanged();
    void windowVisibleChanged();

private:
    QQmlApplicationEngine *m_engine;
    QPointer<QQuickWindow> m_window;
    bool m_trayAvailable = false;
};

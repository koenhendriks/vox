#include "windowcontroller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

WindowController::WindowController(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine) {}

void WindowController::setTrayAvailable(bool available) {
    if (available == m_trayAvailable)
        return;
    m_trayAvailable = available;
    emit trayAvailableChanged();
    // Without a tray, a hidden window would leave Vox running with no way back.
    if (!available && !windowVisible() && m_window)
        quit();
}

bool WindowController::windowVisible() const {
    return m_window && m_window->isVisible();
}

void WindowController::showWindow() {
    if (!m_window && m_engine) {
        m_engine->load(QUrl("qrc:/Main.qml"));
        for (QObject *root : m_engine->rootObjects())
            if (auto *window = qobject_cast<QQuickWindow *>(root))
                m_window = window;
        if (!m_window)
            return;
        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            emit windowVisibleChanged();
            if (!visible && !m_trayAvailable)
                quit();
        });
    }
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    emit windowVisibleChanged();
}

void WindowController::hideWindow() {
    if (!m_window)
        return;
    if (!m_trayAvailable) {
        quit();
        return;
    }
    m_window->hide();
}

void WindowController::toggleWindow() {
    if (windowVisible() && m_window->isActive())
        hideWindow();
    else
        showWindow();
}

void WindowController::quit() {
    // Not quit(): in Qt 6 that first asks every window to close, and the
    // window refuses so that closing it only hides it to the tray.
    QCoreApplication::exit(0);
}

#include "windowcontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>

static const QString WatcherService = "org.kde.StatusNotifierWatcher";

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

// The same question Qt asks before it will make a tray icon.
bool WindowController::trayHostRegistered() {
    QDBusMessage message = QDBusMessage::createMethodCall(WatcherService, "/StatusNotifierWatcher",
                                                          "org.freedesktop.DBus.Properties", "Get");
    message << WatcherService << "IsStatusNotifierHostRegistered";
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 1000);
    return reply.type() == QDBusMessage::ReplyMessage
        && reply.arguments().value(0).value<QDBusVariant>().variant().toBool();
}

void WindowController::watchTray() {
    if (m_trayWatcher)
        return;
    m_trayWatcher = new QDBusServiceWatcher(WatcherService, QDBusConnection::sessionBus(),
                                            QDBusServiceWatcher::WatchForRegistration
                                                | QDBusServiceWatcher::WatchForUnregistration,
                                            this);
    // A host registers itself a moment after its watcher appears, so a new
    // watcher starts the poll rather than the icon.
    connect(m_trayWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { m_hostPoll.start(); });
    // A restarted shell doesn't know the old icon; it registers again with
    // the new one. Vox keeps running meanwhile, as if the tray were there.
    connect(m_trayWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        destroyTray();
        m_hostPoll.start();
    });
    m_hostPoll.setInterval(1000);
    connect(&m_hostPoll, &QTimer::timeout, this, [this] {
        if (!m_tray && trayHostRegistered())
            createTray();
        if (m_tray)
            m_hostPoll.stop();
    });
    if (trayHostRegistered())
        createTray();
    else
        m_hostPoll.start();
}

void WindowController::createTray() {
    if (m_tray || !m_engine)
        return;
    QQmlComponent component(m_engine, QUrl("qrc:/Tray.qml"));
    QObject *tray = component.create(m_engine->rootContext());
    if (!tray) {
        qWarning("vox: couldn't create the tray icon: %s", qPrintable(component.errorString()));
        return;
    }
    m_tray = tray;
}

void WindowController::destroyTray() {
    delete m_tray;
}

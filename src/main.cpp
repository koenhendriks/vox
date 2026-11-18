// Vox makes your microphone sound better, in every app, through a virtual
// microphone that PipeWire runs your voice through.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "instance.h"
#include "pipewireaudio.h"
#include "testrecorder.h"
#include "theme.h"
#include "translator.h"
#include "trayicon.h"
#include "windowcontroller.h"

#include <cstdio>

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);
    const bool window = Cli::wantsWindow(arguments);
    // The commands run from scripts, agents and systemd, where there may be
    // no display to connect to.
    if (!window)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("vox");
    app.setApplicationVersion(VOX_VERSION);

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    if (!window)
        return Cli::run(arguments);

    // Matches the window to vox.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("vox");
    app.setWindowIcon(QIcon::fromTheme("vox"));
    // The tray icon keeps Vox running with no window open.
    app.setQuitOnLastWindowClosed(false);

    const bool trayOnly = arguments.value(1) == "tray";
    Instance instance;
    if (instance.send(trayOnly ? "tray" : "open"))
        return 0;
    if (!instance.listen())
        qWarning("vox: couldn't listen at %s, so a second Vox may start", qPrintable(Instance::defaultSocketPath()));

    QQuickStyle::setStyle("Material");

    Theme theme;
    PipeWireAudio audio;
    Backend backend(&audio);
    TestRecorder tester(&audio, &backend);
    TrayIcons trayIcons;
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::shutdown);

    WindowController shell;
    QQmlApplicationEngine engine;
    shell.setEngine(&engine);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("tester", &tester);
    engine.rootContext()->setContextProperty("trayIcons", &trayIcons);
    engine.rootContext()->setContextProperty("shell", &shell);
    QObject::connect(&instance, &Instance::command, &shell, [&shell](const QString &command) {
        if (command == "open")
            shell.showWindow();
    });

    engine.load(QUrl("qrc:/Tray.qml"));
    if (trayOnly && !shell.trayAvailable()) {
        std::fputs("vox: there's no system tray to show Vox in.\n", stderr);
        return 1;
    }
    if (!trayOnly) {
        shell.showWindow();
        if (!shell.windowVisible())
            return 1;
    }

    return app.exec();
}

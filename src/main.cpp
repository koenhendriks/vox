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
#include "pipewireaudio.h"
#include "testrecorder.h"
#include "theme.h"
#include "translator.h"

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
    app.setApplicationVersion("0.2.0");

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

    QQuickStyle::setStyle("Material");

    Theme theme;
    PipeWireAudio audio;
    Backend backend(&audio);
    TestRecorder tester(&audio, &backend);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &backend, &Backend::shutdown);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("tester", &tester);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

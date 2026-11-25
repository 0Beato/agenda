// agenda — appointments on a calendar, with reminders as desktop notifications.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    // Commands run from the reminder timer and from scripts, where there is
    // no display to open.
    const bool gui = isGuiCommand(arguments);
    if (!gui)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("agenda");
    app.setApplicationDisplayName("Agenda");
    app.setApplicationVersion("0.1.0");

    // Matches the window to agenda.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("agenda");
    app.setWindowIcon(QIcon::fromTheme("agenda"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes. Notifications are
    // translated too.
    Translator translator;
    app.installTranslator(&translator);

    if (!gui)
        return runCli(arguments);

    QQuickStyle::setStyle("Material");

    Theme theme;
    Backend backend;
    backend.ensureReminders();

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::refresh);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

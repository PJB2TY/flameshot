// Copyright 2017 Alejandro Sirgo Rica
//
// This file is part of Flameshot.
//
//     Flameshot is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Flameshot is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Flameshot.  If not, see <http://www.gnu.org/licenses/>.

#include "src/core/controller.h"
#include "singleapplication.h"
#include "src/core/flameshotdbusadapter.h"
#include <QApplication>
#include <QTranslator>
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QCommandLineParser>
#include <QDir>

int main(int argc, char *argv[]) {
    qApp->setApplicationVersion(static_cast<QString>(APP_VERSION));

    QTranslator translator;
    translator.load(QLocale::system().language(),
      "Internationalization", "_", "/usr/share/flameshot/translations/");

    // no arguments, just launch Flameshot
    if (argc == 1) {
        SingleApplication app(argc, argv);
        app.installTranslator(&translator);
        app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
        app.setApplicationName("flameshot");
        app.setOrganizationName("Dharkael");

        auto c = Controller::getInstance();
        new FlameshotDBusAdapter(c);
        QDBusConnection dbus = QDBusConnection::sessionBus();
        dbus.registerObject("/", c);
        dbus.registerService("org.dharkael.Flameshot");
        return app.exec();
    }

    QCoreApplication app(argc, argv);
    app.installTranslator(&translator);
    app.setApplicationName("flameshot");
    app.setOrganizationName("Dharkael");

    // CLI parsing
    QCommandLineParser parser;
    QCommandLineOption versionOption(QStringList() << "v" << "version",
                                     "Show version information");

    parser.addOption(versionOption);
    parser.addHelpOption();
    parser.setApplicationDescription(
                "Powerfull yet simple to use screenshot software.");

    QString fullDescription = "Capture the entire desktop.";
    QString guiDescription = "Start a manual capture in GUI mode.";
    parser.addPositionalArgument("mode", "full\t"+fullDescription+"\n"
                                         "gui\t"+guiDescription,
                                 "mode [mode_options]");
    parser.parse(app.arguments());

    // show app version
    if (parser.isSet("version")) {
        qInfo().noquote() << "Flameshot" << qApp->applicationVersion()
                          << "\nCompiled with Qt" << QT_VERSION_STR;
        return 0;
    }
    const QStringList args = parser.positionalArguments();
    const QString command = args.isEmpty() ? QString() : args.first();

    QCommandLineOption pathOption(QStringList() << "p" << "path",
                                  "Path where the capture will be saved", "");
    QCommandLineOption clipboardOption({{"c", "clipboard"},
                                        "Save the capture to the clipboard"});
    QCommandLineOption delayOption(QStringList() << "d" << "delay",
                                  "Delay time in milliseconds", "0");
    // parse commands
    if (command == "full") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument(
                    "full", fullDescription, "full [full_options]");
        parser.addOptions({ pathOption, clipboardOption, delayOption });
    } else if (command == "gui") {
        parser.clearPositionalArguments();
        parser.addPositionalArgument(
                    "gui", guiDescription, "gui [gui_options]");

        parser.addOptions({ pathOption, delayOption });
    }
    parser.process(app);

    // obtain values
    QDBusMessage m;
    QString pathValue;
    if (parser.isSet("path")) {
        pathValue = QString::fromStdString(parser.value("path").toStdString());
        if (!QDir(pathValue).exists()) {
            qWarning().noquote() << QObject::tr("Invalid path.");
            return 0;
        }
    }
    int delay = 0;
    if (parser.isSet("delay")) {
        delay = parser.value("delay").toInt();
        if (delay < 0) {
            qWarning().noquote() << QObject::tr("Invalid negative delay.");
            return 0;
        }
    }

    // process commands
    if (command == "gui") {
        m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                           "/", "", "graphicCapture");
        m << pathValue << delay;
        QDBusConnection::sessionBus().call(m);

    } else if (command == "full") {
        m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                           "/", "", "fullScreen");
        m << pathValue << parser.isSet("clipboard") << delay;
        QDBusConnection::sessionBus().call(m);
    }
    return 0;
}
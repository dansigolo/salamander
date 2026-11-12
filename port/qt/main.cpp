// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "appearance.h"
#include <QApplication>
#include <QDir>
#include <QSettings>
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Open Salamander Linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("OpenSalamanderLinux"));
    const auto arguments = app.arguments();
    QSettings settings;
    Appearance::apply(settings);
    MainWindow window(arguments.value(1, QDir::homePath()), arguments.value(2, QDir::homePath()), true, &settings);
    window.show();
    return app.exec();
}

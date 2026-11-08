// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QMainWindow>
#include <QMenuBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeView>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Open Salamander Qt Prototype"));
    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Open Salamander — Qt port prototype"));
    auto* splitter = new QSplitter(&window);
    const auto arguments = app.arguments();
    auto* left = new FilePanel(arguments.value(1, QDir::homePath()), splitter);
    auto* right = new FilePanel(arguments.value(2, QDir::homePath()), splitter);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setSizes({550, 550});
    window.setCentralWidget(splitter);
    auto* file = window.menuBar()->addMenu(QObject::tr("&File"));
    auto* quit = file->addAction(QObject::tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);
    QObject::connect(quit, &QAction::triggered, &app, &QApplication::quit);
    auto* view = window.menuBar()->addMenu(QObject::tr("&View"));
    auto* hidden = view->addAction(QObject::tr("Show &hidden files"));
    hidden->setCheckable(true);
    hidden->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    QObject::connect(hidden, &QAction::toggled, left, &FilePanel::setHiddenFilesVisible);
    QObject::connect(hidden, &QAction::toggled, right, &FilePanel::setHiddenFilesVisible);
    for (auto* panel : {left, right})
    {
        auto* other = panel == left ? right : left;
        auto* swap = new QShortcut(QKeySequence(Qt::Key_Tab), panel->view());
        swap->setContext(Qt::WidgetShortcut);
        QObject::connect(swap, &QShortcut::activated, other->view(), [other]
                         { other->view()->setFocus(); });
        auto* up = new QShortcut(QKeySequence(QStringLiteral("Alt+Up")), panel);
        up->setContext(Qt::WidgetWithChildrenShortcut);
        QObject::connect(up, &QShortcut::activated, panel, &FilePanel::goUp);
    }
    window.statusBar()->showMessage(QObject::tr("Tab: switch panels | Alt+Up: parent | Ctrl+H: hidden files | Enter: open directory"));
    window.resize(1200, 700);
    window.show();
    left->view()->setFocus();
    return app.exec();
}

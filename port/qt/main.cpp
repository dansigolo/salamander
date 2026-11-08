// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include "fileview.h"
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QMainWindow>
#include <QMenuBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeView>
#include <QHeaderView>
#include <utility>

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
        const std::pair<int, int> sorting[] = {{Qt::Key_F3, 0}, {Qt::Key_F4, 2}, {Qt::Key_F5, 3}, {Qt::Key_F6, 1}};
        for (const auto& [key, column] : sorting)
        {
            auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), panel);
            shortcut->setContext(Qt::WidgetWithChildrenShortcut);
            QObject::connect(shortcut, &QShortcut::activated, panel, [panel, column]
                             {
                auto* header = panel->view()->header();
                const auto order = header->sortIndicatorSection() == column && header->sortIndicatorOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
                panel->view()->sortByColumn(column, order); });
        }
    }
    window.statusBar()->showMessage(QObject::tr("Tab: panels | Insert/Space: mark | Backspace: parent | Type: quick search | Esc: end search"));
    window.resize(1200, 700);
    window.show();
    left->view()->setFocus();
    return app.exec();
}

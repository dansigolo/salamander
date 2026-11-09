// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>
class WindowTests : public QObject
{
    Q_OBJECT
private slots:
    void activePanelActionsAndNavigation()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        QVERIFY(root.mkpath("right/child"));
        QVERIFY(root.mkpath("left/child"));
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        window.show();
        window.activateWindow();
        window.leftPanel()->view()->setFocus();
        QTRY_VERIFY(window.leftPanel()->view()->hasFocus());
        QTRY_COMPARE(window.activePanel(), window.leftPanel());
        QTest::keyClick(window.leftPanel()->view(), Qt::Key_Tab);
        QTRY_COMPARE(window.activePanel(), window.rightPanel());
        QVERIFY(window.rightPanel()->navigate("child"));
        auto* back = window.findChild<QAction*>("navigateBack");
        QVERIFY(back->isEnabled());
        back->trigger();
        QCOMPARE(window.rightPanel()->currentPath(), root.filePath("right"));
        QCOMPARE(window.leftPanel()->currentPath(), root.filePath("left"));
        window.findChild<QAction*>("navigateForward")->trigger();
        QCOMPARE(window.rightPanel()->currentPath(), root.filePath("right/child"));
        auto* sort = window.findChild<QAction*>("sortExtension");
        sort->trigger();
        QCOMPARE(window.rightPanel()->view()->header()->sortIndicatorSection(), int(DetailedFileModel::Extension));
        QCOMPARE(window.leftPanel()->view()->header()->sortIndicatorSection(), int(DetailedFileModel::Name));
        QTest::keyClick(window.rightPanel()->view(), Qt::Key_F6, Qt::ControlModifier);
        QCOMPARE(window.rightPanel()->view()->header()->sortIndicatorSection(), int(DetailedFileModel::Size));
        QTest::keyClick(window.rightPanel()->view(), Qt::Key_L, Qt::ControlModifier);
        auto* address = window.findChild<QLineEdit*>("goToDirectory");
        QTRY_VERIFY(address->hasFocus());
        address->setText(root.filePath("right"));
        QTest::keyClick(address, Qt::Key_Return);
        QCOMPARE(window.rightPanel()->currentPath(), root.filePath("right"));
        QCOMPARE(window.activePanel(), window.rightPanel());
        QVERIFY(window.leftPanel()->navigate(root.filePath("left/child")));
        QToolButton* panelBack = nullptr;
        for (auto* button : window.leftPanel()->findChildren<QToolButton*>())
            if (button->defaultAction() && button->defaultAction()->text() == "Back")
                panelBack = button;
        QVERIFY(panelBack);
        QTest::mouseClick(panelBack, Qt::LeftButton);
        QCOMPARE(window.activePanel(), window.leftPanel());
        QCOMPARE(window.leftPanel()->currentPath(), root.filePath("left"));
        window.findChild<QAction*>("addFavorite")->trigger();
        QVERIFY(window.leftPanel()->navigate(root.path()));
        QAction* favorite = nullptr;
        for (auto* action : window.findChild<QToolBar*>("placesToolbar")->actions())
            if (action->data().toString() == root.filePath("left"))
                favorite = action;
        QVERIFY(favorite);
        favorite->trigger();
        QCOMPARE(window.leftPanel()->currentPath(), root.filePath("left"));
    }
    void menuStructureAndUnavailableOperations()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MainWindow window(temporary.path(), temporary.path(), false);
        QCOMPARE(window.menuBar()->actions().size(), 8);
        const QStringList names = {"&Left", "&Files", "&Edit", "&Commands", "&Plugins", "&Options", "&Right", "&Help"};
        for (int i = 0; i < names.size(); ++i)
            QCOMPARE(window.menuBar()->actions()[i]->text(), names[i]);
        for (const auto* name : {"topToolbar", "placesToolbar", "functionToolbar"})
            QVERIFY(window.findChild<QToolBar*>(name));
        for (const auto* name : {"renameFiles", "copyFiles", "moveFiles", "makeDirectory", "deleteFiles", "userMenu", "connectNetwork", "disconnectNetwork"})
        {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY(action);
            QVERIFY(!action->isEnabled());
        }
    }
    void screenshotFixture()
    {
        const QString path = qEnvironmentVariable("SALAMANDER_TEST_SCREENSHOT");
        if (path.isEmpty())
            QSKIP("Screenshot fixture is opt-in");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkpath("projects/salamander"));
        QVERIFY(root.mkdir("downloads"));
        for (const auto& name : {"README.md", "archive.tar.gz", "notes.txt", ".hidden"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("Salamander Linux interface fixture\n");
        }
        MainWindow window(root.path(), root.filePath("projects"));
        window.show();
        QTRY_VERIFY(window.leftPanel()->view()->model()->rowCount(window.leftPanel()->view()->rootIndex()) >= 6);
        const auto marked = window.leftPanel()->visibleIndex(root.filePath("notes.txt"));
        window.leftPanel()->view()->selectionModel()->select(marked, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::qWait(150);
        QVERIFY(window.grab().save(path));
    }
};
QTEST_MAIN(WindowTests)
#include "window_tests.moc"

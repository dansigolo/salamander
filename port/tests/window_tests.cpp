// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
#include "namedialog.h"
#include "operationcontroller.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QPointer>
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QSignalSpy>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QTest>
#include <QSettings>
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
        for (const auto* name : {"copyFiles", "moveFiles", "deleteFiles", "userMenu", "connectNetwork", "disconnectNetwork"})
        {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY(action);
            QVERIFY(!action->isEnabled());
        }
    }
    void selectionPreferenceAppliesToBothPanelsAndPersists()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString config = temporary.filePath("preferences.ini");
        {
            QSettings settings(config, QSettings::IniFormat);
            MainWindow window(temporary.path(), temporary.path(), false, &settings);
            auto* option = window.findChild<QAction*>("preserveSelectionOnNavigation");
            QVERIFY(option);
            QVERIFY(!option->isChecked());
            QVERIFY(!window.leftPanel()->preservesSelectionOnNavigation());
            QVERIFY(!window.rightPanel()->preservesSelectionOnNavigation());
            option->setChecked(true);
            QVERIFY(window.leftPanel()->preservesSelectionOnNavigation());
            QVERIFY(window.rightPanel()->preservesSelectionOnNavigation());
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }
        {
            QSettings settings(config, QSettings::IniFormat);
            MainWindow window(temporary.path(), temporary.path(), false, &settings);
            QVERIFY(window.findChild<QAction*>("preserveSelectionOnNavigation")->isChecked());
            QVERIFY(window.leftPanel()->preservesSelectionOnNavigation());
            QVERIFY(window.rightPanel()->preservesSelectionOnNavigation());
            window.findChild<QAction*>("preserveSelectionOnNavigation")->setChecked(false);
            settings.sync();
        }
        QSettings settings(config, QSettings::IniFormat);
        MainWindow window(temporary.path(), temporary.path(), false, &settings);
        QVERIFY(!window.findChild<QAction*>("preserveSelectionOnNavigation")->isChecked());
        QVERIFY(!window.leftPanel()->preservesSelectionOnNavigation());
        QVERIFY(!window.rightPanel()->preservesSelectionOnNavigation());
    }

    void createDirectoryWithF7AndRenameWithF2()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        window.show();
        window.activateWindow();
        auto* panel = window.leftPanel();
        panel->view()->setFocus();
        QTRY_VERIFY(panel->view()->hasFocus());
        QTest::keyClick(panel->view(), Qt::Key_F7);
        QPointer<NameDialog> dialog = window.findChild<NameDialog*>("operationNameDialog");
        QVERIFY(dialog);
        QVERIFY(!window.findChild<QAction*>("makeDirectory")->isEnabled());
        auto* input = dialog->findChild<QLineEdit*>("operationName");
        input->setText("bad/nested");
        dialog->accept();
        QVERIFY(dialog->isVisible());
        QVERIFY(!root.exists("left/bad"));
        input->setText(QString::fromUtf8("nova pasta á"));
        dialog->accept();
        QVERIFY(!window.close()); // A submitted atomic job must finish before closing.
        QTRY_VERIFY(QFileInfo(root.filePath(QString::fromUtf8("left/nova pasta á"))).isDir());
        QTRY_COMPARE(panel->view()->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString::fromUtf8("nova pasta á"));
        QTRY_VERIFY(window.findChild<QAction*>("renameFiles")->isEnabled());
        QTRY_VERIFY(!dialog);
        window.activateWindow();
        panel->view()->setFocus();
        QTRY_VERIFY(panel->view()->hasFocus());
        QTest::keyClick(panel->view(), Qt::Key_F2);
        QTRY_VERIFY(window.findChild<NameDialog*>("operationNameDialog"));
        auto* rename = window.findChild<NameDialog*>("operationNameDialog");
        rename->findChild<QLineEdit*>("operationName")->setText("renamed folder");
        rename->accept();
        QTRY_VERIFY(root.exists("left/renamed folder"));
        QTRY_COMPARE(panel->view()->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("renamed folder"));
        QVERIFY(!root.exists(QString::fromUtf8("left/nova pasta á")));
        QVERIFY(!root.exists("right/renamed folder"));
        auto* bar = window.findChild<QToolBar*>("functionToolbar");
        bool f2 = false, f7 = false;
        for (auto* button : bar->findChildren<QToolButton*>())
        {
            if (button->defaultAction() == window.findChild<QAction*>("renameFiles"))
                f2 = button->text().startsWith("F2 ");
            if (button->defaultAction() == window.findChild<QAction*>("makeDirectory"))
                f7 = button->text().startsWith("F7 ");
        }
        QVERIFY(f2 && f7);
    }
    void cancelAndCollisionDoNotChangeExistingFiles()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QFile file(root.filePath("existing.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("keep me");
        file.close();
        MainWindow window(root.path(), root.path(), false);
        auto* controller = window.findChild<OperationController*>();
        QVERIFY(controller);
        QSignalSpy finished(controller, &OperationController::finished);
        window.findChild<QAction*>("makeDirectory")->trigger();
        auto* cancel = window.findChild<NameDialog*>("operationNameDialog");
        QVERIFY(cancel);
        cancel->findChild<QLineEdit*>("operationName")->setText("canceled");
        cancel->reject();
        QTRY_VERIFY(!window.findChild<NameDialog*>("operationNameDialog"));
        QVERIFY(!root.exists("canceled"));
        QCOMPARE(finished.size(), 0);
        window.findChild<QAction*>("makeDirectory")->trigger();
        auto* collision = window.findChild<NameDialog*>("operationNameDialog");
        QVERIFY(collision);
        collision->findChild<QLineEdit*>("operationName")->setText("existing.txt");
        collision->accept();
        QTRY_VERIFY(window.findChild<QMessageBox*>("operationError"));
        auto* message = window.findChild<QMessageBox*>("operationError");
        QVERIFY(message->text().contains("already exists"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("keep me"));
        message->accept();
    }
    void markedRenamesProceedOneAtATimeAndCancelTheRemainder()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        for (const auto* name : {"a.txt", "b.txt"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        }
        MainWindow window(root.path(), root.path(), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 3);
        for (const auto* name : {"a.txt", "b.txt"})
            panel->view()->selectionModel()->select(panel->visibleIndex(root.filePath(name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        auto* action = window.findChild<QAction*>("renameFiles");
        QVERIFY(action->isEnabled());
        action->trigger();
        auto* first = window.findChild<NameDialog*>("operationNameDialog");
        QVERIFY(first);
        QCOMPARE(first->findChild<QLineEdit*>("operationName")->text(), QString("a.txt"));
        first->findChild<QLineEdit*>("operationName")->setText("changed.txt");
        first->accept();
        QTRY_VERIFY(root.exists("changed.txt"));
        NameDialog* second = nullptr;
        QTRY_VERIFY(([&]
                     {
            for (auto* dialog : window.findChildren<NameDialog*>("operationNameDialog"))
                if (dialog->isVisible() && dialog->findChild<QLineEdit*>("operationName")->text() == "b.txt") second = dialog;
            return second != nullptr; })());
        second->reject();
        QTRY_VERIFY(action->isEnabled());
        QVERIFY(!root.exists("a.txt"));
        QVERIFY(root.exists("b.txt"));
        QVERIFY(root.exists("changed.txt"));
    }

    void renameFailureStopsTheSessionAndKeepsCompletedResults()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        for (const auto* name : {"a.txt", "b.txt", "c.txt", "d.txt"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        }
        MainWindow window(root.path(), root.path(), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 5);
        for (const auto* name : {"a.txt", "b.txt", "c.txt", "d.txt"})
            panel->view()->selectionModel()->select(panel->visibleIndex(root.filePath(name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        window.findChild<QAction*>("renameFiles")->trigger();
        for (const auto& pair : {qMakePair(QString("a.txt"), QString("first.txt")), qMakePair(QString("b.txt"), QString("second.txt")), qMakePair(QString("c.txt"), QString("first.txt"))})
        {
            NameDialog* current = nullptr;
            QTRY_VERIFY(([&]
                         {
                for (auto* dialog : window.findChildren<NameDialog*>("operationNameDialog"))
                    if (dialog->isVisible() && dialog->findChild<QLineEdit*>("operationName")->text() == pair.first) current = dialog;
                return current != nullptr; })());
            current->findChild<QLineEdit*>("operationName")->setText(pair.second);
            current->accept();
        }
        QTRY_VERIFY(window.findChild<QMessageBox*>("operationError"));
        auto* message = window.findChild<QMessageBox*>("operationError");
        QVERIFY(message->text().contains("Completed renames: 2"));
        message->accept();
        QVERIFY(root.exists("first.txt"));
        QVERIFY(root.exists("second.txt"));
        QVERIFY(root.exists("c.txt"));
        QVERIFY(root.exists("d.txt"));
        QVERIFY(!root.exists("a.txt"));
        QVERIFY(!root.exists("b.txt"));
        QTRY_COMPARE(panel->selectedPaths().size(), 4);
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
        const auto dialogPath = qEnvironmentVariable("SALAMANDER_DIALOG_SCREENSHOT");
        if (!dialogPath.isEmpty())
        {
            window.findChild<QAction*>("makeDirectory")->trigger();
            auto* dialog = window.findChild<NameDialog*>("operationNameDialog");
            QVERIFY(dialog);
            dialog->findChild<QLineEdit*>("operationName")->setText(QString::fromUtf8("nova pasta á"));
            QTest::qWait(50);
            QVERIFY(dialog->grab().save(dialogPath));
            dialog->reject();
        }
    }
};
QTEST_MAIN(WindowTests)
#include "window_tests.moc"

// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "appearance.h"
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
#include "namedialog.h"
#include "operationcontroller.h"
#include "copycontroller.h"
#include "copydialogs.h"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QPushButton>
#include <QMessageBox>
#include <QPointer>
#include <QPaintEvent>
#include <QStandardItemModel>
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
#include <QScopeGuard>
#include <sys/stat.h>
#include <QUrl>
#include <algorithm>
#include <QToolBar>
#include <QToolButton>
class ViewportPaintRegion : public QObject
{
public:
    QRegion painted;

protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Paint)
            painted += static_cast<QPaintEvent*>(event)->region();
        return QObject::eventFilter(object, event);
    }
};

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
        for (const auto* name : {"userMenu", "connectNetwork", "disconnectNetwork"})
        {
            auto* action = window.findChild<QAction*>(name);
            QVERIFY(action);
            QVERIFY(!action->isEnabled());
        }
    }
    void fullRowSelectChangesMouseTargetAndPreservesMarks()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir("folder"));
        QFile file(temporary.filePath("test.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QSettings settings(temporary.filePath("preferences.ini"), QSettings::IniFormat);
        {
            MainWindow window(temporary.path(), temporary.path(), false, &settings);
            window.show();
            window.activateWindow();
            auto* panel = window.leftPanel();
            auto* view = panel->view();
            const auto filePath = file.fileName();
            const auto folderPath = temporary.filePath("folder");
            QTRY_VERIFY(panel->visibleIndex(filePath).isValid());
            QTRY_VERIFY(panel->visibleIndex(folderPath).isValid());
            auto* option = window.findChild<QAction*>("fullRowSelect");
            QVERIFY(option && !option->isChecked());
            const auto item = panel->visibleIndex(filePath);
            const auto folder = panel->visibleIndex(folderPath);
            const QPoint folderSize = view->visualRect(folder.siblingAtColumn(DetailedFileModel::Size)).center();
            view->selectionModel()->setCurrentIndex(item, QItemSelectionModel::NoUpdate);
            view->selectionModel()->select(item, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::ControlModifier, folderSize);
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, folderSize);
            QCOMPARE(panel->currentPath(), temporary.path());
            QCOMPARE(view->currentIndex(), item);
            QCOMPARE(panel->selectedPaths(), QStringList{filePath});
            const QPoint folderName = view->visualRect(folder).topLeft() + QPoint(10, view->visualRect(folder).height() / 2);
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, folderName);
            QCOMPARE(view->currentIndex(), folder);
            option->trigger();
            QVERIFY(window.rightPanel()->view()->fullRowSelect());
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::ControlModifier, folderSize);
            QCOMPARE(view->currentIndex().siblingAtColumn(0), folder);
            QVERIFY(panel->selectedPaths().contains(folderPath));
            QCOMPARE(panel->selectedPaths().size(), 2);
            option->trigger();
            QCOMPARE(view->currentIndex(), folder);
            QCOMPARE(panel->selectedPaths().size(), 2);
            option->trigger();
        }
        MainWindow reopened(temporary.path(), temporary.path(), false, &settings);
        QVERIFY(reopened.leftPanel()->view()->fullRowSelect());
        QVERIFY(reopened.rightPanel()->view()->fullRowSelect());
    }

    void fullRowFocusRepaintsPreviousAndCurrentRows()
    {
        QStandardItemModel model(2, 3);
        model.setData(model.index(0, 0), QString("first"));
        model.setData(model.index(1, 0), QString("second"));
        FileView view;
        view.setModel(&model);
        view.setRootIsDecorated(false);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        view.setFullRowSelect(true);
        view.resize(400, 200);
        view.show();
        view.activateWindow();
        view.setFocus();
        QTRY_VERIFY(view.hasFocus());

        const auto first = model.index(0, 0);
        const auto second = model.index(1, 0);
        view.selectionModel()->setCurrentIndex(model.index(0, 2), QItemSelectionModel::NoUpdate);
        QCoreApplication::processEvents();

        ViewportPaintRegion updates;
        view.viewport()->installEventFilter(&updates);
        const auto secondName = view.visualRect(second);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, secondName.center());
        QCOMPARE(view.currentIndex(), second);
        const auto fullRow = [&view](const QModelIndex& item)
        {
            const auto cell = view.visualRect(item);
            return QRect(0, cell.top(), view.viewport()->width(), cell.height());
        };
        const auto expected = QRegion(fullRow(first)) + QRegion(fullRow(second));
        QTRY_VERIFY_WITH_TIMEOUT(expected.subtracted(updates.painted).isEmpty(), 1000);
    }

    void appearancePreviewCancelAndSave()
    {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath("preferences.ini"), QSettings::IniFormat);
        Appearance::apply(settings);
        const auto system = QApplication::palette();
        MainWindow window(temporary.path(), temporary.path(), false, &settings);
        window.show();
        auto* action = window.findChild<QAction*>("appearance");
        QVERIFY(action);
        const auto choose = [&](bool save)
        {
            auto* dialog = window.findChild<QDialog*>("appearanceDialog");
            QVERIFY(dialog);
            auto* combo = dialog->findChild<QComboBox*>("themeChoice");
            combo->setCurrentIndex(combo->findData("dracula"));
            const auto previewBase = QApplication::palette().color(QPalette::Base);
            const auto previewText = window.leftPanel()->view()->palette().color(QPalette::Text);
            dialog->done(save ? QDialog::Accepted : QDialog::Rejected);
            QCOMPARE(previewBase, QColor("#21222c"));
            QCOMPARE(previewText, QColor("#f8f8f2"));
        };
        QTimer::singleShot(0, &window, [&]
                           { choose(false); });
        action->trigger();
        QCOMPARE(QApplication::palette(), system);
        QVERIFY(!settings.contains("appearance/theme"));
        QTimer::singleShot(0, &window, [&]
                           { choose(true); });
        action->trigger();
        QCOMPARE(settings.value("appearance/theme").toString(), QString("dracula"));
        settings.sync();
        QSettings reopened(settings.fileName(), QSettings::IniFormat);
        QApplication::setPalette(QPalette());
        Appearance::apply(reopened);
        QCOMPARE(QApplication::palette().color(QPalette::Link), QColor("#bd93f9"));
        settings.setValue("appearance/theme", "system");
        Appearance::apply(settings);
        QCOMPARE(QApplication::palette(), system);
    }

    void exportedThemeCanBeImportedAndReopened()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath("shared.json");
        QSettings settings(temporary.filePath("preferences.ini"), QSettings::IniFormat);
        MainWindow window(temporary.path(), temporary.path(), false, &settings);
        auto* action = window.findChild<QAction*>("appearance");
        QVERIFY(action);
        QTimer::singleShot(0, &window, [&]
                           {
            auto* dialog = window.findChild<QDialog*>("appearanceDialog");
            QVERIFY(dialog);
            auto* combo = dialog->findChild<QComboBox*>("themeChoice");
            combo->setCurrentIndex(combo->findData("dracula"));
            const auto pickFile = [&] {
                auto* chooser = dialog->findChild<QFileDialog*>();
                QVERIFY(chooser);
                QTimer::singleShot(500, chooser, &QDialog::reject);
                chooser->selectFile(path);
                QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
            };
            const auto saveAndOpen = [&](const char* button) {
                QTimer::singleShot(0, dialog, pickFile);
                dialog->findChild<QPushButton*>(button)->click();
            };
            saveAndOpen("exportTheme");
            const bool exported = QFileInfo(path).isFile();
            if (exported)
                saveAndOpen("importTheme");
            const auto selectedName = combo->currentText();
            const auto selectedId = combo->currentData().toString();
            dialog->accept();
            QVERIFY(exported);
            QCOMPARE(selectedName, QString("Dracula"));
            QVERIFY(selectedId != "dracula"); });
        action->trigger();
        settings.sync();
        QSettings reopened(settings.fileName(), QSettings::IniFormat);
        Appearance::apply(reopened);
        const auto loaded = QApplication::palette().color(QPalette::Link);
        QApplication::setPalette(QPalette());
        QCOMPARE(loaded, QColor("#bd93f9"));
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

    void f8ConfirmsAndTrashesMarkedItemsWithRestoreMetadata()
    {
        const QDir testRoot(qEnvironmentVariable("SALAMANDER_TRASH_TEST_ROOT", QDir::homePath()));
        QTemporaryDir temporary(testRoot.filePath("salamander-trash-test-XXXXXX"));
        QVERIFY(temporary.isValid());
        // GIO uses the home Trash for entries on the home device.
        struct stat testDevice
        {
        }, homeDevice{};
        QCOMPARE(::stat(QFile::encodeName(temporary.path()).constData(), &testDevice), 0);
        QCOMPARE(::stat(QFile::encodeName(QDir::homePath()).constData(), &homeDevice), 0);
        QCOMPARE(testDevice.st_dev, homeDevice.st_dev);
        QDir root(temporary.path());
        QVERIFY(root.mkpath("source/folder-a"));
        QVERIFY(root.mkdir("data"));
        const QByteArray previousData = qgetenv("XDG_DATA_HOME");
        QVERIFY(qputenv("XDG_DATA_HOME", QFile::encodeName(root.filePath("data"))));
        const auto restoreEnvironment = qScopeGuard([&]
                                                    {
            if (previousData.isNull())
                qunsetenv("XDG_DATA_HOME");
            else
                qputenv("XDG_DATA_HOME", previousData); });
        const QDir source(root.filePath("source"));
        const QString filename = QString::fromUtf8("a % cão.txt");
        const auto write = [&](const QString& name)
        {
            QFile file(source.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
        };
        QVERIFY(write(filename));
        QVERIFY(write("folder-a/child"));
        QVERIFY(write("keep.txt"));
        QVERIFY(QFile::link(source.filePath("missing"), source.filePath("broken-link")));
        const QDir trash(root.filePath("data/Trash"));

        QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
        MainWindow window(source.path(), source.path(), false, &settings);
        window.show();
        window.activateWindow();
        auto* panel = window.leftPanel();
        auto* view = panel->view();
        view->setFocus();
        QTRY_VERIFY(view->hasFocus());
        for (const auto& name : {filename, QString("folder-a"), QString("broken-link"), QString("keep.txt")})
            QTRY_VERIFY(panel->visibleIndex(source.filePath(name)).isValid());

        view->selectionModel()->setCurrentIndex(panel->visibleIndex(source.filePath(filename)), QItemSelectionModel::NoUpdate);
        QTest::keyClick(view, Qt::Key_F8);
        auto* confirmation = window.findChild<QMessageBox*>("trashConfirmation");
        QVERIFY(confirmation);
        QVERIFY(confirmation->text().contains(filename));
        QCOMPARE(confirmation->defaultButton(), confirmation->button(QMessageBox::Cancel));
        QTest::keyClick(confirmation, Qt::Key_Escape);
        QTRY_VERIFY(!window.findChild<QMessageBox*>("trashConfirmation"));
        QVERIFY(source.exists(filename));
        QVERIFY(!trash.exists());

        const QStringList marked{"folder-a", filename, "broken-link"};
        for (const auto& name : marked)
            view->selectionModel()->select(panel->visibleIndex(source.filePath(name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        view->selectionModel()->setCurrentIndex(panel->visibleIndex(source.filePath("folder-a")), QItemSelectionModel::NoUpdate);
        window.activateWindow();
        view->setFocus();
        QTRY_VERIFY(view->hasFocus());
        QTest::keyClick(view, Qt::Key_F8);
        confirmation = window.findChild<QMessageBox*>("trashConfirmation");
        QVERIFY(confirmation);
        for (const auto& name : marked)
            QVERIFY(confirmation->text().contains(name));
        confirmation->findChild<QPushButton*>("confirmTrash")->click();
        QTRY_VERIFY(trash.exists("files/folder-a/child"));
        QTRY_VERIFY(trash.exists("files/" + filename));
        QTRY_VERIFY(QFileInfo(trash.filePath("files/broken-link")).isSymbolicLink());
        QTRY_VERIFY(panel->selectedPaths().isEmpty());
        QTRY_COMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("keep.txt"));
        QVERIFY(source.exists("keep.txt"));
        QVERIFY(!source.exists(filename));
        QVERIFY(!QFileInfo(source.filePath("broken-link")).isSymbolicLink());

        QFile info(trash.filePath("info/" + filename + ".trashinfo"));
        QVERIFY(info.open(QIODevice::ReadOnly));
        const auto fields = info.readAll().split('\n');
        QVERIFY(fields.contains("[Trash Info]"));
        const auto path = std::find_if(fields.begin(), fields.end(), [](const QByteArray& field)
                                       { return field.startsWith("Path="); });
        QVERIFY(path != fields.end());
        const auto original = QUrl::fromPercentEncoding(path->mid(5));
        QCOMPARE(original, source.filePath(filename));
        QVERIFY(QFile::rename(trash.filePath("files/" + filename), original));
        QFile restored(original);
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), QByteArray("test"));

        QVERIFY(write("another.txt"));
        QVERIFY(write("will-fail.txt"));
        QVERIFY(write("will-stay.txt"));
        for (const QString name : {"another.txt", "will-fail.txt", "will-stay.txt"})
            QTRY_VERIFY(panel->visibleIndex(source.filePath(name)).isValid());
        for (const QString name : {"another.txt", "will-fail.txt", "will-stay.txt"})
            view->selectionModel()->select(panel->visibleIndex(source.filePath(name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        window.activateWindow();
        view->setFocus();
        QTRY_VERIFY(view->hasFocus());
        QTest::keyClick(view, Qt::Key_F8);
        confirmation = window.findChild<QMessageBox*>("trashConfirmation");
        QVERIFY(confirmation);
        QVERIFY(QFile::remove(source.filePath("will-fail.txt")));
        confirmation->findChild<QPushButton*>("confirmTrash")->click();
        QTRY_VERIFY(window.findChild<QMessageBox*>("trashError"));
        const auto* error = window.findChild<QMessageBox*>("trashError");
        QVERIFY(error->text().contains("will-fail.txt"));
        QVERIFY(error->detailedText().contains("will-stay.txt"));
        QVERIFY(trash.exists("files/another.txt"));
        QVERIFY(source.exists("will-stay.txt"));
        QVERIFY(!trash.exists("files/will-stay.txt"));

        QVERIFY(write("another.txt"));
        const auto duplicate = FileOperations::execute({FileOperations::Kind::Trash, source.path(), {}, {}, {"another.txt"}});
        QVERIFY2(duplicate.succeeded(), qPrintable(duplicate.message));
        const auto entries = QDir(trash.filePath("files")).entryList({"another*"}, QDir::Files);
        QCOMPARE(entries.size(), 2);
        const auto alreadyTrashed = FileOperations::execute({FileOperations::Kind::Trash, trash.filePath("files"), {}, {}, {entries.first()}});
        QCOMPARE(alreadyTrashed.error, FileOperations::Error::Unsupported);
        QCOMPARE(QDir(trash.filePath("files")).entryList({"another*"}, QDir::Files), entries);
    }

    void f5CopiesMarkedItemsToTheOtherPanelWithoutDeletingSources()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        for (const auto* name : {"first.txt", "second.txt", "unselected.txt"})
        {
            QFile file(root.filePath(QString("left/") + name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        }
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        window.show();
        window.activateWindow();
        auto* source = window.leftPanel();
        source->view()->setFocus();
        QTRY_VERIFY(source->view()->hasFocus());
        QTRY_COMPARE(source->view()->model()->rowCount(source->view()->rootIndex()), 4);
        source->view()->selectionModel()->setCurrentIndex(source->visibleIndex(root.filePath("left/unselected.txt")), QItemSelectionModel::NoUpdate);
        for (const auto* name : {"first.txt", "second.txt"})
            source->view()->selectionModel()->select(source->visibleIndex(root.filePath(QString("left/") + name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::keyClick(source->view(), Qt::Key_F5);
        auto* dialog = window.findChild<CopyDialog*>("copyDialog");
        QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QLineEdit*>("copyDestination")->text(), root.filePath("right"));
        dialog->accept();
        auto* controller = window.findChild<CopyController*>();
        QVERIFY(controller);
        QTRY_VERIFY(!controller->isBusy());
        QTRY_VERIFY(!window.findChild<CopyProgressDialog*>("copyProgressDialog"));
        QVERIFY(root.exists("right/first.txt"));
        QVERIFY(root.exists("right/second.txt"));
        QVERIFY(!root.exists("right/unselected.txt"));
        QVERIFY(root.exists("left/first.txt"));
        QVERIFY(root.exists("left/second.txt"));
        QTRY_COMPARE(window.rightPanel()->view()->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("second.txt"));
        QCOMPARE(source->selectedPaths().size(), 2);
        QCOMPARE(window.activePanel(), window.leftPanel());
    }
    void f6MovesMarkedItemsToTheOtherPanel()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        for (const auto* name : {"first.txt", "second.txt", "unselected.txt"})
        {
            QFile file(root.filePath(QString("left/") + name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        }
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        window.show();
        window.activateWindow();
        auto* source = window.leftPanel();
        source->view()->setFocus();
        QTRY_VERIFY(source->view()->hasFocus());
        QTRY_COMPARE(source->view()->model()->rowCount(source->view()->rootIndex()), 4);
        source->view()->selectionModel()->setCurrentIndex(source->visibleIndex(root.filePath("left/unselected.txt")), QItemSelectionModel::NoUpdate);
        for (const auto* name : {"first.txt", "second.txt"})
            source->view()->selectionModel()->select(source->visibleIndex(root.filePath(QString("left/") + name)), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::keyClick(source->view(), Qt::Key_F6);
        auto* dialog = window.findChild<CopyDialog*>("moveDialog");
        QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QLineEdit*>("copyDestination")->text(), root.filePath("right"));
        dialog->accept();
        auto* controller = window.findChild<CopyController*>();
        QVERIFY(controller);
        QTRY_VERIFY(!controller->isBusy());
        QTRY_VERIFY(!window.findChild<CopyProgressDialog*>("moveProgressDialog"));
        QVERIFY(root.exists("right/first.txt"));
        QVERIFY(root.exists("right/second.txt"));
        QVERIFY(!root.exists("right/unselected.txt"));
        QVERIFY(!root.exists("left/first.txt"));
        QVERIFY(!root.exists("left/second.txt"));
        QTRY_COMPARE(window.rightPanel()->view()->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("second.txt"));
        QTRY_VERIFY(source->selectedPaths().isEmpty());
        QVERIFY(root.exists("left/unselected.txt"));
        QCOMPARE(window.activePanel(), window.leftPanel());
    }
    void copyConflictButtonsSkipAndReplaceOnlyAfterConfirmation()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        for (const auto& pair : {qMakePair(QString("left/file"), QByteArray("new")), qMakePair(QString("right/file"), QByteArray("old"))})
        {
            QFile file(root.filePath(pair.first));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(pair.second);
        }
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 2);
        panel->view()->selectionModel()->setCurrentIndex(panel->visibleIndex(root.filePath("left/file")), QItemSelectionModel::NoUpdate);
        auto* action = window.findChild<QAction*>("copyFiles");
        auto* controller = window.findChild<CopyController*>();
        QVERIFY(action->isEnabled());
        for (const auto* buttonName : {"skipCopy", "replaceCopy"})
        {
            action->trigger();
            auto* dialog = window.findChild<CopyDialog*>("copyDialog");
            QVERIFY(dialog);
            dialog->accept();
            QTRY_VERIFY(window.findChild<QMessageBox*>("copyConflict"));
            auto* conflict = window.findChild<QMessageBox*>("copyConflict");
            QFile before(root.filePath("right/file"));
            QVERIFY(before.open(QIODevice::ReadOnly));
            QCOMPARE(before.readAll(), QByteArray("old"));
            before.close();
            QTest::mouseClick(conflict->findChild<QPushButton*>(buttonName), Qt::LeftButton);
            QTRY_VERIFY(!controller->isBusy());
            QTRY_VERIFY(!window.findChild<CopyDialog*>("copyDialog"));
            QTRY_VERIFY(!window.findChild<QMessageBox*>("copyConflict"));
        }
        QFile after(root.filePath("right/file"));
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), QByteArray("new"));
    }
    void moveConflictButtonsSkipAndReplaceOnlyAfterConfirmation()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        for (const auto& pair : {qMakePair(QString("left/file"), QByteArray("new")), qMakePair(QString("right/file"), QByteArray("old"))})
        {
            QFile file(root.filePath(pair.first));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(pair.second);
        }
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 2);
        panel->view()->selectionModel()->setCurrentIndex(panel->visibleIndex(root.filePath("left/file")), QItemSelectionModel::NoUpdate);
        auto* action = window.findChild<QAction*>("moveFiles");
        auto* controller = window.findChild<CopyController*>();
        QVERIFY(action->isEnabled());
        for (const auto* buttonName : {"skipCopy", "replaceCopy"})
        {
            action->trigger();
            auto* dialog = window.findChild<CopyDialog*>("moveDialog");
            QVERIFY(dialog);
            dialog->accept();
            QTRY_VERIFY(window.findChild<QMessageBox*>("moveConflict"));
            auto* conflict = window.findChild<QMessageBox*>("moveConflict");
            QFile before(root.filePath("right/file"));
            QVERIFY(before.open(QIODevice::ReadOnly));
            QCOMPARE(before.readAll(), QByteArray("old"));
            before.close();
            QTest::mouseClick(conflict->findChild<QPushButton*>(buttonName), Qt::LeftButton);
            QTRY_VERIFY(!controller->isBusy());
            QTRY_VERIFY(!window.findChild<CopyDialog*>("moveDialog"));
            QTRY_VERIFY(!window.findChild<QMessageBox*>("moveConflict"));
        }
        QVERIFY(!root.exists("left/file"));
        QFile after(root.filePath("right/file"));
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), QByteArray("new"));
    }
    void copyCancelButtonKeepsTheSourceAndRemovesUnfinishedOutput()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        QFile file(root.filePath("left/large"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(32 * 1024 * 1024));
        file.close();
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 2);
        panel->view()->selectionModel()->setCurrentIndex(panel->visibleIndex(file.fileName()), QItemSelectionModel::NoUpdate);
        window.findChild<QAction*>("copyFiles")->trigger();
        auto* dialog = window.findChild<CopyDialog*>("copyDialog");
        QVERIFY(dialog);
        dialog->accept();
        auto* progress = window.findChild<CopyProgressDialog*>("copyProgressDialog");
        QVERIFY(progress);
        QSignalSpy canceled(progress, &CopyProgressDialog::cancelRequested);
        auto* controller = window.findChild<CopyController*>();
        QSignalSpy finished(controller, &CopyController::finished);
        progress->findChild<QPushButton*>("cancelCopy")->click();
        QCOMPARE(canceled.size(), 1);
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(root.exists("left/large"));
        const auto result = qvariant_cast<CopyOperations::Result>(finished.first().first());
        if (result.completed.isEmpty())
            QVERIFY(!root.exists("right/large"));
        else
            QCOMPARE(QFileInfo(root.filePath("right/large")).size(), qint64(32 * 1024 * 1024));
        QVERIFY(QDir(root.filePath("right")).entryList({".salamander-copy-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void missingCopyDestinationShowsAnErrorAndKeepsTheSource()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("left"));
        QVERIFY(root.mkdir("right"));
        QFile file(root.filePath("left/file"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original");
        file.close();
        MainWindow window(root.filePath("left"), root.filePath("right"), false);
        auto* panel = window.leftPanel();
        QTRY_COMPARE(panel->view()->model()->rowCount(panel->view()->rootIndex()), 2);
        panel->view()->selectionModel()->setCurrentIndex(panel->visibleIndex(file.fileName()), QItemSelectionModel::NoUpdate);
        window.findChild<QAction*>("copyFiles")->trigger();
        auto* dialog = window.findChild<CopyDialog*>("copyDialog");
        QVERIFY(dialog);
        dialog->findChild<QLineEdit*>("copyDestination")->setText(root.filePath("missing"));
        dialog->accept();
        QTRY_VERIFY(window.findChild<QMessageBox*>("copyError"));
        QVERIFY(root.exists("left/file"));
        QVERIFY(!root.exists("missing"));
        QVERIFY(!window.findChild<CopyController*>()->isBusy());
        window.findChild<QMessageBox*>("copyError")->accept();
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
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings preferences(config.filePath("preferences.ini"), QSettings::IniFormat);
        preferences.setValue("appearance/theme", qEnvironmentVariable("SALAMANDER_TEST_THEME", "system"));
        preferences.setValue("panels/fullRowSelect", qEnvironmentVariableIsSet("SALAMANDER_TEST_FULL_ROW"));
        Appearance::apply(preferences);
        MainWindow window(root.path(), root.filePath("projects"), true, &preferences);
        window.show();
        QTRY_VERIFY(window.leftPanel()->view()->model()->rowCount(window.leftPanel()->view()->rootIndex()) >= 6);
        const auto marked = window.leftPanel()->visibleIndex(root.filePath("notes.txt"));
        window.leftPanel()->view()->selectionModel()->select(marked, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        window.leftPanel()->view()->selectionModel()->setCurrentIndex(window.leftPanel()->visibleIndex(root.filePath("archive.tar.gz")), QItemSelectionModel::NoUpdate);
        QTest::qWait(150);
        QVERIFY(window.grab().save(path));
        const auto appearancePath = qEnvironmentVariable("SALAMANDER_TEST_APPEARANCE_SCREENSHOT");
        if (!appearancePath.isEmpty())
        {
            bool saved = false;
            QTimer::singleShot(50, &window, [&]
                               {
                auto* appearance = window.findChild<QDialog*>("appearanceDialog");
                saved = appearance->grab().save(appearancePath);
                appearance->reject(); });
            window.findChild<QAction*>("appearance")->trigger();
            QVERIFY(saved);
        }
        const auto copyPath = qEnvironmentVariable("SALAMANDER_COPY_SCREENSHOT");
        if (!copyPath.isEmpty())
        {
            window.findChild<QAction*>("copyFiles")->trigger();
            auto* copy = window.findChild<CopyDialog*>("copyDialog");
            QVERIFY(copy);
            QTest::qWait(50);
            QVERIFY(copy->grab().save(copyPath));
            copy->reject();
        }
        const auto movePath = qEnvironmentVariable("SALAMANDER_MOVE_SCREENSHOT");
        if (!movePath.isEmpty())
        {
            window.findChild<QAction*>("moveFiles")->trigger();
            auto* move = window.findChild<CopyDialog*>("moveDialog");
            QVERIFY(move);
            QTest::qWait(50);
            QVERIFY(move->grab().save(movePath));
            move->reject();
        }
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

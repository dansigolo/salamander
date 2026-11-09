// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
#include <QAbstractItemModelTester>
#include <QLabel>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
class PanelTests : public QObject
{
    Q_OBJECT
private slots:
    void navigationAndErrors()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir(QStringLiteral("sub")));
        QFile file(root.filePath(QStringLiteral("a.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FilePanel panel(root.absolutePath());
        QCOMPARE(panel.currentPath(), root.absolutePath());
        QTRY_COMPARE(panel.view()->model()->rowCount(panel.view()->rootIndex()), 3);
        QVERIFY(panel.navigate(QStringLiteral("sub")));
        QCOMPARE(panel.currentPath(), root.filePath(QStringLiteral("sub")));
        panel.goUp();
        QCOMPARE(panel.currentPath(), root.absolutePath());
        QVERIFY(!panel.navigate(QStringLiteral("missing")));
        QVERIFY(!panel.navigate(QStringLiteral("a.txt")));
        QCOMPARE(panel.currentPath(), root.absolutePath());
        QVERIFY(panel.model()->isReadOnly());
    }
    void hiddenFilesAndIndependentPanels()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QFile hidden(root.filePath(QStringLiteral(".hidden")));
        QVERIFY(hidden.open(QIODevice::WriteOnly));
        hidden.close();
#ifdef Q_OS_WIN
        const auto nativePath = QDir::toNativeSeparators(hidden.fileName());
        QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), FILE_ATTRIBUTE_HIDDEN));
#endif
        QVERIFY(root.mkdir(QStringLiteral("sub")));
        FilePanel left(root.absolutePath()), right(root.absolutePath());
        QTRY_COMPARE(left.view()->model()->rowCount(left.view()->rootIndex()), 2);
        left.setHiddenFilesVisible(true);
        QTRY_COMPARE(left.view()->model()->rowCount(left.view()->rootIndex()), 3);
        QTRY_COMPARE(right.view()->model()->rowCount(right.view()->rootIndex()), 2);
        QVERIFY(left.navigate(QStringLiteral("sub")));
        QCOMPARE(right.currentPath(), root.absolutePath());
    }
    void markingAndIndependentFocus()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("folder"));
        for (const auto& name : {"alpha.txt", "alphabet.txt", "beta.txt"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FilePanel panel(root.path());
        panel.show();
        auto* view = panel.view();
        view->setFocus();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 5);
        const auto alpha = panel.visibleIndex(root.filePath("alpha.txt"));
        const auto beta = panel.visibleIndex(root.filePath("beta.txt"));
        view->selectionModel()->setCurrentIndex(alpha, QItemSelectionModel::NoUpdate);
        QTest::keyClick(view, Qt::Key_Insert);
        QVERIFY(view->selectionModel()->isSelected(alpha));
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("alphabet.txt"));
        QTest::keyClick(view, Qt::Key_Down);
        QCOMPARE(view->currentIndex(), beta);
        QVERIFY(view->selectionModel()->isSelected(alpha));
        QTest::keyClick(view, Qt::Key_Space);
        QVERIFY(view->selectionModel()->isSelected(beta));
        QCOMPARE(view->currentIndex(), beta);
        QTest::keyClick(view, Qt::Key_Space);
        QVERIFY(!view->selectionModel()->isSelected(beta));
        QTest::keyClick(view, Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(view->selectionModel()->selectedRows().size(), 4);
        view->selectionModel()->setCurrentIndex(view->model()->index(1, 0, view->rootIndex()), QItemSelectionModel::NoUpdate);
        QTest::keyClick(view, Qt::Key_End, Qt::ShiftModifier);
        QCOMPARE(view->selectionModel()->selectedRows().size(), 0);
        QTest::keyClick(view, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(view->selectionModel()->selectedRows().size(), 4);
        QTest::keyClick(view, Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(view->selectionModel()->selectedRows().size(), 0);
    }
    void quickSearchAndParentNavigation()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("child"));
        for (const auto& name : {"alpha.txt", "alphabet.txt", "beta.txt"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        FilePanel panel(root.path());
        panel.setPreserveSelectionOnNavigation(true);
        panel.show();
        auto* view = panel.view();
        view->setFocus();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 5);
        const auto marked = panel.visibleIndex(root.filePath("beta.txt"));
        view->selectionModel()->select(marked, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::keyClicks(view, "al");
        QCOMPARE(view->searchPrefix(), QString("al"));
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("alpha.txt"));
        QVERIFY(view->selectionModel()->isSelected(marked));
        QTest::keyClick(view, Qt::Key_Down);
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("alphabet.txt"));
        QTest::keyClick(view, Qt::Key_Up);
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("alpha.txt"));
        QTest::keyClick(view, Qt::Key_Right);
        QCOMPARE(view->searchPrefix(), QString("alp"));
        QTest::keyClick(view, Qt::Key_Backspace);
        QCOMPARE(view->searchPrefix(), QString("al"));
        QTest::keyClick(view, Qt::Key_Z);
        QCOMPARE(view->searchPrefix(), QString("al")); // Reject an unmatched character.
        QTest::keyClick(view, Qt::Key_Escape);
        QVERIFY(view->searchPrefix().isEmpty());
        QVERIFY(panel.navigate("child"));
        QTest::keyClick(view, Qt::Key_Backspace);
        QCOMPARE(panel.currentPath(), root.path());
        QTRY_COMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("child"));
        QVERIFY(view->selectionModel()->isSelected(marked));
    }
    void directoriesFirstInBothSortOrdersAndHistory()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("z-directory"));
        for (const auto& name : {"a.txt", "b.txt"})
        {
            QFile file(root.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name[0] == 'a' ? "a" : "longer");
        }
        FilePanel panel(root.path());
        panel.setPreserveSelectionOnNavigation(true);
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
        for (const auto order : {Qt::AscendingOrder, Qt::DescendingOrder})
            for (const int column : {0, 1, 2, 3, 4, 5})
            {
                view->sortByColumn(column, order);
                QCOMPARE(view->model()->index(1, 0, view->rootIndex()).data().toString(), QString("z-directory"));
            }
        const auto file = panel.visibleIndex(root.filePath("b.txt"));
        view->selectionModel()->setCurrentIndex(file, QItemSelectionModel::NoUpdate);
        view->selectionModel()->select(file, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QVERIFY(panel.navigate("z-directory"));
        QVERIFY(panel.navigate(root.path()));
        QTRY_COMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("b.txt"));
        QVERIFY(view->selectionModel()->isSelected(panel.visibleIndex(root.filePath("b.txt"))));
        QVERIFY(!panel.navigate("missing"));
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("b.txt"));
    }
    void detailedColumnsAndParentAreConsistent()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir root(temporary.path());
        QFile file(root.filePath("example.tar.gz"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("test"), qint64(4));
        file.close();
        FilePanel panel(root.path());
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 2);
        QCOMPARE(view->model()->columnCount(), 6);
        const auto index = panel.visibleIndex(file.fileName());
        QCOMPARE(index.data().toString(), QString("example.tar"));
        QCOMPARE(index.siblingAtColumn(1).data().toString(), QString("gz"));
        QCOMPARE(index.data(QFileSystemModel::FileNameRole).toString(), QString("example.tar.gz"));
        QVERIFY(!index.siblingAtColumn(3).data().toString().isEmpty());
        QVERIFY(!index.siblingAtColumn(4).data().toString().isEmpty());
        QCOMPARE(index.siblingAtColumn(5).data().toString().size(), 10);
        const auto parent = view->model()->index(0, 0, view->rootIndex());
        QCOMPARE(parent.data().toString(), QString(".."));
        QVERIFY(!parent.flags().testFlag(Qt::ItemIsSelectable));
        view->selectionModel()->setCurrentIndex(parent, QItemSelectionModel::NoUpdate);
        QTest::keyClick(view, Qt::Key_Insert);
        QVERIFY(panel.selectedPaths().isEmpty());
        QCOMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("example.tar.gz"));
        QTest::keyClick(view, Qt::Key_Space);
        QCOMPARE(panel.selectedPaths().size(), 1);
        QVERIFY(panel.findChild<QLabel*>("selectionStatus")->text().contains("selected"));
        QAbstractItemModelTester consistency(view->model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        consistency.setUseFetchMore(false);
    }
    void historyAndWatcherPreserveSelection()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("child"));
        QFile file(root.filePath("file.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FilePanel panel(root.path());
        panel.setPreserveSelectionOnNavigation(true);
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
        const auto index = panel.visibleIndex(file.fileName());
        view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        view->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QVERIFY(panel.navigate("child"));
        QVERIFY(panel.canGoBack());
        panel.goBack();
        QCOMPARE(panel.currentPath(), root.path());
        QVERIFY(panel.canGoForward());
        QTRY_COMPARE(panel.selectedPaths().size(), 1);
        panel.goForward();
        QCOMPARE(panel.currentPath(), root.filePath("child"));
        panel.goBack();
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            panel.goForward();
            panel.goBack();
        }
        QFile created(root.filePath("another.txt"));
        QVERIFY(created.open(QIODevice::WriteOnly));
        created.close();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
        QCOMPARE(panel.selectedPaths(), QStringList{file.fileName()});
    }
    void defaultClearsMarksButKeepsFocusAndInvalidNavigationPreservesMarks()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("child"));
        QFile file(root.filePath("file.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FilePanel panel(root.path());
        QVERIFY(!panel.preservesSelectionOnNavigation());
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
        const auto mark = [&]
        {
            for (const auto& path : {file.fileName(), root.filePath("child")})
                view->selectionModel()->select(panel.visibleIndex(path), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        };
        mark();
        QCOMPARE(panel.selectedPaths().size(), 2);
        QVERIFY(!panel.navigate("missing"));
        QCOMPARE(panel.selectedPaths().size(), 2);
        QVERIFY(panel.navigate(root.path())); // Same location is not a directory change.
        QCOMPARE(panel.selectedPaths().size(), 2);
        QVERIFY(panel.navigate("child"));
        panel.goUp();
        QTRY_COMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("child"));
        QVERIFY(panel.selectedPaths().isEmpty());
        mark();
        QVERIFY(panel.navigate("child"));
        panel.goBack();
        QTRY_COMPARE(panel.currentPath(), root.path());
        QVERIFY(panel.selectedPaths().isEmpty());
    }
    void disablingPreferenceDropsCachedMarksWithoutClearingCurrentMarks()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        QVERIFY(root.mkdir("child"));
        QFile file(root.filePath("file.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        FilePanel panel(root.path());
        panel.setPreserveSelectionOnNavigation(true);
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
        view->selectionModel()->select(panel.visibleIndex(file.fileName()), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        panel.setPreserveSelectionOnNavigation(false);
        QCOMPARE(panel.selectedPaths().size(), 1);
        panel.setPreserveSelectionOnNavigation(true);
        QVERIFY(panel.navigate("child"));              // Cache root marks while enabled.
        panel.setPreserveSelectionOnNavigation(false); // Drop that cache.
        panel.setPreserveSelectionOnNavigation(true);
        panel.goUp();
        QTRY_COMPARE(view->currentIndex().data(QFileSystemModel::FileNameRole).toString(), QString("child"));
        QVERIFY(panel.selectedPaths().isEmpty());
    }
};
QTEST_MAIN(PanelTests)
#include "panel_tests.moc"

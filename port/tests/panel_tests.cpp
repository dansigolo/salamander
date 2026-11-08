// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include "fileview.h"
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
        QTRY_COMPARE(panel.view()->model()->rowCount(panel.view()->rootIndex()), 2);
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
        QTRY_COMPARE(left.view()->model()->rowCount(left.view()->rootIndex()), 1);
        left.setHiddenFilesVisible(true);
        QTRY_COMPARE(left.view()->model()->rowCount(left.view()->rootIndex()), 2);
        QTRY_COMPARE(right.view()->model()->rowCount(right.view()->rootIndex()), 1);
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
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
        const auto alpha = panel.visibleIndex(root.filePath("alpha.txt"));
        const auto beta = panel.visibleIndex(root.filePath("beta.txt"));
        view->selectionModel()->setCurrentIndex(alpha, QItemSelectionModel::NoUpdate);
        QTest::keyClick(view, Qt::Key_Insert);
        QVERIFY(view->selectionModel()->isSelected(alpha));
        QCOMPARE(view->currentIndex().data().toString(), QString("alphabet.txt"));
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
        panel.show();
        auto* view = panel.view();
        view->setFocus();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 4);
        const auto marked = panel.visibleIndex(root.filePath("beta.txt"));
        view->selectionModel()->select(marked, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::keyClicks(view, "al");
        QCOMPARE(view->searchPrefix(), QString("al"));
        QCOMPARE(view->currentIndex().data().toString(), QString("alpha.txt"));
        QVERIFY(view->selectionModel()->isSelected(marked));
        QTest::keyClick(view, Qt::Key_Down);
        QCOMPARE(view->currentIndex().data().toString(), QString("alphabet.txt"));
        QTest::keyClick(view, Qt::Key_Up);
        QCOMPARE(view->currentIndex().data().toString(), QString("alpha.txt"));
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
        QTRY_COMPARE(view->currentIndex().data().toString(), QString("child"));
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
        auto* view = panel.view();
        QTRY_COMPARE(view->model()->rowCount(view->rootIndex()), 3);
        for (const auto order : {Qt::AscendingOrder, Qt::DescendingOrder})
            for (const int column : {0, 1, 2, 3})
            {
                view->sortByColumn(column, order);
                QCOMPARE(view->model()->index(0, 0, view->rootIndex()).data().toString(), QString("z-directory"));
            }
        const auto file = panel.visibleIndex(root.filePath("b.txt"));
        view->selectionModel()->setCurrentIndex(file, QItemSelectionModel::NoUpdate);
        view->selectionModel()->select(file, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QVERIFY(panel.navigate("z-directory"));
        QVERIFY(panel.navigate(root.path()));
        QTRY_COMPARE(view->currentIndex().data().toString(), QString("b.txt"));
        QVERIFY(view->selectionModel()->isSelected(panel.visibleIndex(root.filePath("b.txt"))));
        QVERIFY(!panel.navigate("missing"));
        QCOMPARE(view->currentIndex().data().toString(), QString("b.txt"));
    }
};
QTEST_MAIN(PanelTests)
#include "panel_tests.moc"

// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
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
        QTRY_COMPARE(panel.model()->rowCount(panel.view()->rootIndex()), 2);
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
        QVERIFY(root.mkdir(QStringLiteral("sub")));
        FilePanel left(root.absolutePath()), right(root.absolutePath());
        QTRY_COMPARE(left.model()->rowCount(left.view()->rootIndex()), 1);
        left.setHiddenFilesVisible(true);
        QTRY_COMPARE(left.model()->rowCount(left.view()->rootIndex()), 2);
        QTRY_COMPARE(right.model()->rowCount(right.view()->rootIndex()), 1);
        QVERIFY(left.navigate(QStringLiteral("sub")));
        QCOMPARE(right.currentPath(), root.absolutePath());
    }
};
QTEST_MAIN(PanelTests)
#include "panel_tests.moc"

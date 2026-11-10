// SPDX-License-Identifier: GPL-2.0-or-later
#include "copyoperations.h"
#include "copycontroller.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
#include <unistd.h>
using namespace CopyOperations;
class CopyTests : public QObject
{
    Q_OBJECT
    void write(const QString& path, const QByteArray& data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), qint64(data.size()));
    }
    QByteArray read(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }
    void directories(const QDir& root)
    {
        QVERIFY(root.mkdir("source"));
        QVERIFY(root.mkdir("destination"));
    }
    void clean(const QString& destination)
    {
        QVERIFY2(QDir(destination).entryList({".salamander-copy-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty(), qPrintable(destination));
    }
private slots:
    void filesDirectoriesHiddenNamesAndLinks()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        QVERIFY(root.mkpath("source/folder/nested"));
        write(root.filePath("source/folder/nested/file.txt"), "contents");
        write(root.filePath("source/folder/.hidden"), "hidden contents");
        const auto unicode = QString::fromUtf8("relatório com espaços.txt");
        write(root.filePath("source/") + unicode, "unicode");
        QVERIFY(QFile::link("missing-target", root.filePath("source/folder/broken")));
        QVERIFY(QFile::link("..", root.filePath("source/folder/nested/up")));
        QVERIFY(QFile::setPermissions(root.filePath("source/folder/nested/file.txt"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto result = execute({root.filePath("source"), {"folder", unicode}, root.filePath("destination")});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.completed.size(), 2);
        QVERIFY(!result.canceled);
        QCOMPARE(read(root.filePath("destination/folder/nested/file.txt")), QByteArray("contents"));
        QCOMPARE(read(root.filePath("destination/folder/.hidden")), QByteArray("hidden contents"));
        QCOMPARE(read(root.filePath("destination/") + unicode), QByteArray("unicode"));
        QVERIFY(QFileInfo(root.filePath("destination/folder/broken")).isSymbolicLink());
        QVERIFY(QFileInfo(root.filePath("destination/folder/nested/up")).isSymbolicLink());
        QVERIFY(QFile::permissions(root.filePath("destination/folder/nested/file.txt")).testFlag(QFileDevice::ExeOwner));
        QCOMPARE(read(root.filePath("source/folder/nested/file.txt")), QByteArray("contents"));
        QCOMPARE(QFileInfo(root.filePath("destination/folder/nested/file.txt")).lastModified(), QFileInfo(root.filePath("source/folder/nested/file.txt")).lastModified());
        clean(root.filePath("destination"));
    }
    void explicitReplaceAllAndSkipAll()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        for (const auto* name : {"a", "b"})
        {
            write(root.filePath(QString("source/") + name), QByteArray("new-") + name);
            write(root.filePath(QString("destination/") + name), QByteArray("old-") + name);
        }
        int prompts = 0;
        Callbacks skip;
        skip.conflict = [&](const Conflict&)
        { ++prompts; return Decision::SkipAll; };
        auto result = execute({root.filePath("source"), {"a", "b"}, root.filePath("destination")}, skip);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(prompts, 1);
        QCOMPARE(result.skipped.size(), 2);
        QCOMPARE(read(root.filePath("destination/a")), QByteArray("old-a"));
        Callbacks replace;
        prompts = 0;
        replace.conflict = [&](const Conflict& conflict)
        { ++prompts; if (!conflict.replaceAllowed) return Decision::Cancel; return Decision::ReplaceAll; };
        result = execute({root.filePath("source"), {"a", "b"}, root.filePath("destination")}, replace);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(prompts, 1);
        QCOMPARE(result.completed.size(), 2);
        QCOMPARE(read(root.filePath("destination/a")), QByteArray("new-a"));
        QCOMPARE(read(root.filePath("destination/b")), QByteArray("new-b"));
        QCOMPARE(read(root.filePath("source/a")), QByteArray("new-a"));
        clean(root.filePath("destination"));
    }
    void cancelCleansOnlyTheUnfinishedEntry()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        write(root.filePath("source/large"), QByteArray(4 * 1024 * 1024, 'x'));
        bool cancel = false;
        Callbacks callbacks;
        callbacks.canceled = [&]
        { return cancel; };
        callbacks.progress = [&](const Progress& value)
        { if (!value.scanning && value.bytesDone > 0) cancel = true; };
        auto result = execute({root.filePath("source"), {"large"}, root.filePath("destination")}, callbacks);
        QVERIFY(result.canceled);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.cleanupError.isEmpty());
        QVERIFY(!root.exists("destination/large"));
        QCOMPARE(QFileInfo(root.filePath("source/large")).size(), qint64(4 * 1024 * 1024));
        clean(root.filePath("destination"));
        write(root.filePath("source/first"), "complete");
        cancel = false;
        callbacks.progress = [&](const Progress& value)
        { if (value.completed == 1) cancel = true; };
        result = execute({root.filePath("source"), {"first", "large"}, root.filePath("destination")}, callbacks);
        QVERIFY(result.canceled);
        QCOMPARE(result.completed, QStringList{"first"});
        QCOMPARE(read(root.filePath("destination/first")), QByteArray("complete"));
        QVERIFY(!root.exists("destination/large"));
        clean(root.filePath("destination"));
    }
    void directoryCancelDoesNotFollowLinksDuringCleanup()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        QVERIFY(root.mkpath("source/tree/sub"));
        write(root.filePath("precious"), "outside the copy");
        QVERIFY(QFile::link(root.filePath("precious"), root.filePath("source/tree/link")));
        write(root.filePath("source/tree/sub/large"), QByteArray(3 * 1024 * 1024, 'y'));
        bool cancel = false;
        Callbacks callbacks;
        callbacks.canceled = [&]
        { return cancel; };
        callbacks.progress = [&](const Progress& value)
        { if (!value.scanning && value.bytesDone > 0) cancel = true; };
        auto result = execute({root.filePath("source"), {"tree"}, root.filePath("destination")}, callbacks);
        QVERIFY(result.canceled);
        QVERIFY(result.cleanupError.isEmpty());
        QVERIFY(!root.exists("destination/tree"));
        QCOMPARE(read(root.filePath("precious")), QByteArray("outside the copy"));
        clean(root.filePath("destination"));
    }
    void refusesSelfAndDescendantsEvenThroughDirectoryAliases()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        QVERIFY(root.mkpath("source/tree/child"));
        write(root.filePath("source/file"), "original");
        auto result = execute({root.filePath("source"), {"file"}, root.filePath("source")});
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(read(root.filePath("source/file")), QByteArray("original"));
        QVERIFY(QFile::link(root.filePath("source/tree/child"), root.filePath("alias")));
        result = execute({root.filePath("source"), {"tree"}, root.filePath("alias")});
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!root.exists("source/tree/child/tree"));
        clean(root.filePath("source/tree/child"));
    }
    void existingFoldersAreNotMergedOrRemoved()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        QVERIFY(root.mkpath("source/folder"));
        QVERIFY(root.mkpath("destination/folder"));
        write(root.filePath("source/folder/new"), "new");
        write(root.filePath("destination/folder/old"), "old");
        bool offeredReplace = true;
        Callbacks callbacks;
        callbacks.conflict = [&](const Conflict& conflict)
        { offeredReplace = conflict.replaceAllowed; return Decision::Skip; };
        const auto result = execute({root.filePath("source"), {"folder"}, root.filePath("destination")}, callbacks);
        QVERIFY(!offeredReplace);
        QCOMPARE(result.skipped, QStringList{"folder"});
        QVERIFY(root.exists("destination/folder/old"));
        QVERIFY(!root.exists("destination/folder/new"));
    }
    void destinationAppearingDuringCopyIsNotSilentlyReplaced()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        write(root.filePath("source/file"), QByteArray(2 * 1024 * 1024, 'x'));
        bool appeared = false;
        int prompts = 0;
        Callbacks callbacks;
        callbacks.progress = [&](const Progress& value)
        { if (!value.scanning && value.bytesDone > 0 && !appeared) { appeared = true; write(root.filePath("destination/file"), "competitor"); } };
        callbacks.conflict = [&](const Conflict&)
        { ++prompts; return Decision::Skip; };
        const auto result = execute({root.filePath("source"), {"file"}, root.filePath("destination")}, callbacks);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(prompts, 1);
        QCOMPARE(result.skipped, QStringList{"file"});
        QCOMPARE(read(root.filePath("destination/file")), QByteArray("competitor"));
        clean(root.filePath("destination"));
    }
    void changedSourcesAndSpecialFilesAreRejected()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        write(root.filePath("source/file"), QByteArray(2 * 1024 * 1024, 'x'));
        bool changed = false;
        Callbacks callbacks;
        callbacks.progress = [&](const Progress& value)
        {
            if (!value.scanning && value.bytesDone > 0 && !changed)
            {
                changed = true;
                QFile file(root.filePath("source/file"));
                if (file.open(QIODevice::Append))
                    file.write("changed");
            }
        };
        auto result = execute({root.filePath("source"), {"file"}, root.filePath("destination")}, callbacks);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!root.exists("destination/file"));
        clean(root.filePath("destination"));
        QCOMPARE(::mkfifo(QFile::encodeName(root.filePath("source/pipe")).constData(), 0600), 0);
        result = execute({root.filePath("source"), {"pipe"}, root.filePath("destination")});
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!root.exists("destination/pipe"));
    }

    void cancelReplacementPreservesTheOldLinkAndNeverWritesItsTarget()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        write(root.filePath("source/file"), QByteArray(3 * 1024 * 1024, 'z'));
        write(root.filePath("precious"), "old link target");
        QVERIFY(QFile::link(root.filePath("precious"), root.filePath("destination/file")));
        bool cancel = false, wrote = false;
        Callbacks callbacks;
        callbacks.conflict = [](const Conflict&)
        { return Decision::Replace; };
        callbacks.canceled = [&]
        { return cancel; };
        callbacks.progress = [&](const Progress& value)
        { if (!value.scanning && value.bytesDone > 0) { wrote = true; cancel = true; } };
        auto result = execute({root.filePath("source"), {"file"}, root.filePath("destination")}, callbacks);
        QVERIFY(wrote);
        QVERIFY(result.canceled);
        QVERIFY(QFileInfo(root.filePath("destination/file")).isSymbolicLink());
        QCOMPARE(read(root.filePath("precious")), QByteArray("old link target"));
        clean(root.filePath("destination"));
        cancel = false;
        callbacks.progress = {};
        result = execute({root.filePath("source"), {"file"}, root.filePath("destination")}, callbacks);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.completed, QStringList{"file"});
        QVERIFY(!QFileInfo(root.filePath("destination/file")).isSymbolicLink());
        QCOMPARE(QFileInfo(root.filePath("destination/file")).size(), qint64(3 * 1024 * 1024));
        QCOMPARE(read(root.filePath("precious")), QByteArray("old link target"));
        clean(root.filePath("destination"));
    }
    void controllerCanCancelWhileWaitingForAConflict()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        directories(root);
        write(root.filePath("source/file"), "new");
        write(root.filePath("destination/file"), "old");
        CopyController controller;
        QSignalSpy finished(&controller, &CopyController::finished);
        bool conflict = false;
        connect(&controller, &CopyController::conflict, &controller, [&](const Conflict&, quint64 token)
                {
            conflict = true;
            controller.resolve(Decision::Replace, token - 1); // stale answers are ignored
            controller.cancel(); });
        QVERIFY(controller.start({root.filePath("source"), {"file"}, root.filePath("destination")}));
        QVERIFY(!controller.start({root.filePath("source"), {"file"}, root.filePath("destination")}));
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(conflict);
        QVERIFY(qvariant_cast<Result>(finished.first().first()).canceled);
        QCOMPARE(read(root.filePath("destination/file")), QByteArray("old"));
        QVERIFY(!controller.isBusy());
    }
};
QTEST_GUILESS_MAIN(CopyTests)
#include "copy_tests.moc"

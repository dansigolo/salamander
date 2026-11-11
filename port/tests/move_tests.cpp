// SPDX-License-Identifier: GPL-2.0-or-later
#include "copyoperations.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
using namespace CopyOperations;
class MoveTests : public QObject
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
    struct stat info(const QString& path)
    {
        struct stat result
        {
        };
        ::lstat(QFile::encodeName(path).constData(), &result);
        return result;
    }
    QString crossRoot()
    {
        const auto configured = qEnvironmentVariable("SALAMANDER_MOVE_TEST_DESTINATION");
        const QString base = configured.isEmpty() ? "/dev/shm/salamander-move-tests" : configured;
        if (!QDir().mkpath(base))
            return {};
        return base + "/case-XXXXXX";
    }
    void clean(const QString& source, const QString& destination)
    {
        QVERIFY(QDir(source).entryList({".salamander-move-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        QVERIFY(QDir(destination).entryList({".salamander-copy-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
private slots:
    void nativeMovePreservesInodesAndDoesNotNeedToReadDirectoryContents()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QDir directory(root.path());
        QVERIFY(directory.mkdir("source"));
        QVERIFY(directory.mkdir("destination"));
        QVERIFY(directory.mkpath("source/folder"));
        write(root.filePath("source/file"), "content");
        write(root.filePath("source/folder/child"), "nested");
        QVERIFY(QFile::link("missing-target", root.filePath("source/link")));
        auto file = info(root.filePath("source/file"));
        auto folder = info(root.filePath("source/folder"));
        QVERIFY(QFile::setPermissions(root.filePath("source/folder"), QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto result = execute({root.filePath("source"), {"file", "folder", "link"}, root.filePath("destination"), true});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.completed.size(), 3);
        QCOMPARE(info(root.filePath("destination/file")).st_ino, file.st_ino);
        QCOMPARE(info(root.filePath("destination/folder")).st_ino, folder.st_ino);
        QVERIFY(QFile::setPermissions(root.filePath("destination/folder"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QCOMPARE(read(root.filePath("destination/folder/child")), QByteArray("nested"));
        QVERIFY(QFileInfo(root.filePath("destination/link")).isSymbolicLink());
        QVERIFY(!directory.exists("source/file"));
        QVERIFY(!directory.exists("source/folder"));
        QCOMPARE(result.bytesCopied, qint64(0));
        clean(root.filePath("source"), root.filePath("destination"));
    }
    void nativeConflictsSkipReplaceAndCancel()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir("source"));
        QVERIFY(QDir(root.path()).mkdir("destination"));
        write(root.filePath("source/file"), "new");
        write(root.filePath("destination/file"), "old");
        Callbacks callbacks;
        callbacks.conflict = [](const Conflict&)
        { return Decision::Skip; };
        auto result = execute({root.filePath("source"), {"file"}, root.filePath("destination"), true}, callbacks);
        QCOMPARE(result.skipped, QStringList{"file"});
        QCOMPARE(read(root.filePath("source/file")), QByteArray("new"));
        QCOMPARE(read(root.filePath("destination/file")), QByteArray("old"));
        callbacks.conflict = [](const Conflict&)
        { return Decision::Replace; };
        result = execute({root.filePath("source"), {"file"}, root.filePath("destination"), true}, callbacks);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.completed, QStringList{"file"});
        QVERIFY(!QFileInfo::exists(root.filePath("source/file")));
        QCOMPARE(read(root.filePath("destination/file")), QByteArray("new"));
        write(root.filePath("source/second"), "safe");
        callbacks.canceled = []
        { return true; };
        result = execute({root.filePath("source"), {"second"}, root.filePath("destination"), true}, callbacks);
        QVERIFY(result.canceled);
        QVERIFY(QFileInfo::exists(root.filePath("source/second")));
        QVERIFY(!QFileInfo::exists(root.filePath("destination/second")));
    }
    void nativeDirectoryConflictsAndDescendantsAreRefused()
    {
        QTemporaryDir root;
        QDir directory(root.path());
        QVERIFY(directory.mkpath("source/tree/child"));
        QVERIFY(directory.mkpath("target/tree"));
        write(root.filePath("source/tree/keep"), "original");
        write(root.filePath("target/tree/keep"), "existing");
        Callbacks callbacks;
        callbacks.conflict = [](const Conflict& conflict)
        {
            if (conflict.replaceAllowed)
                return Decision::Cancel;
            return Decision::Replace;
        };
        auto result = execute({root.filePath("source"), {"tree"}, root.filePath("target"), true}, callbacks);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(read(root.filePath("target/tree/keep")), QByteArray("existing"));
        result = execute({root.filePath("source"), {"tree"}, root.filePath("source/tree/child"), true});
        QVERIFY(!result.error.isEmpty());
        result = execute({root.filePath("source"), {"tree"}, root.filePath("source"), true});
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(read(root.filePath("source/tree/keep")), QByteArray("original"));
    }
    void crossCancellationDuringRemovalFinishesTheCurrentRoot()
    {
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(info(source.path()).st_dev != info(target.path()).st_dev);
        QVERIFY(QDir(source.path()).mkdir("tree"));
        write(source.filePath("tree/a"), "first");
        write(source.filePath("tree/b"), "second");
        write(source.filePath("next"), "stay");
        bool cancel = false;
        Callbacks callbacks;
        callbacks.canceled = [&]
        { return cancel; };
        callbacks.progress = [&](const Progress& value)
        {
            if (value.phase == Phase::RemovingSource && value.removedEntries > 0)
                cancel = true;
        };
        const auto result = execute({source.path(), {"tree", "next"}, target.path(), true}, callbacks);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.canceled);
        QCOMPARE(result.completed, QStringList{"tree"});
        QVERIFY(!QFileInfo::exists(source.filePath("tree")));
        QCOMPARE(read(target.filePath("tree/a")), QByteArray("first"));
        QCOMPARE(read(target.filePath("tree/b")), QByteArray("second"));
        QCOMPARE(read(source.filePath("next")), QByteArray("stay"));
        QVERIFY(!QFileInfo::exists(target.filePath("next")));
        clean(source.path(), target.path());
    }
    void crossFilesystemFilesFoldersAndDanglingLinks()
    {
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid());
        QVERIFY2(target.isValid(), "A writable cross-filesystem test destination is required");
        QVERIFY2(info(source.path()).st_dev != info(target.path()).st_dev, "Test destination must be on another filesystem");
        QVERIFY(QDir(source.path()).mkpath("folder/sub"));
        write(source.filePath("file.txt"), "whole file");
        write(source.filePath("folder/sub/child"), "nested content");
        QVERIFY(QFile::link("missing-target", source.filePath("broken")));
        const auto result = execute({source.path(), {"file.txt", "folder", "broken"}, target.path(), true});
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error + "\n" + result.cleanupError));
        QCOMPARE(result.completed.size(), 3);
        QVERIFY(result.retained.isEmpty());
        QVERIFY(!QFileInfo::exists(source.filePath("file.txt")));
        QVERIFY(!QFileInfo::exists(source.filePath("folder")));
        QVERIFY(!QFileInfo(source.filePath("broken")).isSymbolicLink());
        QCOMPARE(read(target.filePath("file.txt")), QByteArray("whole file"));
        QCOMPARE(read(target.filePath("folder/sub/child")), QByteArray("nested content"));
        QVERIFY(QFileInfo(target.filePath("broken")).isSymbolicLink());
        clean(source.path(), target.path());
    }
    void crossCancelDuringCopyAndDuringVerification()
    {
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(info(source.path()).st_dev != info(target.path()).st_dev);
        write(source.filePath("large"), QByteArray(3 * 1024 * 1024, 'x'));
        bool cancel = false;
        Callbacks callbacks;
        callbacks.canceled = [&]
        { return cancel; };
        callbacks.progress = [&](const Progress& value)
        { if (value.phase == Phase::Copying && value.bytesDone > 0) cancel = true; };
        auto result = execute({source.path(), {"large"}, target.path(), true}, callbacks);
        QVERIFY(result.canceled);
        QVERIFY(result.error.isEmpty());
        QVERIFY(QFileInfo::exists(source.filePath("large")));
        QVERIFY(!QFileInfo::exists(target.filePath("large")));
        clean(source.path(), target.path());
        cancel = false;
        callbacks.progress = [&](const Progress& value)
        { if (value.phase == Phase::Verifying) cancel = true; };
        result = execute({source.path(), {"large"}, target.path(), true}, callbacks);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.canceled);
        QCOMPARE(result.retained, QStringList{"large"});
        QCOMPARE(QFileInfo(source.filePath("large")).size(), qint64(3 * 1024 * 1024));
        QCOMPARE(QFileInfo(target.filePath("large")).size(), qint64(3 * 1024 * 1024));
        clean(source.path(), target.path());
    }
    void changedSourceIsRestoredAndNameCollisionsNeverOverwriteIt()
    {
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(info(source.path()).st_dev != info(target.path()).st_dev);
        write(source.filePath("file"), QByteArray(2 * 1024 * 1024, 'a'));
        QFile writer(source.filePath("file"));
        QVERIFY(writer.open(QIODevice::Append));
        bool changed = false;
        Callbacks callbacks;
        callbacks.progress = [&](const Progress& value)
        {
            if (!changed && value.phase == Phase::Verifying && value.bytesDone > 0)
            {
                changed = true;
                writer.write("external change");
                writer.flush();
                write(source.filePath("file"), "competing name");
            }
        };
        const auto result = execute({source.path(), {"file"}, target.path(), true}, callbacks);
        QVERIFY(changed);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(read(source.filePath("file")), QByteArray("competing name"));
        QCOMPARE(result.retained, QStringList{"file"});
        QCOMPARE(result.recoveryPaths.size(), 1);
        QVERIFY(read(result.recoveryPaths.first()).endsWith("external change"));
        QCOMPARE(QFileInfo(target.filePath("file")).size(), qint64(2 * 1024 * 1024));
        // The recovery entry is intentionally retained; QTemporaryDir owns it.
    }
    void unsupportedCrossFilesystemMetadataKeepsTheOriginal()
    {
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(info(source.path()).st_dev != info(target.path()).st_dev);
        write(source.filePath("file"), "keep metadata");
        QVERIFY(QFile::link(source.filePath("file"), source.filePath("symbolic")));
        QCOMPARE(::link(QFile::encodeName(source.filePath("file")).constData(), QFile::encodeName(source.filePath("hard")).constData()), 0);
        auto result = execute({source.path(), {"file"}, target.path(), true});
        QVERIFY(!result.error.isEmpty());
        QVERIFY(QFileInfo::exists(source.filePath("file")));
        QVERIFY(!QFileInfo::exists(target.filePath("file")));
        QVERIFY(QFile::remove(source.filePath("hard")));
        const int attributes = ::setxattr(QFile::encodeName(source.filePath("file")).constData(), "user.salamander_test", "value", 5, 0);
        if (attributes != 0)
            QSKIP("Source filesystem does not support test xattrs");
        result = execute({source.path(), {"file"}, target.path(), true});
        QVERIFY(!result.error.isEmpty());
        QVERIFY(QFileInfo::exists(source.filePath("file")));
        QVERIFY(!QFileInfo::exists(target.filePath("file")));
        clean(source.path(), target.path());
    }
    void partialSourceRemovalIsReportedWithoutDeletingUnknownEntries()
    {
        if (::geteuid() == 0)
            QSKIP("Root bypasses source directory permissions");
        QTemporaryDir source;
        QTemporaryDir target(crossRoot());
        QVERIFY(source.isValid() && target.isValid());
        QVERIFY(info(source.path()).st_dev != info(target.path()).st_dev);
        QVERIFY(QDir(source.path()).mkdir("tree"));
        for (const auto* name : {"a", "b", "c"})
            write(source.filePath(QString("tree/") + name), name);
        const int directory = ::open(QFile::encodeName(source.filePath("tree")).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        QVERIFY(directory >= 0);
        bool changed = false;
        Callbacks callbacks;
        callbacks.progress = [&](const Progress& value)
        {
            if (!changed && value.phase == Phase::RemovingSource && value.removedEntries == 1)
            {
                changed = true;
                ::fchmod(directory, 0500);
            }
        };
        const auto result = execute({source.path(), {"tree"}, target.path(), true}, callbacks);
        ::fchmod(directory, 0700);
        ::close(directory);
        QVERIFY(changed);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(result.partialSources, QStringList{"tree"});
        const QString remaining = result.recoveryPaths.isEmpty() ? source.filePath("tree") : result.recoveryPaths.first();
        QVERIFY(QFileInfo(remaining).isDir());
        for (const auto* name : {"a", "b", "c"})
            QCOMPARE(read(target.filePath(QString("tree/") + name)), QByteArray(name));
        QCOMPARE(QDir(remaining).entryList(QDir::Files).size(), 2);
        if (result.recoveryPaths.isEmpty())
            clean(source.path(), target.path());
    }
};
QTEST_GUILESS_MAIN(MoveTests)
#include "move_tests.moc"

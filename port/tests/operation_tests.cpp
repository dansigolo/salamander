// SPDX-License-Identifier: GPL-2.0-or-later
#include "fileoperations.h"
#include "operationcontroller.h"
#include "textpreview.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
using namespace FileOperations;
class OperationTests : public QObject
{
    Q_OBJECT
    bool write(const QString& path, const QByteArray& bytes)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    QByteArray read(const QString& path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
private slots:
    void previewDecodesTextAndKeepsTheOriginalBytes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto file = root.filePath("texto.txt");
        const auto utf16 = QByteArray::fromHex("fffe6100e7000a00");
        QVERIFY(write(file, utf16));
        auto result = TextPreview::read(file);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.text, QString::fromUtf8("aç\n"));
        QCOMPARE(result.encoding, QString("UTF-16"));
        QCOMPARE(read(file), utf16);

        QVERIFY(write(file, QByteArray::fromHex("636166e9")));
        result = TextPreview::read(file);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.text, QString::fromUtf8("café"));
        QCOMPARE(result.encoding, QString("ISO-8859-1"));

        QVERIFY(write(file, {}));
        result = TextPreview::read(file);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.text.isEmpty());
        QVERIFY(!result.truncated);
    }

    void previewLimitsBytesAndRejectsBinaryAndSpecialFiles()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto file = root.filePath("large.txt");
        QByteArray text(TextPreview::byteLimit - 1, 'a');
        text += QString::fromUtf8("ç").toUtf8(); // Character crosses the preview boundary.
        QVERIFY(write(file, text));
        const auto link = root.filePath("link");
        QVERIFY(QFile::link(file, link));
        auto result = TextPreview::read(link);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.truncated);
        QCOMPARE(result.encoding, QString("UTF-8"));
        QCOMPARE(result.text.size(), TextPreview::byteLimit - 1);
        QCOMPARE(result.text.back(), QChar('a'));

        QVERIFY(write(file, QByteArray("binary\0data", 11)));
        QVERIFY(!TextPreview::read(file).error.isEmpty());
        const auto fifo = root.filePath("pipe");
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
        QVERIFY(!TextPreview::read(fifo).error.isEmpty());
        QVERIFY(!TextPreview::read(root.path()).error.isEmpty());
        QVERIFY(!TextPreview::read(root.filePath("missing")).error.isEmpty());
    }

    void createUnicodeFolderAndRefuseAllExistingEntryTypes()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString name = QString::fromUtf8("pasta com acentuação");
        QVERIFY(execute({Kind::CreateDirectory, root.path(), {}, name}).succeeded());
        QVERIFY(QFileInfo(root.filePath(name)).isDir());
        QCOMPARE(execute({Kind::CreateDirectory, root.path(), {}, name}).error, Error::AlreadyExists);
        QVERIFY(write(root.filePath("file"), "unchanged"));
        QCOMPARE(execute({Kind::CreateDirectory, root.path(), {}, "file"}).error, Error::AlreadyExists);
        QCOMPARE(read(root.filePath("file")), QByteArray("unchanged"));
        QVERIFY(QFile::link(root.filePath("missing"), root.filePath("dangling")));
        QCOMPARE(execute({Kind::CreateDirectory, root.path(), {}, "dangling"}).error, Error::AlreadyExists);
        QVERIFY(QFileInfo(root.filePath("dangling")).isSymbolicLink());
    }
    void rejectPathTraversalAndInvalidComponents()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QDir(root.path()).mkdir("scope"));
        const QString scope = root.filePath("scope");
        const QStringList invalid = {QString(), ".", "..", "child/nested", "../escape", root.filePath("absolute"), QString("bad") + QChar::Null + "name"};
        for (const auto& name : invalid)
        {
            QCOMPARE(execute({Kind::CreateDirectory, scope, {}, name}).error, Error::InvalidName);
            QCOMPARE(execute({Kind::Rename, scope, name, "target"}).error, Error::InvalidName);
        }
        QVERIFY(QDir(scope).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
        QVERIFY(execute({Kind::CreateDirectory, root.path(), {}, " leading and trailing "}).succeeded());
        QCOMPARE(execute({Kind::CreateDirectory, QDir::current().relativeFilePath(root.path()), {}, "folder"}).error, Error::InvalidName);
    }
    void trashRejectsInvalidBatchBeforeMovingAnyEntry()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(write(root.filePath("keep"), "original"));
        for (const auto& invalid : {QString(".."), QString("outside/file"), QString("keep")})
        {
            const auto result = execute({Kind::Trash, root.path(), {}, {}, {"keep", invalid}});
            QCOMPARE(result.error, Error::InvalidName);
            QVERIFY(result.trashed.isEmpty());
            QCOMPARE(read(root.filePath("keep")), QByteArray("original"));
        }
    }

    void renameFilesFoldersAndNoOverwrite()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(write(root.filePath("source.txt"), "source content"));
        QVERIFY(write(root.filePath("destination.txt"), "destination content"));
        QCOMPARE(execute({Kind::Rename, root.path(), "source.txt", "destination.txt"}).error, Error::AlreadyExists);
        QCOMPARE(read(root.filePath("source.txt")), QByteArray("source content"));
        QCOMPARE(read(root.filePath("destination.txt")), QByteArray("destination content"));
        const QString target = QString::fromUtf8("novo relatório.txt");
        QVERIFY(execute({Kind::Rename, root.path(), "source.txt", target}).succeeded());
        QCOMPARE(read(root.filePath(target)), QByteArray("source content"));
        QVERIFY(!QFileInfo::exists(root.filePath("source.txt")));
        QVERIFY(QDir(root.path()).mkdir("folder"));
        QVERIFY(write(root.filePath("folder/child"), "inside"));
        QVERIFY(execute({Kind::Rename, root.path(), "folder", "renamed folder"}).succeeded());
        QCOMPARE(read(root.filePath("renamed folder/child")), QByteArray("inside"));
        QVERIFY(execute({Kind::Rename, root.path(), target, target}).succeeded());
        QCOMPARE(execute({Kind::Rename, root.path(), "missing", "missing"}).error, Error::NotFound);
    }
    void renameSymlinkItselfIncludingDanglingSymlinks()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(write(root.filePath("target"), "real target"));
        QVERIFY(QFile::link(root.filePath("target"), root.filePath("link")));
        QVERIFY(execute({Kind::Rename, root.path(), "link", "renamed link"}).succeeded());
        QVERIFY(QFileInfo(root.filePath("renamed link")).isSymbolicLink());
        QCOMPARE(read(root.filePath("target")), QByteArray("real target"));
        QCOMPARE(QFileInfo(root.filePath("renamed link")).symLinkTarget(), root.filePath("target"));
        QVERIFY(QFile::link(root.filePath("missing"), root.filePath("broken")));
        QVERIFY(execute({Kind::Rename, root.path(), "broken", "renamed broken"}).succeeded());
        QVERIFY(QFileInfo(root.filePath("renamed broken")).isSymbolicLink());
        QCOMPARE(execute({Kind::Rename, root.path(), "renamed link", "renamed broken"}).error, Error::AlreadyExists);
    }
    void permissionsAndMissingParent()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QCOMPARE(execute({Kind::CreateDirectory, root.filePath("missing"), {}, "folder"}).error, Error::NotFound);
        if (::geteuid() == 0)
            QSKIP("Root bypasses directory permission checks");
        const auto permissions = QFile::permissions(root.path());
        QVERIFY(QFile::setPermissions(root.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        const auto denied = execute({Kind::CreateDirectory, root.path(), {}, "blocked"});
        QVERIFY(QFile::setPermissions(root.path(), permissions));
        QCOMPARE(denied.error, Error::PermissionDenied);
        QVERIFY(!QFileInfo::exists(root.filePath("blocked")));
    }
    void concurrentRenameNeverReplacesTheWinner()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(write(root.filePath("first"), "one"));
        QVERIFY(write(root.filePath("second"), "two"));
        std::atomic<bool> go{false};
        Result first, second;
        std::thread a([&]
                      { while (!go.load()) std::this_thread::yield(); first = execute({Kind::Rename, root.path(), "first", "winner"}); });
        std::thread b([&]
                      { while (!go.load()) std::this_thread::yield(); second = execute({Kind::Rename, root.path(), "second", "winner"}); });
        go.store(true);
        a.join();
        b.join();
        QVERIFY(first.succeeded() != second.succeeded());
        QCOMPARE(first.succeeded() ? second.error : first.error, Error::AlreadyExists);
        QCOMPARE(read(root.filePath("winner")), first.succeeded() ? QByteArray("one") : QByteArray("two"));
        QVERIFY(QFileInfo::exists(root.filePath(first.succeeded() ? "second" : "first")));
    }
    void controllerIsAsynchronousAndRejectsOverlap()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        OperationController controller;
        QSignalSpy finished(&controller, &OperationController::finished);
        QVERIFY(controller.start({Kind::CreateDirectory, root.path(), {}, "first"}));
        QVERIFY(controller.isBusy());
        QVERIFY(!controller.start({Kind::CreateDirectory, root.path(), {}, "second"}));
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!controller.isBusy());
        const auto result = qvariant_cast<Result>(finished.first().first());
        QVERIFY(result.succeeded());
        QVERIFY(QFileInfo(root.filePath("first")).isDir());
        QVERIFY(!QFileInfo::exists(root.filePath("second")));
    }
};
QTEST_GUILESS_MAIN(OperationTests)
#include "operation_tests.moc"

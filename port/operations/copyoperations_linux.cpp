// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "copyoperations.h"
#include "fileoperations.h"
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QSet>
#include <limits>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <memory>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace CopyOperations
{
    namespace
    {
        struct Fd
        {
            int n = -1;
            explicit Fd(int value) : n(value) {}
            ~Fd()
            {
                if (n >= 0)
                    ::close(n);
            }
            Fd(const Fd&) = delete;
            Fd& operator=(const Fd&) = delete;
        };
        struct Failure
        {
            QString message;
        };
        struct Canceled
        {
        };
        struct DirCloser
        {
            void operator()(DIR* directory) const { ::closedir(directory); }
        };
        using DirectoryStream = std::unique_ptr<DIR, DirCloser>;
        DirectoryStream stream(int fd)
        {
            const int duplicate = ::dup(fd);
            if (duplicate < 0)
                return {};
            DIR* raw = ::fdopendir(duplicate);
            if (!raw)
            {
                const int error = errno;
                ::close(duplicate);
                errno = error;
            }
            return DirectoryStream(raw);
        }
        struct Node
        {
            QByteArray name;
            struct stat metadata
            {
            };
            std::vector<Node> children;
            qint64 bytes = 0;
        };
        bool identity(const struct stat& a, const struct stat& b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
        bool unchanged(const struct stat& a, const struct stat& b)
        {
            return identity(a, b) && a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
        }
        QString nativeError(const QString& context) { return context + ": " + QString::fromLocal8Bit(std::strerror(errno)); }
        struct Job
        {
            const Request& request;
            const Callbacks& callbacks;
            Progress progress;
            Result result;
            bool skipAll = false;
            bool replaceAll = false;
            void canceled()
            {
                if (callbacks.canceled && callbacks.canceled())
                    throw Canceled{};
            }
            void report(const QString& current = {})
            {
                if (!current.isEmpty())
                    progress.current = current;
                if (callbacks.progress)
                    callbacks.progress(progress);
            }
            Node scan(int parent, const QByteArray& name, const QString& display, std::vector<struct stat> ancestors = {})
            {
                canceled();
                report(display);
                Node node;
                node.name = name;
                if (::fstatat(parent, name.constData(), &node.metadata, AT_SYMLINK_NOFOLLOW))
                    throw Failure{nativeError(display)};
                if (S_ISREG(node.metadata.st_mode))
                    node.bytes = node.metadata.st_size;
                else if (S_ISDIR(node.metadata.st_mode))
                {
                    if (ancestors.size() >= 256)
                        throw Failure{QStringLiteral("Directory nesting exceeds 256 levels: ") + display};
                    for (const auto& ancestor : ancestors)
                        if (identity(ancestor, node.metadata))
                            throw Failure{QStringLiteral("Directory cycle detected: ") + display};
                    ancestors.push_back(node.metadata);
                    Fd fd(::openat(parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    if (fd.n < 0)
                        throw Failure{nativeError(display)};
                    struct stat opened;
                    if (::fstat(fd.n, &opened) || !identity(opened, node.metadata))
                        throw Failure{QStringLiteral("Source directory changed: ") + display};
                    auto entries = stream(fd.n);
                    if (!entries)
                        throw Failure{nativeError(display)};
                    while (true)
                    {
                        canceled();
                        errno = 0;
                        const auto* entry = ::readdir(entries.get());
                        if (!entry)
                        {
                            if (errno)
                                throw Failure{nativeError(display)};
                            break;
                        }
                        const QByteArray child(entry->d_name);
                        if (child == "." || child == "..")
                            continue;
                        node.children.push_back(scan(fd.n, child, display + "/" + QFile::decodeName(child), ancestors));
                        if (node.children.back().bytes > std::numeric_limits<qint64>::max() - node.bytes)
                            throw Failure{QStringLiteral("Source size exceeds supported range.")};
                        node.bytes += node.children.back().bytes;
                    }
                }
                else if (!S_ISLNK(node.metadata.st_mode))
                    throw Failure{QStringLiteral("Special files are not supported: ") + display};
                return node;
            }
            void verify(int fd, const Node& node, const QString& display)
            {
                struct stat now;
                if (::fstat(fd, &now) || !unchanged(now, node.metadata))
                    throw Failure{QStringLiteral("Source changed during copy: ") + display};
            }
            void metadata(int fd, const Node& node, const QString& display)
            {
                const timespec times[] = {node.metadata.st_atim, node.metadata.st_mtim};
                if (::fchmod(fd, node.metadata.st_mode & 0777) || ::futimens(fd, times))
                    throw Failure{nativeError(QStringLiteral("Cannot preserve metadata for ") + display)};
            }
            void copy(int sourceParent, const Node& node, int destinationParent, const QByteArray& name, const QString& display)
            {
                canceled();
                report(display);
                if (S_ISREG(node.metadata.st_mode))
                {
                    Fd source(::openat(sourceParent, node.name.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
                    if (source.n < 0)
                        throw Failure{nativeError(display)};
                    struct stat opened;
                    if (::fstat(source.n, &opened) || !S_ISREG(opened.st_mode) || !unchanged(opened, node.metadata))
                        throw Failure{QStringLiteral("Source changed: ") + display};
                    Fd target(::openat(destinationParent, name.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
                    if (target.n < 0)
                        throw Failure{nativeError(display)};
                    std::vector<char> buffer(1024 * 1024);
                    while (true)
                    {
                        canceled();
                        const auto length = ::read(source.n, buffer.data(), buffer.size());
                        if (length < 0)
                        {
                            if (errno == EINTR)
                                continue;
                            throw Failure{nativeError(display)};
                        }
                        if (!length)
                            break;
                        ssize_t written = 0;
                        while (written < length)
                        {
                            canceled();
                            const auto amount = ::write(target.n, buffer.data() + written, length - written);
                            if (amount < 0 && errno == EINTR)
                                continue;
                            if (amount <= 0)
                                throw Failure{nativeError(display)};
                            written += amount;
                            progress.bytesDone += amount;
                            report(display);
                        }
                    }
                    verify(source.n, node, display);
                    metadata(target.n, node, display);
                    if (::fsync(target.n))
                        throw Failure{nativeError(display)};
                }
                else if (S_ISDIR(node.metadata.st_mode))
                {
                    Fd source(::openat(sourceParent, node.name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    if (source.n < 0)
                        throw Failure{nativeError(display)};
                    verify(source.n, node, display);
                    if (::mkdirat(destinationParent, name.constData(), 0700))
                        throw Failure{nativeError(display)};
                    Fd target(::openat(destinationParent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    if (target.n < 0)
                        throw Failure{nativeError(display)};
                    for (const auto& child : node.children)
                        copy(source.n, child, target.n, child.name, display + "/" + QFile::decodeName(child.name));
                    verify(source.n, node, display);
                    metadata(target.n, node, display);
                    if (::fsync(target.n))
                        throw Failure{nativeError(display)};
                }
                else
                {
                    struct stat current;
                    if (::fstatat(sourceParent, node.name.constData(), &current, AT_SYMLINK_NOFOLLOW) || !unchanged(current, node.metadata))
                        throw Failure{QStringLiteral("Source link changed: ") + display};
                    std::vector<char> buffer(256);
                    ssize_t length;
                    while (true)
                    {
                        length = ::readlinkat(sourceParent, node.name.constData(), buffer.data(), buffer.size());
                        if (length < 0)
                            throw Failure{nativeError(display)};
                        if (static_cast<size_t>(length) < buffer.size())
                            break;
                        if (buffer.size() > 1024 * 1024)
                            throw Failure{QStringLiteral("Symbolic link is too long: ") + display};
                        buffer.resize(buffer.size() * 2);
                    }
                    const QByteArray value(buffer.data(), length);
                    if (::symlinkat(value.constData(), destinationParent, name.constData()))
                        throw Failure{nativeError(display)};
                    if (::fstatat(sourceParent, node.name.constData(), &current, AT_SYMLINK_NOFOLLOW) || !unchanged(current, node.metadata))
                        throw Failure{QStringLiteral("Source link changed: ") + display};
                }
            }
            Decision conflict(const Node& node, int destination, const QByteArray& target, const QString& display, bool* present = nullptr)
            {
                if (present)
                    *present = false;
                struct stat existing;
                if (::fstatat(destination, target.constData(), &existing, AT_SYMLINK_NOFOLLOW))
                {
                    if (errno == ENOENT)
                        return Decision::Replace; // absent, but publish still uses NOREPLACE
                    throw Failure{nativeError(display)};
                }
                if (present)
                    *present = true;
                if (identity(existing, node.metadata))
                    throw Failure{QStringLiteral("Cannot copy an item onto itself: ") + display};
                const bool allowed = !S_ISDIR(node.metadata.st_mode) && (S_ISREG(existing.st_mode) || S_ISLNK(existing.st_mode));
                if (skipAll)
                    return Decision::Skip;
                if (replaceAll && allowed)
                    return Decision::Replace;
                const auto answer = callbacks.conflict ? callbacks.conflict({QDir(request.sourceDirectory).filePath(QFile::decodeName(node.name)), display, allowed}) : Decision::Cancel;
                if (answer == Decision::SkipAll)
                {
                    skipAll = true;
                    return Decision::Skip;
                }
                if (answer == Decision::ReplaceAll && allowed)
                {
                    replaceAll = true;
                    return Decision::Replace;
                }
                if ((answer == Decision::Replace || answer == Decision::ReplaceAll) && !allowed)
                    throw Failure{QStringLiteral("Replacing or merging directories is not supported: ") + display};
                return answer;
            }
        };
        bool removeEntry(int parent, const QByteArray& name)
        {
            struct stat info;
            if (::fstatat(parent, name.constData(), &info, AT_SYMLINK_NOFOLLOW))
                return errno == ENOENT;
            if (!S_ISDIR(info.st_mode))
                return ::unlinkat(parent, name.constData(), 0) == 0;
            if (::fchmodat(parent, name.constData(), 0700, AT_SYMLINK_NOFOLLOW))
                return false;
            Fd directory(::openat(parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
            if (directory.n < 0)
                return false;
            auto entries = stream(directory.n);
            if (!entries)
                return false;
            while (true)
            {
                errno = 0;
                const auto* entry = ::readdir(entries.get());
                if (!entry)
                {
                    if (errno)
                        return false;
                    break;
                }
                const QByteArray child(entry->d_name);
                if (child != "." && child != ".." && !removeEntry(directory.n, child))
                    return false;
            }
            return ::unlinkat(parent, name.constData(), AT_REMOVEDIR) == 0;
        }
        void rejectDescendant(int destination, const Node& source)
        {
            if (!S_ISDIR(source.metadata.st_mode))
                return;
            struct stat actualDestination;
            if (::fstat(destination, &actualDestination))
                throw Failure{nativeError("Destination")};
            const auto contains = [&](const auto& self, const Node& node) -> bool
            {
                if (!S_ISDIR(node.metadata.st_mode))
                    return false;
                if (identity(actualDestination, node.metadata))
                    return true;
                for (const auto& child : node.children)
                    if (self(self, child))
                        return true;
                return false;
            };
            if (contains(contains, source))
                throw Failure{QStringLiteral("Destination is inside a selected source directory.")};
            Fd cursor(::dup(destination));
            while (true)
            {
                struct stat current, parent;
                if (::fstat(cursor.n, &current))
                    throw Failure{nativeError("Destination")};
                if (identity(current, source.metadata))
                    throw Failure{QStringLiteral("Destination is inside a selected source directory.")};
                const int next = ::openat(cursor.n, "..", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                if (next < 0)
                    throw Failure{nativeError("Destination parent")};
                if (::fstat(next, &parent))
                {
                    ::close(next);
                    throw Failure{nativeError("Destination parent")};
                }
                ::close(cursor.n);
                cursor.n = next;
                if (identity(current, parent))
                    break;
            }
        }
    } // namespace
    Result execute(const Request& request, const Callbacks& callbacks)
    {
        Job job{request, callbacks, {}, {request}};
        QByteArray stage;
        try
        {
            if (!QDir::isAbsolutePath(request.sourceDirectory) || !QDir::isAbsolutePath(request.destinationDirectory) || request.sourceDirectory.contains(QChar::Null) || request.destinationDirectory.contains(QChar::Null) || request.names.isEmpty())
                throw Failure{QStringLiteral("Choose valid absolute source and destination directories and at least one item.")};
            for (const auto& name : request.names)
                if (!FileOperations::validateName(name).isEmpty())
                    throw Failure{QStringLiteral("Invalid source name: ") + name};
            if (QSet<QString>(request.names.begin(), request.names.end()).size() != request.names.size())
                throw Failure{QStringLiteral("Duplicate source items are not allowed.")};
            Fd source(::open(QFile::encodeName(request.sourceDirectory).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
            if (source.n < 0)
                throw Failure{nativeError("Cannot open source directory")};
            Fd destination(::open(QFile::encodeName(request.destinationDirectory).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
            if (destination.n < 0)
                throw Failure{nativeError("Cannot open destination directory")};
            job.progress.total = request.names.size();
            std::vector<Node> nodes;
            for (const auto& name : request.names)
            {
                nodes.push_back(job.scan(source.n, QFile::encodeName(name), QDir(request.sourceDirectory).filePath(name)));
                rejectDescendant(destination.n, nodes.back());
                if (nodes.back().bytes > std::numeric_limits<qint64>::max() - job.progress.bytesTotal)
                    throw Failure{QStringLiteral("Source size exceeds supported range.")};
                job.progress.bytesTotal += nodes.back().bytes;
            }
            job.progress.scanning = false;
            job.report();
            for (const auto& node : nodes)
            {
                job.canceled();
                const QString name = QFile::decodeName(node.name);
                const QString targetPath = QDir(request.destinationDirectory).filePath(name);
                bool present = false;
                const auto decision = job.conflict(node, destination.n, node.name, targetPath, &present);
                bool replacementApproved = present && decision == Decision::Replace;
                if (decision == Decision::Cancel)
                    throw Canceled{};
                if (decision == Decision::Skip)
                {
                    job.result.skipped.append(name);
                    ++job.progress.skipped;
                    job.progress.bytesDone += node.bytes;
                    job.report(targetPath);
                    continue;
                }
                stage = ".salamander-copy-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
                if (::mkdirat(destination.n, stage.constData(), 0700))
                    throw Failure{nativeError(targetPath)};
                try
                {
                    Fd staging(::openat(destination.n, stage.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    if (staging.n < 0)
                        throw Failure{nativeError(targetPath)};
                    job.copy(source.n, node, staging.n, "entry", QDir(request.sourceDirectory).filePath(name));
                    job.canceled();
                    bool published = false;
                    while (true)
                    {
                        if (::syscall(SYS_renameat2, staging.n, "entry", destination.n, node.name.constData(), RENAME_NOREPLACE) == 0)
                        {
                            published = true;
                            break;
                        }
                        if (errno != EEXIST && errno != ENOTEMPTY)
                            throw Failure{nativeError(targetPath)};
                        const auto answer = replacementApproved ? Decision::Replace : job.conflict(node, destination.n, node.name, targetPath);
                        replacementApproved = false;
                        if (answer == Decision::Cancel)
                            throw Canceled{};
                        if (answer == Decision::Skip)
                            break;
                        job.canceled();
                        struct stat current;
                        if (::fstatat(destination.n, node.name.constData(), &current, AT_SYMLINK_NOFOLLOW))
                        {
                            if (errno == ENOENT)
                                continue;
                            throw Failure{nativeError(targetPath)};
                        }
                        if (identity(current, node.metadata))
                            throw Failure{QStringLiteral("Cannot copy an item onto itself: ") + targetPath};
                        if (S_ISDIR(node.metadata.st_mode) || (!S_ISREG(current.st_mode) && !S_ISLNK(current.st_mode)))
                            throw Failure{QStringLiteral("Destination type changed or cannot be replaced: ") + targetPath};
                        if (::renameat(staging.n, "entry", destination.n, node.name.constData()))
                            throw Failure{nativeError(targetPath)};
                        published = true;
                        break;
                    }
                    if (!published)
                    {
                        job.result.skipped.append(name);
                        ++job.progress.skipped;
                    }
                    else
                    {
                        job.result.completed.append(name);
                        ++job.progress.completed;
                        job.result.bytesCopied += node.bytes;
                        if (::fsync(destination.n))
                            throw Failure{nativeError(QStringLiteral("Cannot sync destination ") + targetPath)};
                    }
                }
                catch (...)
                {
                    if (!removeEntry(destination.n, stage))
                        job.result.cleanupError = QStringLiteral("Could not remove unfinished copy: ") + QDir(request.destinationDirectory).filePath(QFile::decodeName(stage));
                    stage.clear();
                    throw;
                }
                if (!removeEntry(destination.n, stage))
                    throw Failure{QStringLiteral("Could not remove temporary copy directory: ") + QDir(request.destinationDirectory).filePath(QFile::decodeName(stage))};
                stage.clear();
                job.report(targetPath);
            }
        }
        catch (const Canceled&)
        {
            job.result.canceled = true;
        }
        catch (const Failure& error)
        {
            job.result.error = error.message;
        }
        catch (const std::exception& error)
        {
            job.result.error = QString::fromLocal8Bit(error.what());
        }
        return job.result;
    }
} // namespace CopyOperations

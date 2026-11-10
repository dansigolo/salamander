// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "fileoperations.h"
#include <QDir>
#include <QFile>
#include <cerrno>
#include <system_error>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace FileOperations
{
    namespace
    {
        struct DirectoryHandle
        {
            int value;
            ~DirectoryHandle()
            {
                if (value >= 0)
                    ::close(value);
            }
        };
        Result failure(const Request& request, int error)
        {
            Error category = Error::Io;
            switch (error)
            {
            case EEXIST:
            case ENOTEMPTY:
                category = Error::AlreadyExists;
                break;
            case ENOENT:
            case ENOTDIR:
                category = Error::NotFound;
                break;
            case EACCES:
            case EPERM:
            case EROFS:
                category = Error::PermissionDenied;
                break;
            case ENAMETOOLONG:
                category = Error::InvalidName;
                break;
            case ENOSYS:
            case EINVAL:
            case EOPNOTSUPP:
                category = Error::Unsupported;
                break;
            }
            const auto message = std::system_category().message(error);
            return {request, category, QString::fromLocal8Bit(message.data(), message.size())};
        }
    } // namespace
    QString validateName(const QString& name)
    {
        if (name.isEmpty())
            return QStringLiteral("Enter a name.");
        if (name == "." || name == "..")
            return QStringLiteral("The names . and .. are reserved.");
        if (name.contains('/') || name.contains(QChar::Null))
            return QStringLiteral("Use a single name without / or NUL characters.");
        if (QString::fromUtf8(name.toUtf8()) != name)
            return QStringLiteral("The name contains invalid Unicode.");
        return {};
    }
    Result execute(const Request& request)
    {
        QString invalid = validateName(request.targetName);
        if (invalid.isEmpty() && request.kind == Kind::Rename)
            invalid = validateName(request.sourceName);
        if (invalid.isEmpty() && (!QDir::isAbsolutePath(request.directory) || request.directory.contains(QChar::Null)))
            invalid = QStringLiteral("The parent directory must be an absolute path without NUL characters.");
        if (!invalid.isEmpty())
            return {request, Error::InvalidName, invalid};
        const auto directory = QFile::encodeName(request.directory);
        DirectoryHandle handle{::open(directory.constData(), O_PATH | O_DIRECTORY | O_CLOEXEC)};
        if (handle.value < 0)
            return failure(request, errno);
        const auto target = QFile::encodeName(request.targetName);
        if (request.kind == Kind::CreateDirectory)
        {
            if (::mkdirat(handle.value, target.constData(), 0777) != 0)
                return failure(request, errno);
        }
        else
        {
            const auto source = QFile::encodeName(request.sourceName);
            if (source == target)
            {
                struct stat metadata;
                if (::fstatat(handle.value, source.constData(), &metadata, AT_SYMLINK_NOFOLLOW) != 0)
                    return failure(request, errno);
                return {request, Error::None, {}};
            }
            // No QFile::rename fallback: its copy/remove behavior would not
            // provide the atomic, non-replacing operation required here.
            if (::syscall(SYS_renameat2, handle.value, source.constData(), handle.value, target.constData(), RENAME_NOREPLACE) != 0)
                return failure(request, errno);
        }
        return {request, Error::None, {}};
    }
} // namespace FileOperations

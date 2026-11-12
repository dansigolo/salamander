// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QStringList>
#include <QMetaType>
namespace FileOperations
{
    enum class Kind
    {
        CreateDirectory,
        Rename,
        Trash
    };
    enum class Error
    {
        None,
        InvalidName,
        NotFound,
        AlreadyExists,
        PermissionDenied,
        Unsupported,
        Io
    };
    struct Request
    {
        Kind kind = Kind::CreateDirectory;
        QString directory;
        QString sourceName;
        QString targetName;
        QStringList names; // Trash batch, in operation order.
    };
    struct Result
    {
        Request request;
        Error error = Error::None;
        QString message;
        QStringList trashed;
        QString failedName;
        bool succeeded() const { return error == Error::None; }
    };
    // Names are single Linux path components; whitespace is significant.
    QString validateName(const QString& name);
    // Runs on a worker. Parent directory is opened once; rename never follows
    // the source symlink and never replaces an existing destination entry.
    Result execute(const Request& request);
} // namespace FileOperations
Q_DECLARE_METATYPE(FileOperations::Result)

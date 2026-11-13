// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>

class QFile;

namespace TextPreview
{
    inline constexpr qsizetype byteLimit = 1024 * 1024;

    struct Result
    {
        QString text;
        QString encoding;
        QString error;
        bool truncated = false;
    };

    // Read-only, bounded I/O. Call on a worker thread.
    QString openRegularFile(const QString& path, QFile& file);
    Result read(QFile& file);
    Result read(const QString& path);
} // namespace TextPreview

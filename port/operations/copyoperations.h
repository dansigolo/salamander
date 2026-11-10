// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
#include <QMetaType>
#include <functional>
namespace CopyOperations
{
    struct Request
    {
        QString sourceDirectory;
        QStringList names;
        QString destinationDirectory;
    };
    enum class Decision
    {
        Skip,
        SkipAll,
        Replace,
        ReplaceAll,
        Cancel
    };
    struct Conflict
    {
        QString source;
        QString destination;
        bool replaceAllowed = false;
    };
    struct Progress
    {
        bool scanning = true;
        QString current;
        qint64 bytesDone = 0;
        qint64 bytesTotal = 0;
        int completed = 0;
        int skipped = 0;
        int total = 0;
    };
    struct Result
    {
        Request request;
        QStringList completed;
        QStringList skipped;
        qint64 bytesCopied = 0;
        bool canceled = false;
        QString error;
        QString cleanupError;
    };
    struct Callbacks
    {
        std::function<bool()> canceled;
        std::function<void(const Progress&)> progress;
        std::function<Decision(const Conflict&)> conflict;
    };
    // Linux worker-only entry point. Sources are never deleted. Each selected
    // entry is staged privately and published atomically after it is complete.
    Result execute(const Request& request, const Callbacks& callbacks = {});
} // namespace CopyOperations
Q_DECLARE_METATYPE(CopyOperations::Progress)
Q_DECLARE_METATYPE(CopyOperations::Conflict)
Q_DECLARE_METATYPE(CopyOperations::Result)

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
        bool move = false;
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
    enum class Phase
    {
        Copying,
        Moving,
        Verifying,
        RemovingSource
    };
    struct PublishedEntry
    {
        QString name;
        quint64 device = 0;
        quint64 inode = 0;
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
        Phase phase = Phase::Copying;
        int removedEntries = 0;
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
        QList<PublishedEntry> published;
        QStringList retained;
        QStringList partialSources;
        QStringList recoveryPaths;
    };
    struct Callbacks
    {
        std::function<bool()> canceled;
        std::function<void(const Progress&)> progress;
        std::function<Decision(const Conflict&)> conflict;
    };
    // Worker-only transfer entry point. Copy preserves sources; move uses native
    // rename or a verified cross-filesystem copy/removal pipeline.
    Result execute(const Request& request, const Callbacks& callbacks = {});
} // namespace CopyOperations
Q_DECLARE_METATYPE(CopyOperations::Progress)
Q_DECLARE_METATYPE(CopyOperations::Conflict)
Q_DECLARE_METATYPE(CopyOperations::Result)

// SPDX-License-Identifier: GPL-2.0-or-later
// Private part of the native transfer engine, included in its anonymous namespace.
bool sameExceptChangeTime(const struct stat& a, const struct stat& b)
{
    return identity(a, b) && a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec && a.st_mode == b.st_mode && a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_nlink == b.st_nlink;
}
void requireIdentity(int parent, const QByteArray& name, const struct stat& expected, const QString& display)
{
    struct stat actual;
    if (::fstatat(parent, name.constData(), &actual, AT_SYMLINK_NOFOLLOW) || !identity(actual, expected))
        throw Failure{QStringLiteral("Entry changed: ") + display};
}
QByteArray linkValue(int parent, const QByteArray& name, const QString& display)
{
    std::vector<char> bytes(256);
    while (true)
    {
        const auto length = ::readlinkat(parent, name.constData(), bytes.data(), bytes.size());
        if (length < 0)
            throw Failure{nativeError(display)};
        if (size_t(length) < bytes.size())
            return QByteArray(bytes.data(), length);
        if (bytes.size() > 1024 * 1024)
            throw Failure{QStringLiteral("Link is too long: ") + display};
        bytes.resize(bytes.size() * 2);
    }
}
void noExtendedMetadata(int fd, const QString& display)
{
    const auto length = ::flistxattr(fd, nullptr, 0);
    if (length > 0)
        throw Failure{QStringLiteral("Cross-filesystem moves of entries with ACLs or extended attributes are not supported yet: ") + display};
    if (length < 0 && errno != ENOTSUP && errno != EOPNOTSUPP)
        throw Failure{nativeError(display)};
}
void noLinkMetadata(int parent, const QByteArray& name, const QString& display)
{
    const auto path = QByteArray("/proc/self/fd/") + QByteArray::number(parent) + "/" + name;
    const auto length = ::llistxattr(path.constData(), nullptr, 0);
    if (length > 0)
        throw Failure{QStringLiteral("Cross-filesystem moves of links with extended attributes are not supported yet: ") + display};
    if (length < 0 && errno != ENOTSUP && errno != EOPNOTSUPP)
        throw Failure{nativeError(display)};
}
void crossMoveRequirements(Job& job, int parent, const Node& node, const QString& display)
{
    job.canceled();
    if ((node.metadata.st_mode & 07000) || (::geteuid() != 0 && node.metadata.st_uid != ::geteuid()))
        throw Failure{QStringLiteral("Cross-filesystem move cannot preserve this entry's ownership or special permissions: ") + display};
    if (!S_ISDIR(node.metadata.st_mode) && node.metadata.st_nlink != 1)
        throw Failure{QStringLiteral("Cross-filesystem moves of hard-linked entries are not supported yet: ") + display};
    if (S_ISLNK(node.metadata.st_mode))
    {
        noLinkMetadata(parent, node.name, display);
        return;
    }
    Fd source(::openat(parent, node.name.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | (S_ISDIR(node.metadata.st_mode) ? O_DIRECTORY : 0)));
    if (source.n < 0)
        throw Failure{nativeError(display)};
    struct stat actual;
    if (::fstat(source.n, &actual) || !unchanged(actual, node.metadata))
        throw Failure{QStringLiteral("Source changed: ") + display};
    noExtendedMetadata(source.n, display);
    if (S_ISDIR(node.metadata.st_mode))
    {
        if (::faccessat(source.n, ".", W_OK | X_OK, AT_EACCESS))
            throw Failure{nativeError(QStringLiteral("Source directory cannot be cleaned safely: ") + display)};
        for (const auto& child : node.children)
            crossMoveRequirements(job, source.n, child, display + "/" + QFile::decodeName(child.name));
    }
}
QSet<QByteArray> childNames(int fd, const QString& display)
{
    // Open a new description so a previous readdir offset cannot hide entries.
    Fd fresh(::openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (fresh.n < 0)
        throw Failure{nativeError(display)};
    auto entries = stream(fresh.n);
    if (!entries)
        throw Failure{nativeError(display)};
    QSet<QByteArray> names;
    while (true)
    {
        errno = 0;
        const auto* entry = ::readdir(entries.get());
        if (!entry)
        {
            if (errno)
                throw Failure{nativeError(display)};
            break;
        }
        const QByteArray name(entry->d_name);
        if (name != "." && name != "..")
            names.insert(name);
    }
    return names;
}
void moveMetadata(int fd, const Node& node, const QString& display, bool link = false)
{
    const timespec times[] = {node.metadata.st_atim, node.metadata.st_mtim};
    if (link)
    {
        if (::fchownat(fd, "", node.metadata.st_uid, node.metadata.st_gid, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW) || ::utimensat(fd, "", times, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
            throw Failure{nativeError(QStringLiteral("Cannot preserve link metadata: ") + display)};
    }
    else
    {
        if (::fchown(fd, node.metadata.st_uid, node.metadata.st_gid) || ::fchmod(fd, node.metadata.st_mode & 0777) || ::futimens(fd, times))
            throw Failure{nativeError(QStringLiteral("Cannot preserve move metadata: ") + display)};
    }
    struct stat actual;
    if (::fstat(fd, &actual) || actual.st_uid != node.metadata.st_uid || actual.st_gid != node.metadata.st_gid || (actual.st_mode & 07777) != (node.metadata.st_mode & 07777) || actual.st_mtim.tv_sec != node.metadata.st_mtim.tv_sec || actual.st_mtim.tv_nsec != node.metadata.st_mtim.tv_nsec)
        throw Failure{QStringLiteral("Destination cannot preserve source metadata; source will be retained: ") + display};
}
void verifyMove(Job& job, int sourceParent, Node& node, int destinationParent, const QByteArray& destinationName, const QString& display)
{
    job.canceled();
    struct stat sourceInfo, destinationInfo;
    if (::fstatat(sourceParent, node.name.constData(), &sourceInfo, AT_SYMLINK_NOFOLLOW) || !unchanged(sourceInfo, node.metadata))
        throw Failure{QStringLiteral("Source changed before removal: ") + display};
    if (::fstatat(destinationParent, destinationName.constData(), &destinationInfo, AT_SYMLINK_NOFOLLOW) || (sourceInfo.st_mode & S_IFMT) != (destinationInfo.st_mode & S_IFMT))
        throw Failure{QStringLiteral("Destination changed before verification: ") + display};
    if (node.destinationMetadata.st_ino && !identity(destinationInfo, node.destinationMetadata))
        throw Failure{QStringLiteral("Destination identity changed before verification: ") + display};
    if (S_ISLNK(sourceInfo.st_mode))
    {
        noLinkMetadata(sourceParent, node.name, display);
        noLinkMetadata(destinationParent, destinationName, display);
        if (linkValue(sourceParent, node.name, display) != linkValue(destinationParent, destinationName, display))
            throw Failure{QStringLiteral("Copied link does not match: ") + display};
        Fd target(::openat(destinationParent, destinationName.constData(), O_PATH | O_NOFOLLOW | O_CLOEXEC));
        struct stat actual;
        if (target.n < 0 || ::fstat(target.n, &actual) || !identity(actual, destinationInfo))
            throw Failure{QStringLiteral("Destination link changed: ") + display};
        moveMetadata(target.n, node, display, true);
        if (::fstat(target.n, &node.destinationMetadata))
            throw Failure{nativeError(display)};
    }
    else
    {
        const int flags = O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC | (S_ISDIR(sourceInfo.st_mode) ? O_DIRECTORY : 0);
        Fd source(::openat(sourceParent, node.name.constData(), flags));
        Fd target(::openat(destinationParent, destinationName.constData(), flags));
        struct stat actualSource, actualTarget;
        if (source.n < 0 || target.n < 0 || ::fstat(source.n, &actualSource) || ::fstat(target.n, &actualTarget) || !identity(actualSource, sourceInfo) || !identity(actualTarget, destinationInfo))
            throw Failure{QStringLiteral("Entry changed during verification: ") + display};
        noExtendedMetadata(source.n, display);
        noExtendedMetadata(target.n, display);
        if (S_ISDIR(sourceInfo.st_mode))
        {
            QSet<QByteArray> expected;
            for (const auto& child : node.children)
                expected.insert(child.name);
            if (childNames(source.n, display) != expected || childNames(target.n, display) != expected)
                throw Failure{QStringLiteral("Directory contents changed: ") + display};
            for (auto& child : node.children)
                verifyMove(job, source.n, child, target.n, child.name, display + "/" + QFile::decodeName(child.name));
        }
        else
        {
            std::vector<char> a(1024 * 1024), b(a.size());
            while (true)
            {
                job.canceled();
                ssize_t size;
                do
                {
                    size = ::read(source.n, a.data(), a.size());
                } while (size < 0 && errno == EINTR);
                if (size < 0)
                    throw Failure{nativeError(display)};
                if (!size)
                {
                    char extra;
                    ssize_t end;
                    do
                    {
                        end = ::read(target.n, &extra, 1);
                    } while (end < 0 && errno == EINTR);
                    if (end != 0)
                        throw Failure{QStringLiteral("Destination size does not match: ") + display};
                    break;
                }
                ssize_t read = 0;
                while (read < size)
                {
                    job.canceled();
                    const auto amount = ::read(target.n, b.data() + read, size - read);
                    if (amount < 0 && errno == EINTR)
                        continue;
                    if (amount <= 0)
                        throw Failure{QStringLiteral("Destination read failed: ") + display};
                    read += amount;
                }
                if (std::memcmp(a.data(), b.data(), size) != 0)
                    throw Failure{QStringLiteral("Copied bytes do not match: ") + display};
                job.progress.bytesDone += size;
                job.report(display);
            }
        }
        if (::fstat(source.n, &actualSource) || !unchanged(actualSource, node.metadata))
            throw Failure{QStringLiteral("Source changed during verification: ") + display};
        if (::fstat(target.n, &actualTarget) || !unchanged(actualTarget, destinationInfo))
            throw Failure{QStringLiteral("Destination changed during verification: ") + display};
        moveMetadata(target.n, node, display);
        if (::fsync(target.n) || ::fstat(target.n, &node.destinationMetadata))
            throw Failure{nativeError(display)};
    }
    requireIdentity(sourceParent, node.name, node.metadata, display);
    requireIdentity(destinationParent, destinationName, node.destinationMetadata, display);
}
void removeVerifiedSource(Job& job, int sourceParent, const Node& node, int destinationParent, const QByteArray& destinationName, const QString& display, bool& removed)
{
    struct stat sourceInfo, destinationInfo;
    if (::fstatat(sourceParent, node.name.constData(), &sourceInfo, AT_SYMLINK_NOFOLLOW) || !unchanged(sourceInfo, node.metadata))
        throw Failure{QStringLiteral("Source identity changed during removal: ") + display};
    if (::fstatat(destinationParent, destinationName.constData(), &destinationInfo, AT_SYMLINK_NOFOLLOW) || !unchanged(destinationInfo, node.destinationMetadata))
        throw Failure{QStringLiteral("Verified destination changed; remaining source retained: ") + display};
    if (S_ISDIR(sourceInfo.st_mode))
    {
        Fd source(::openat(sourceParent, node.name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        Fd target(::openat(destinationParent, destinationName.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        if (source.n < 0 || target.n < 0)
            throw Failure{nativeError(display)};
        for (const auto& child : node.children)
            removeVerifiedSource(job, source.n, child, target.n, child.name, display + "/" + QFile::decodeName(child.name), removed);
        requireIdentity(sourceParent, node.name, node.metadata, display);
        if (::unlinkat(sourceParent, node.name.constData(), AT_REMOVEDIR))
            throw Failure{nativeError(display)};
    }
    else
    {
        if (!unchanged(sourceInfo, node.metadata))
            throw Failure{QStringLiteral("Source changed before unlink: ") + display};
        if (::unlinkat(sourceParent, node.name.constData(), 0))
            throw Failure{nativeError(display)};
    }
    removed = true;
    ++job.progress.removedEntries;
    job.report(display); // cancellation takes effect after the current verified root completes
}
Result executeMove(const Request& request, const Callbacks& callbacks)
{
    Job job{request, callbacks, {}, {request}};
    try
    {
        if (!QDir::isAbsolutePath(request.sourceDirectory) || !QDir::isAbsolutePath(request.destinationDirectory) || request.sourceDirectory.contains(QChar::Null) || request.destinationDirectory.contains(QChar::Null) || request.names.isEmpty())
            throw Failure{QStringLiteral("Choose valid absolute source and destination directories.")};
        for (const auto& name : request.names)
            if (!FileOperations::validateName(name).isEmpty())
                throw Failure{QStringLiteral("Invalid source name: ") + name};
        if (QSet<QString>(request.names.begin(), request.names.end()).size() != request.names.size())
            throw Failure{QStringLiteral("Duplicate source names.")};
        Fd source(::open(QFile::encodeName(request.sourceDirectory).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (source.n < 0)
            throw Failure{nativeError(request.sourceDirectory)};
        Fd destination(::open(QFile::encodeName(request.destinationDirectory).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (destination.n < 0)
            throw Failure{nativeError(request.destinationDirectory)};
        job.progress.total = request.names.size();
        std::vector<Node> entries;
        for (const auto& name : request.names)
        {
            job.canceled();
            Node node;
            node.name = QFile::encodeName(name);
            if (::fstatat(source.n, node.name.constData(), &node.metadata, AT_SYMLINK_NOFOLLOW))
                throw Failure{nativeError(name)};
            if (!S_ISREG(node.metadata.st_mode) && !S_ISDIR(node.metadata.st_mode) && !S_ISLNK(node.metadata.st_mode))
                throw Failure{QStringLiteral("Special entries are not supported: ") + name};
            rejectDescendant(destination.n, node);
            entries.push_back(std::move(node));
        }
        job.progress.scanning = false;
        for (auto& initial : entries)
        {
            job.canceled();
            const QString name = QFile::decodeName(initial.name);
            const QString sourcePath = QDir(request.sourceDirectory).filePath(name);
            const QString destinationPath = QDir(request.destinationDirectory).filePath(name);
            job.progress.phase = Phase::Moving;
            job.progress.bytesDone = job.progress.bytesTotal = 0;
            job.report(sourcePath);
            bool present = false;
            const auto decision = job.conflict(initial, destination.n, initial.name, destinationPath, &present);
            if (decision == Decision::Cancel)
                throw Canceled{};
            if (decision == Decision::Skip)
            {
                job.result.skipped.append(name);
                ++job.progress.skipped;
                continue;
            }
            bool approved = present && decision == Decision::Replace;
            bool cross = false, moved = false;
            while (true)
            {
                job.canceled();
                requireIdentity(source.n, initial.name, initial.metadata, sourcePath);
                if (::syscall(SYS_renameat2, source.n, initial.name.constData(), destination.n, initial.name.constData(), RENAME_NOREPLACE) == 0)
                {
                    moved = true;
                    break;
                }
                const int code = errno;
                if (code == EXDEV)
                {
                    cross = true;
                    break;
                }
                if (code != EEXIST && code != ENOTEMPTY)
                    throw Failure{nativeError(sourcePath)};
                const auto answer = approved ? Decision::Replace : job.conflict(initial, destination.n, initial.name, destinationPath);
                approved = false;
                if (answer == Decision::Cancel)
                    throw Canceled{};
                if (answer == Decision::Skip)
                    break;
                job.canceled();
                struct stat target;
                if (::fstatat(destination.n, initial.name.constData(), &target, AT_SYMLINK_NOFOLLOW))
                {
                    if (errno == ENOENT)
                        continue;
                    throw Failure{nativeError(destinationPath)};
                }
                if (identity(target, initial.metadata) || S_ISDIR(initial.metadata.st_mode) || (!S_ISREG(target.st_mode) && !S_ISLNK(target.st_mode)))
                    throw Failure{QStringLiteral("Destination changed or cannot be replaced: ") + destinationPath};
                requireIdentity(source.n, initial.name, initial.metadata, sourcePath);
                if (::renameat(source.n, initial.name.constData(), destination.n, initial.name.constData()) == 0)
                {
                    moved = true;
                    break;
                }
                if (errno == EXDEV)
                {
                    cross = true;
                    approved = true;
                    break;
                }
                throw Failure{nativeError(sourcePath)};
            }
            if (moved)
            {
                job.result.completed.append(name);
                job.result.published.append({name, quint64(initial.metadata.st_dev), quint64(initial.metadata.st_ino)});
                ++job.progress.completed;
                if (::fsync(destination.n) || ::fsync(source.n))
                    throw Failure{nativeError(QStringLiteral("Move completed but directory sync failed: ") + name)};
                job.report(destinationPath);
                continue;
            }
            if (!cross)
            {
                job.result.skipped.append(name);
                ++job.progress.skipped;
                continue;
            }
            job.progress.scanning = true;
            auto node = job.scan(source.n, initial.name, sourcePath);
            if (!identity(node.metadata, initial.metadata))
                throw Failure{QStringLiteral("Source identity changed: ") + sourcePath};
            rejectDescendant(destination.n, node);
            crossMoveRequirements(job, source.n, node, sourcePath);
            Callbacks sub;
            sub.canceled = callbacks.canceled;
            sub.progress = [&](const Progress& value)
            {
                job.progress.scanning = value.scanning;
                job.progress.phase = Phase::Copying;
                job.progress.bytesDone = value.bytesDone;
                job.progress.bytesTotal = value.bytesTotal;
                job.report(value.current);
            };
            bool firstApproval = approved;
            sub.conflict = [&](const Conflict& value)
            {
                if (firstApproval && value.replaceAllowed)
                {
                    firstApproval = false;
                    return Decision::Replace;
                }
                firstApproval = false;
                return job.choose(value);
            };
            const auto copied = execute({request.sourceDirectory, {name}, request.destinationDirectory, false}, sub);
            job.result.bytesCopied += copied.bytesCopied;
            if (copied.completed.contains(name))
                job.result.retained.append(name);
            if (!copied.cleanupError.isEmpty())
                job.result.cleanupError += copied.cleanupError + "\n";
            if (!copied.error.isEmpty())
                throw Failure{copied.error};
            if (copied.canceled)
                throw Canceled{};
            if (copied.skipped.contains(name))
            {
                job.result.skipped.append(name);
                ++job.progress.skipped;
                continue;
            }
            if (copied.published.size() != 1)
                throw Failure{QStringLiteral("Copied destination identity was not recorded.")};
            const auto published = copied.published.first();
            struct stat destinationIdentity;
            if (::fstatat(destination.n, node.name.constData(), &destinationIdentity, AT_SYMLINK_NOFOLLOW) || quint64(destinationIdentity.st_dev) != published.device || quint64(destinationIdentity.st_ino) != published.inode)
                throw Failure{QStringLiteral("Destination was replaced after copy; source retained: ") + destinationPath};
            job.canceled();
            const QByteArray wrapper = ".salamander-move-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
            if (::mkdirat(source.n, wrapper.constData(), 0700))
                throw Failure{nativeError(sourcePath)};
            bool quarantined = false, removed = false;
            try
            {
                Fd holding(::openat(source.n, wrapper.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                if (holding.n < 0)
                    throw Failure{nativeError(sourcePath)};
                requireIdentity(source.n, node.name, node.metadata, sourcePath);
                job.canceled();
                if (::syscall(SYS_renameat2, source.n, node.name.constData(), holding.n, "entry", RENAME_NOREPLACE))
                    throw Failure{nativeError(sourcePath)};
                quarantined = true;
                struct stat held;
                if (::fstatat(holding.n, "entry", &held, AT_SYMLINK_NOFOLLOW) || !sameExceptChangeTime(held, node.metadata))
                    throw Failure{QStringLiteral("Source changed while preparing removal: ") + sourcePath};
                node.name = "entry";
                node.metadata = held; // rename changes only the root change timestamp
                job.progress.scanning = false;
                job.progress.phase = Phase::Verifying;
                job.progress.bytesDone = 0;
                job.progress.bytesTotal = node.bytes;
                job.report(sourcePath);
                crossMoveRequirements(job, holding.n, node, sourcePath);
                node.destinationMetadata = destinationIdentity;
                verifyMove(job, holding.n, node, destination.n, initial.name, sourcePath);
                requireIdentity(destination.n, initial.name, destinationIdentity, destinationPath);
                job.canceled();
                job.progress.phase = Phase::RemovingSource;
                job.progress.removedEntries = 0;
                job.report(sourcePath);
                // Cancellation after this boundary stops the next root. Do not
                // abandon the current root halfway through source removal.
                removeVerifiedSource(job, holding.n, node, destination.n, initial.name, sourcePath, removed);
                quarantined = false;
                job.result.retained.removeAll(name);
                job.result.completed.append(name);
                job.result.published.append(published);
                ++job.progress.completed;
                if (::fsync(source.n))
                    throw Failure{nativeError(QStringLiteral("Source removal completed but sync failed: ") + sourcePath)};
            }
            catch (...)
            {
                if (quarantined)
                {
                    Fd holding(::openat(source.n, wrapper.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
                    if (removed)
                        job.result.partialSources.append(name);
                    if (holding.n < 0 || ::syscall(SYS_renameat2, holding.n, "entry", source.n, initial.name.constData(), RENAME_NOREPLACE))
                        job.result.recoveryPaths.append(QDir(request.sourceDirectory).filePath(QFile::decodeName(wrapper) + "/entry"));
                    else
                        quarantined = false;
                }
                if (!quarantined && ::unlinkat(source.n, wrapper.constData(), AT_REMOVEDIR))
                    job.result.cleanupError += QStringLiteral("Could not remove move wrapper: ") + QDir(request.sourceDirectory).filePath(QFile::decodeName(wrapper)) + "\n";
                throw;
            }
            if (::unlinkat(source.n, wrapper.constData(), AT_REMOVEDIR))
                throw Failure{QStringLiteral("Move completed but wrapper cleanup failed: ") + QDir(request.sourceDirectory).filePath(QFile::decodeName(wrapper))};
            job.report(destinationPath);
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

// SPDX-License-Identifier: GPL-2.0-or-later
#include "textpreview.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringDecoder>
#include <algorithm>
#include <cerrno>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace TextPreview
{
    namespace
    {
        struct Descriptor
        {
            int fd;
            ~Descriptor()
            {
                if (fd >= 0)
                    ::close(fd);
            }
        };

        Result failure(QString message)
        {
            Result result;
            result.error = std::move(message);
            return result;
        }

        Result nativeFailure(int error)
        {
            const auto message = std::system_category().message(error);
            return failure(QString::fromLocal8Bit(message.data(), message.size()));
        }

        Result decode(QByteArray bytes)
        {
            Result result;
            result.truncated = bytes.size() > byteLimit;
            if (result.truncated)
                bytes.truncate(byteLimit);

            using Encoding = QStringConverter::Encoding;
            const bool utf16 = bytes.startsWith("\xff\xfe") || bytes.startsWith("\xfe\xff");
            const bool utf8Bom = bytes.startsWith("\xef\xbb\xbf");
            const auto encoding = utf16 ? Encoding::Utf16 : Encoding::Utf8;
            // A bounded preview can end in the middle of a character.
            const auto flags = result.truncated ? QStringConverter::Flag::Default : QStringConverter::Flag::Stateless;
            QStringDecoder decoder(encoding, flags);
            result.text = decoder(bytes);
            result.encoding = utf16 ? QStringLiteral("UTF-16") : QStringLiteral("UTF-8");
            if (decoder.hasError())
            {
                if (utf16 || utf8Bom)
                    return failure(QCoreApplication::translate("TextPreview", "This file contains invalid Unicode text."));
                result.text = QString::fromLatin1(bytes);
                result.encoding = QStringLiteral("ISO-8859-1");
            }
            const bool binary = std::any_of(result.text.cbegin(), result.text.cend(), [](QChar character)
                                            {
        const auto code = character.unicode();
        return code == 0x7f || (code < 0x20 && code != '\t' && code != '\r' && code != '\n' && code != '\f'); });
            if (binary)
                return failure(QCoreApplication::translate("TextPreview", "This file appears to be binary. The viewer currently displays text files."));
            return result;
        }
    } // namespace

    Result read(const QString& path)
    {
        if (!QDir::isAbsolutePath(path) || path.contains(QChar::Null))
            return failure(QCoreApplication::translate("TextPreview", "Choose a file with a valid absolute path."));
        const auto nativePath = QFile::encodeName(path);
        // Inspect the target before opening it for I/O. O_PATH does not open FIFOs or devices for reading.
        const Descriptor entry{::open(nativePath.constData(), O_PATH | O_CLOEXEC)};
        if (entry.fd < 0)
            return nativeFailure(errno);
        struct stat metadata
        {
        };
        if (::fstat(entry.fd, &metadata) != 0)
            return nativeFailure(errno);
        if (!S_ISREG(metadata.st_mode))
            return failure(QCoreApplication::translate("TextPreview", "Choose a regular file."));

        const auto procPath = QByteArray("/proc/self/fd/") + QByteArray::number(entry.fd);
        const Descriptor input{::open(procPath.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
        if (input.fd < 0)
            return nativeFailure(errno);
        QFile file;
        if (!file.open(input.fd, QIODevice::ReadOnly, QFileDevice::DontCloseHandle))
            return failure(file.errorString());
        auto bytes = file.read(byteLimit + 1);
        if (file.error() != QFileDevice::NoError)
            return failure(file.errorString());
        return decode(std::move(bytes));
    }
} // namespace TextPreview

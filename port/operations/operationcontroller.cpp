// SPDX-License-Identifier: GPL-2.0-or-later
#include "operationcontroller.h"
#include <QtConcurrent>
OperationController::OperationController(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<FileOperations::Result>();
    connect(&watcher_, &QFutureWatcher<FileOperations::Result>::finished, this, [this]
            {
        const auto result = watcher_.result();
        busy_ = false;
        emit busyChanged(false);
        emit finished(result); });
}
bool OperationController::start(const FileOperations::Request& request)
{
    if (busy_)
        return false;
    busy_ = true;
    emit busyChanged(true);
    watcher_.setFuture(QtConcurrent::run([request]
                                         { return FileOperations::execute(request); }));
    return true;
}

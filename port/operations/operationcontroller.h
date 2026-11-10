// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "fileoperations.h"
#include <QObject>
#include <QFutureWatcher>
class OperationController : public QObject
{
    Q_OBJECT
public:
    explicit OperationController(QObject* parent = nullptr);
    bool start(const FileOperations::Request& request);
    bool isBusy() const { return busy_; }
signals:
    void busyChanged(bool busy);
    void finished(const FileOperations::Result& result);

private:
    QFutureWatcher<FileOperations::Result> watcher_;
    bool busy_ = false;
};

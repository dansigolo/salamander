// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "copyoperations.h"
#include <QObject>
#include <QFutureWatcher>
#include <memory>
class CopyController : public QObject
{
    Q_OBJECT
public:
    explicit CopyController(QObject* parent = nullptr);
    ~CopyController() override;
    bool start(const CopyOperations::Request& request);
    bool isBusy() const { return busy_; }
    void cancel();
    void resolve(CopyOperations::Decision decision, quint64 token);
signals:
    void busyChanged(bool busy);
    void progress(const CopyOperations::Progress& progress);
    void conflict(const CopyOperations::Conflict& conflict, quint64 token);
    void finished(const CopyOperations::Result& result);

private:
    struct State;
    std::shared_ptr<State> state_;
    QFutureWatcher<CopyOperations::Result> watcher_;
    bool busy_ = false;
    quint64 generation_ = 0;
};

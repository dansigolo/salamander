// SPDX-License-Identifier: GPL-2.0-or-later
#include "copycontroller.h"
#include <QElapsedTimer>
#include <QtConcurrent>
#include <atomic>
#include <condition_variable>
#include <mutex>
struct CopyController::State
{
    std::atomic<bool> canceled{false};
    std::mutex mutex;
    std::condition_variable changed;
    bool ready = false;
    CopyOperations::Decision decision = CopyOperations::Decision::Cancel;
};
CopyController::CopyController(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<CopyOperations::Progress>();
    qRegisterMetaType<CopyOperations::Conflict>();
    qRegisterMetaType<CopyOperations::Result>();
    connect(&watcher_, &QFutureWatcher<CopyOperations::Result>::finished, this, [this]
            {
        const auto result = watcher_.result();
        busy_ = false;
        emit busyChanged(false);
        emit finished(result); });
}
CopyController::~CopyController()
{
    cancel();
    if (busy_)
        watcher_.future().waitForFinished();
}
void CopyController::cancel()
{
    if (!state_)
        return;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->canceled.store(true);
    }
    state_->changed.notify_all();
}
void CopyController::resolve(CopyOperations::Decision decision, quint64 token)
{
    if (!busy_ || token != generation_)
        return;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->decision = decision;
        state_->ready = true;
    }
    state_->changed.notify_all();
}
bool CopyController::start(const CopyOperations::Request& request)
{
    if (busy_)
        return false;
    state_ = std::make_shared<State>();
    const auto state = state_;
    const auto token = ++generation_;
    busy_ = true;
    emit busyChanged(true);
    watcher_.setFuture(QtConcurrent::run([this, request, state, token]
                                         {
        CopyOperations::Callbacks callbacks;
        callbacks.canceled = [state] { return state->canceled.load(); };
        QElapsedTimer timer;
        bool scanning = true;
        callbacks.progress = [this, token, &timer, &scanning](const CopyOperations::Progress& value) {
            if (timer.isValid() && timer.elapsed() < 40 && scanning == value.scanning) return;
            timer.start();
            scanning = value.scanning;
            QMetaObject::invokeMethod(this, [this, value, token] { if (busy_ && generation_ == token) emit progress(value); }, Qt::QueuedConnection);
        };
        callbacks.conflict = [this, state, token](const CopyOperations::Conflict& value) {
            if (state->canceled.load()) return CopyOperations::Decision::Cancel;
            QMetaObject::invokeMethod(this, [this, value, token] { if (busy_ && generation_ == token) emit conflict(value, token); }, Qt::QueuedConnection);
            std::unique_lock<std::mutex> lock(state->mutex);
            state->changed.wait(lock, [state] { return state->ready || state->canceled.load(); });
            if (state->canceled.load()) return CopyOperations::Decision::Cancel;
            const auto decision = state->decision;
            state->ready = false;
            return decision;
        };
        return CopyOperations::execute(request, callbacks); }));
    return true;
}

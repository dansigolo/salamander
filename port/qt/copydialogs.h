// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "copyoperations.h"
#include <QDialog>
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class CopyDialog : public QDialog
{
    Q_OBJECT
public:
    CopyDialog(const QString& source, const QStringList& names, const QString& destination, QWidget* parent = nullptr);
    QString destination() const;
    void accept() override;

private:
    QLineEdit* destination_;
    QLabel* error_;
};
class CopyProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CopyProgressDialog(QWidget* parent = nullptr);
    void updateProgress(const CopyOperations::Progress& progress);
    void complete();
    void reject() override;
signals:
    void cancelRequested();

private:
    QLabel* label_;
    QProgressBar* bar_;
    QPushButton* cancel_;
    bool canceled_ = false;
};

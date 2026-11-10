// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
class QLineEdit;
class QLabel;
class NameDialog : public QDialog
{
    Q_OBJECT
public:
    NameDialog(const QString& title, const QString& description, const QString& initialName, QWidget* parent = nullptr);
    QString name() const;
    void accept() override;

private:
    QLineEdit* input_;
    QLabel* error_;
};

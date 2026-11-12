// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;

class TextViewer : public QDialog
{
    Q_OBJECT
public:
    explicit TextViewer(const QString& path, QWidget* parent = nullptr);

private:
    void find(bool backward);
    QPlainTextEdit* text_;
    QLineEdit* query_;
    QLabel* searchStatus_;
};

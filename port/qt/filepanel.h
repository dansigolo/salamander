// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
class QFileSystemModel;
class QLineEdit;
class QLabel;
class QTreeView;
class FilePanel : public QWidget
{
public:
    explicit FilePanel(const QString& path, QWidget* parent = nullptr);
    bool navigate(const QString& path);
    void goUp();
    void setHiddenFilesVisible(bool visible);
    QString currentPath() const;
    QTreeView* view() const { return view_; }
    QFileSystemModel* model() const { return model_; }

private:
    QFileSystemModel* model_;
    QTreeView* view_;
    QLineEdit* path_;
    QLabel* status_;
    QString currentPath_;
};

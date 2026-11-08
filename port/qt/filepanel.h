// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QHash>
#include <QModelIndex>
#include <QSet>
#include <QStringList>
class QFileSystemModel;
class QLineEdit;
class QLabel;
class FileView;
class DirectorySortModel;
class FilePanel : public QWidget
{
public:
    explicit FilePanel(const QString& path, QWidget* parent = nullptr);
    bool navigate(const QString& path);
    void goUp();
    void setHiddenFilesVisible(bool visible);
    QString currentPath() const;
    FileView* view() const { return view_; }
    QFileSystemModel* model() const { return model_; }
    QModelIndex visibleIndex(const QString& path) const;

private:
    struct LocationState
    {
        QString focused;
        QStringList selected;
        int scroll = 0;
    };
    void saveLocation();
    void restoreLocation(bool loaded);
    void updateStatus();
    QFileSystemModel* model_;
    DirectorySortModel* sorted_;
    FileView* view_;
    QLineEdit* path_;
    QLabel* status_;
    QString currentPath_;
    QHash<QString, LocationState> locations_;
    QSet<QString> loaded_;
    bool restoring_ = false;
};

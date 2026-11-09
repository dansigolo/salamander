// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QHash>
#include <QModelIndex>
#include <QSet>
#include <QStringList>
class QFileSystemModel;
class QComboBox;
class QAction;
class DetailedFileModel;
class QLabel;
class FileView;
class DirectorySortModel;
class FilePanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilePanel(const QString& path, QWidget* parent = nullptr);
    ~FilePanel() override;
    bool navigate(const QString& path);
    void goUp();
    void goBack();
    void goForward();
    bool canGoBack() const;
    bool canGoForward() const;
    void setActive(bool active);
    QStringList selectedPaths() const;
    void setHiddenFilesVisible(bool visible);
    QString currentPath() const;
    FileView* view() const { return view_; }
    QFileSystemModel* model() const { return model_; }
    QModelIndex visibleIndex(const QString& path) const;

signals:
    void panelActivated();
    void pathChanged(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool navigateInternal(const QString& path, int historyTarget);
    void updateNavigation();
    void refreshSpace();
    QModelIndex sourceIndex(const QModelIndex& visible) const;
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
    DetailedFileModel* details_;
    FileView* view_;
    QComboBox* path_;
    QLabel* space_;
    QWidget* pathBar_;
    QAction* back_;
    QAction* forward_;
    QStringList history_;
    int historyPosition_ = -1;
    QLabel* status_;
    QString currentPath_;
    QHash<QString, LocationState> locations_;
    QSet<QString> loaded_;
    bool restoring_ = false;
};

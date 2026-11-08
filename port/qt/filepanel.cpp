// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTreeView>

FilePanel::FilePanel(const QString& path, QWidget* parent) : QWidget(parent)
{
    model_ = new QFileSystemModel(this);
    model_->setReadOnly(true);
    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
    view_ = new QTreeView(this);
    view_->setModel(model_);
    view_->setRootIsDecorated(false);
    view_->setItemsExpandable(false);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setDragDropMode(QAbstractItemView::NoDragDrop);
    view_->setSortingEnabled(true);
    view_->sortByColumn(0, Qt::AscendingOrder);
    view_->header()->setStretchLastSection(true);
    view_->setColumnWidth(0, 260);
    path_ = new QLineEdit(this);
    path_->setAccessibleName(tr("Directory path"));
    auto* up = new QPushButton(tr("Up"), this);
    up->setToolTip(tr("Parent directory (Alt+Up)"));
    auto* row = new QHBoxLayout;
    row->addWidget(up);
    row->addWidget(path_);
    status_ = new QLabel(tr("Read-only navigation prototype"), this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(view_);
    layout->addWidget(status_);
    connect(up, &QPushButton::clicked, this, [this]
            { goUp(); });
    connect(path_, &QLineEdit::returnPressed, this, [this]
            { navigate(path_->text()); });
    connect(view_, &QTreeView::activated, this, [this](const QModelIndex& index)
            {
        if (model_->isDir(index))
            navigate(model_->filePath(index)); });
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
            { status_->setText(tr("%1 selected").arg(view_->selectionModel()->selectedRows().size())); });
    if (!navigate(path))
        navigate(QDir::homePath());
}

bool FilePanel::navigate(const QString& path)
{
    QString expanded = path;
    if (path == QStringLiteral("~"))
        expanded = QDir::homePath();
    else if (path.startsWith(QStringLiteral("~/")))
        expanded = QDir::homePath() + path.mid(1);
    const QFileInfo info(QDir(currentPath_.isEmpty() ? QDir::currentPath() : currentPath_).absoluteFilePath(expanded));
    if (!info.exists() || !info.isDir() || !info.isReadable())
    {
        path_->setText(currentPath_);
        status_->setText(tr("Cannot read directory: %1").arg(path));
        return false;
    }
    currentPath_ = QDir::cleanPath(info.absoluteFilePath());
    view_->setRootIndex(model_->setRootPath(currentPath_));
    view_->clearSelection();
    path_->setText(currentPath_);
    status_->setText(tr("Read-only navigation prototype"));
    return true;
}

void FilePanel::goUp()
{
    QDir directory(currentPath_);
    if (directory.cdUp())
        navigate(directory.absolutePath());
}

void FilePanel::setHiddenFilesVisible(bool visible)
{
    auto filters = model_->filter();
    filters.setFlag(QDir::Hidden, visible);
    model_->setFilter(filters);
}

QString FilePanel::currentPath() const { return currentPath_; }

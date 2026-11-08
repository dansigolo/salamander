// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include "fileview.h"
#include "directorysortmodel.h"
#include <QScrollBar>
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
    sorted_ = new DirectorySortModel(this);
    sorted_->setSourceModel(model_);
    view_ = new FileView(this);
    view_->setModel(sorted_);
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
    up->setToolTip(tr("Parent directory (Backspace)"));
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
        const auto source = sorted_->mapToSource(index);
        if (model_->isDir(source))
            navigate(model_->filePath(source)); });
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
            { updateStatus(); });
    connect(view_, &FileView::searchChanged, this, [this]
            { updateStatus(); });
    connect(view_, &FileView::parentRequested, this, &FilePanel::goUp);
    connect(model_, &QFileSystemModel::directoryLoaded, this, [this](const QString& path)
            {
        loaded_.insert(QDir::cleanPath(path));
        if (QDir::cleanPath(path) == currentPath_)
            restoreLocation(true); });
    connect(sorted_, &QAbstractItemModel::rowsInserted, this, [this]
            { restoreLocation(false); });
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
    const QString destination = QDir::cleanPath(info.absoluteFilePath());
    if (destination == currentPath_)
    {
        path_->setText(currentPath_);
        updateStatus();
        return true;
    }
    saveLocation();
    view_->clearSearch();
    currentPath_ = destination;
    restoring_ = false;
    view_->setRootIndex(sorted_->mapFromSource(model_->setRootPath(currentPath_)));
    view_->clearSelection();
    view_->selectionModel()->setCurrentIndex({}, QItemSelectionModel::NoUpdate);
    restoring_ = true;
    restoreLocation(loaded_.contains(currentPath_));
    path_->setText(currentPath_);
    updateStatus();
    return true;
}

void FilePanel::goUp()
{
    QDir directory(currentPath_);
    if (directory.cdUp())
    {
        locations_[directory.absolutePath()].focused = currentPath_;
        navigate(directory.absolutePath());
    }
}

void FilePanel::setHiddenFilesVisible(bool visible)
{
    auto filters = model_->filter();
    filters.setFlag(QDir::Hidden, visible);
    model_->setFilter(filters);
}

QString FilePanel::currentPath() const { return currentPath_; }

QModelIndex FilePanel::visibleIndex(const QString& path) const
{
    return sorted_->mapFromSource(model_->index(path));
}

void FilePanel::saveLocation()
{
    if (currentPath_.isEmpty() || restoring_)
        return;
    LocationState state;
    state.focused = model_->filePath(sorted_->mapToSource(view_->currentIndex()));
    for (const auto& index : view_->selectionModel()->selectedRows())
        state.selected.append(model_->filePath(sorted_->mapToSource(index)));
    state.scroll = view_->verticalScrollBar()->value();
    locations_[currentPath_] = state;
}

void FilePanel::restoreLocation(bool loaded)
{
    if (!restoring_)
        return;
    const auto state = locations_.value(currentPath_);
    auto focused = visibleIndex(state.focused);
    if (focused.parent() != view_->rootIndex())
        focused = {};
    if (!loaded && !focused.isValid() && !state.focused.isEmpty())
        return;
    if (!focused.isValid())
        focused = sorted_->index(0, 0, view_->rootIndex());
    if (!loaded && !focused.isValid())
        return;
    view_->clearSelection();
    for (const auto& path : state.selected)
    {
        const auto index = visibleIndex(path);
        if (index.isValid() && index.parent() == view_->rootIndex())
            view_->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    view_->selectionModel()->setCurrentIndex(focused, QItemSelectionModel::NoUpdate);
    view_->scrollTo(focused);
    view_->verticalScrollBar()->setValue(state.scroll);
    restoring_ = false;
    updateStatus();
}

void FilePanel::updateStatus()
{
    const QString selection = tr("%1 selected").arg(view_->selectionModel()->selectedRows().size());
    status_->setText(view_->searchPrefix().isEmpty() ? selection : tr("%1 | Quick search: %2").arg(selection, view_->searchPrefix()));
}

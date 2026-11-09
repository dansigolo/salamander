// SPDX-License-Identifier: GPL-2.0-or-later
#include "filepanel.h"
#include "fileview.h"
#include "directorysortmodel.h"
#include "detailedfilemodel.h"
#include <QComboBox>
#include <QToolBar>
#include <QAction>
#include <QStyle>
#include <QStorageInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QLocale>
#include <QSignalBlocker>
#include <QEvent>
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
    model_->setFilter(QDir::AllEntries | QDir::NoDot | QDir::System);
    details_ = new DetailedFileModel(model_, this);
    sorted_ = new DirectorySortModel(this);
    sorted_->setSourceModel(details_);
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
    view_->setUniformRowHeights(true);
    const int widths[] = {230, 60, 85, 95, 80, 100};
    for (int column = 0; column < DetailedFileModel::ColumnCount; ++column)
        view_->setColumnWidth(column, widths[column]);
    pathBar_ = new QWidget(this);
    pathBar_->setObjectName("panelPathBar");
    auto* toolbar = new QToolBar(pathBar_);
    toolbar->setIconSize(QSize(16, 16));
    back_ = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"), this, &FilePanel::goBack);
    forward_ = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Forward"), this, &FilePanel::goForward);
    toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Parent directory"), this, &FilePanel::goUp);
    path_ = new QComboBox(pathBar_);
    path_->setObjectName("directoryPath");
    path_->setEditable(true);
    path_->setInsertPolicy(QComboBox::NoInsert);
    path_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    path_->setMinimumWidth(100);
    path_->setAccessibleName(tr("Directory path and history"));
    space_ = new QLabel(pathBar_);
    space_->setObjectName("freeSpace");
    auto* row = new QHBoxLayout(pathBar_);
    row->setContentsMargins(2, 0, 2, 0);
    row->setSpacing(3);
    row->addWidget(toolbar);
    row->addWidget(path_, 1);
    row->addWidget(space_);
    status_ = new QLabel(this);
    status_->setObjectName("selectionStatus");
    status_->setTextFormat(Qt::PlainText);
    status_->setMinimumHeight(20);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(2);
    layout->addWidget(pathBar_);
    layout->addWidget(view_);
    layout->addWidget(status_);
    connect(path_->lineEdit(), &QLineEdit::returnPressed, this, [this]
            { navigate(path_->currentText()); });
    connect(path_, &QComboBox::textActivated, this, [this](const QString& text)
            { navigate(text); });
    connect(view_, &QTreeView::activated, this, [this](const QModelIndex& index)
            {
        const auto source = sourceIndex(index);
        if (model_->fileName(source) == "..")
            goUp();
        else if (model_->isDir(source))
            navigate(model_->filePath(source)); });
    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
            { updateStatus(); });
    connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]
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
    for (auto* child : findChildren<QWidget*>())
        child->installEventFilter(this);
}

FilePanel::~FilePanel()
{
    disconnect(view_->selectionModel(), nullptr, this, nullptr);
    disconnect(view_, nullptr, this, nullptr);
    disconnect(model_, nullptr, this, nullptr);
    disconnect(sorted_, nullptr, this, nullptr);
    view_->setModel(nullptr);
    delete sorted_;
    delete details_;
    delete model_;
}

bool FilePanel::navigate(const QString& path) { return navigateInternal(path, -1); }

bool FilePanel::navigateInternal(const QString& path, int historyTarget)
{
    QString expanded = path;
    if (path == QStringLiteral("~"))
        expanded = QDir::homePath();
    else if (path.startsWith(QStringLiteral("~/")))
        expanded = QDir::homePath() + path.mid(1);
    const QFileInfo info(QDir(currentPath_.isEmpty() ? QDir::currentPath() : currentPath_).absoluteFilePath(expanded));
    if (!info.exists() || !info.isDir() || !info.isReadable())
    {
        path_->setEditText(currentPath_);
        status_->setText(tr("Cannot read directory: %1").arg(path));
        return false;
    }
    const QString destination = QDir::cleanPath(info.absoluteFilePath());
    if (destination == currentPath_)
    {
        path_->setEditText(currentPath_);
        updateStatus();
        return true;
    }
    saveLocation();
    view_->clearSearch();
    currentPath_ = destination;
    restoring_ = false;
    model_->setFilter(QDir::AllEntries | QDir::System | (model_->filter() & QDir::Hidden) | (QDir(currentPath_).isRoot() ? QDir::NoDotAndDotDot : QDir::NoDot));
    view_->setRootIndex(sorted_->mapFromSource(details_->mapFromSource(model_->setRootPath(currentPath_))));
    view_->clearSelection();
    view_->selectionModel()->setCurrentIndex({}, QItemSelectionModel::NoUpdate);
    restoring_ = true;
    restoreLocation(loaded_.contains(currentPath_));
    if (historyTarget >= 0)
        historyPosition_ = historyTarget;
    else
    {
        history_ = history_.mid(0, historyPosition_ + 1);
        history_.append(currentPath_);
        if (history_.size() > 100)
            history_.removeFirst();
        historyPosition_ = history_.size() - 1;
    }
    updateNavigation();
    refreshSpace();
    updateStatus();
    emit pathChanged(currentPath_);
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
    return sorted_->mapFromSource(details_->mapFromSource(model_->index(path)));
}

void FilePanel::saveLocation()
{
    if (currentPath_.isEmpty() || restoring_)
        return;
    LocationState state;
    state.focused = model_->filePath(sourceIndex(view_->currentIndex()));
    if (preserveSelection_)
        state.selected = selectedPaths();
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
    for (const auto& path : preserveSelection_ ? state.selected : QStringList())
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

QModelIndex FilePanel::sourceIndex(const QModelIndex& index) const
{
    return details_->mapToSource(sorted_->mapToSource(index));
}
QStringList FilePanel::selectedPaths() const
{
    QStringList paths;
    for (const auto& index : view_->selectionModel()->selectedRows())
    {
        const auto source = sourceIndex(index);
        if (model_->fileName(source) != "..")
            paths.append(model_->filePath(source));
    }
    return paths;
}
void FilePanel::updateStatus()
{
    qint64 bytes = 0;
    int directories = 0;
    const auto selected = selectedPaths();
    for (const auto& path : selected)
    {
        const auto info = model_->fileInfo(model_->index(path));
        if (info.isDir())
            ++directories;
        else
            bytes += info.size();
    }
    QString text;
    if (!selected.isEmpty())
        text = tr("%1 selected (%2 folders), %3 in files").arg(selected.size()).arg(directories).arg(QLocale().formattedDataSize(bytes));
    else if (view_->currentIndex().isValid())
    {
        const auto info = model_->fileInfo(sourceIndex(view_->currentIndex()));
        text = info.fileName() == ".." ? tr("Parent directory") : tr("%1 | %2 | %3").arg(info.fileName(), info.isDir() ? tr("DIR") : QLocale().formattedDataSize(info.size()), DetailedFileModel::permissions(info));
    }
    else
        text = tr("No items");
    if (!view_->searchPrefix().isEmpty())
        text += tr(" | Quick search: %1").arg(view_->searchPrefix());
    status_->setText(text);
}
void FilePanel::updateNavigation()
{
    QSignalBlocker blocker(path_);
    QStringList recent;
    for (auto i = history_.crbegin(); i != history_.crend(); ++i)
        if (!recent.contains(*i))
            recent.append(*i);
    path_->clear();
    path_->addItems(recent);
    path_->setEditText(currentPath_);
    back_->setEnabled(canGoBack());
    forward_->setEnabled(canGoForward());
}
bool FilePanel::canGoBack() const { return historyPosition_ > 0; }
bool FilePanel::canGoForward() const { return historyPosition_ + 1 < history_.size(); }
void FilePanel::goBack()
{
    if (canGoBack())
        navigateInternal(history_[historyPosition_ - 1], historyPosition_ - 1);
}
void FilePanel::goForward()
{
    if (canGoForward())
        navigateInternal(history_[historyPosition_ + 1], historyPosition_ + 1);
}
void FilePanel::setActive(bool active)
{
    auto colors = palette();
    colors.setColor(QPalette::Window, active ? palette().color(QPalette::Highlight).lighter(175) : palette().color(QPalette::AlternateBase));
    pathBar_->setPalette(colors);
    pathBar_->setAutoFillBackground(true);
}
void FilePanel::refreshSpace()
{
    const auto path = currentPath_;
    space_->setText(tr("…"));
    auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, path]
            {
        const auto result = watcher->result();
        if (path == currentPath_)
        {
            space_->setText(result.first);
            space_->setToolTip(result.second);
        }
        watcher->deleteLater(); });
    watcher->setFuture(QtConcurrent::run([path]
                                         {
        const QStorageInfo storage(path);
        return qMakePair(storage.isValid() && storage.isReady() ? QLocale().formattedDataSize(storage.bytesAvailable(), 1) + QObject::tr(" free") : QObject::tr("Space unavailable"), storage.rootPath()); }));
}

bool FilePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
        emit panelActivated();
    return QWidget::eventFilter(watched, event);
}

void FilePanel::setPreserveSelectionOnNavigation(bool enabled)
{
    preserveSelection_ = enabled;
    if (!enabled)
        for (auto& location : locations_)
            location.selected.clear();
}

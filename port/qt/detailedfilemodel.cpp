// SPDX-License-Identifier: GPL-2.0-or-later
#include "detailedfilemodel.h"
#include <QDateTime>
#include <QLocale>

DetailedFileModel::DetailedFileModel(QFileSystemModel* files, QObject* parent) : QAbstractProxyModel(parent)
{
    setSourceModel(files);
    connect(files, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex& parent, int first, int last)
            { beginInsertRows(mapFromSource(parent), first, last); });
    connect(files, &QAbstractItemModel::rowsInserted, this, [this]
            { endInsertRows(); });
    connect(files, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex& parent, int first, int last)
            { beginRemoveRows(mapFromSource(parent), first, last); });
    connect(files, &QAbstractItemModel::rowsRemoved, this, [this]
            { endRemoveRows(); });
    connect(files, &QAbstractItemModel::modelAboutToBeReset, this, [this]
            { beginResetModel(); });
    connect(files, &QAbstractItemModel::modelReset, this, [this]
            { endResetModel(); });
    connect(files, &QAbstractItemModel::layoutAboutToBeChanged, this, [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint hint)
            {
        emit layoutAboutToBeChanged({}, hint);
        layoutIndexes_ = persistentIndexList();
        for (const auto& index : layoutIndexes_)
        {
            layoutSources_.append(mapToSource(index));
            layoutColumns_.append(index.column());
        } });
    connect(files, &QAbstractItemModel::layoutChanged, this, [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint hint)
            {
        QModelIndexList updated;
        for (int i = 0; i < layoutSources_.size(); ++i)
        {
            const auto source = layoutSources_[i];
            updated.append(source.isValid() ? createIndex(source.row(), layoutColumns_[i], source.internalPointer()) : QModelIndex());
        }
        changePersistentIndexList(layoutIndexes_, updated);
        layoutIndexes_.clear();
        layoutSources_.clear();
        layoutColumns_.clear();
        emit layoutChanged({}, hint); });
    connect(files, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& first, const QModelIndex& last)
            { emit dataChanged(index(first.row(), 0, mapFromSource(first.parent())), index(last.row(), ColumnCount - 1, mapFromSource(last.parent()))); });
}
QModelIndex DetailedFileModel::parent(const QModelIndex& index) const { return index.isValid() ? mapFromSource(sourceModel()->parent(mapToSource(index))) : QModelIndex(); }
int DetailedFileModel::rowCount(const QModelIndex& parent) const { return !sourceModel() || (parent.isValid() && parent.column() != 0) ? 0 : sourceModel()->rowCount(mapToSource(parent)); }
bool DetailedFileModel::hasChildren(const QModelIndex& parent) const { return sourceModel() && (!parent.isValid() || parent.column() == 0) && sourceModel()->hasChildren(mapToSource(parent)); }
bool DetailedFileModel::canFetchMore(const QModelIndex& parent) const { return sourceModel() && (!parent.isValid() || parent.column() == 0) && sourceModel()->canFetchMore(mapToSource(parent)); }
void DetailedFileModel::fetchMore(const QModelIndex& parent)
{
    if (canFetchMore(parent))
        sourceModel()->fetchMore(mapToSource(parent));
}
int DetailedFileModel::columnCount(const QModelIndex& parent) const { return !sourceModel() || (parent.isValid() && parent.column() != 0) ? 0 : ColumnCount; }
QModelIndex DetailedFileModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= columnCount(parent))
        return {};
    const auto source = sourceModel()->index(row, 0, mapToSource(parent));
    return source.isValid() ? createIndex(row, column, source.internalPointer()) : QModelIndex();
}
QModelIndex DetailedFileModel::sibling(int row, int column, const QModelIndex& other) const { return index(row, column, parent(other)); }
QModelIndex DetailedFileModel::mapToSource(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    static const int columns[] = {0, 0, 1, 3, 3, 0};
    return createSourceIndex(index.row(), columns[index.column()], index.internalPointer());
}
QModelIndex DetailedFileModel::mapFromSource(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    static const int columns[] = {Name, Size, Attributes, Date};
    return createIndex(index.row(), columns[index.column()], index.internalPointer());
}
QFileInfo DetailedFileModel::fileInfo(const QModelIndex& index) const
{
    return static_cast<const QFileSystemModel*>(sourceModel())->fileInfo(mapToSource(index));
}
QString DetailedFileModel::permissions(const QFileInfo& info)
{
    QString result = info.isSymLink() ? "l" : info.isDir() ? "d"
                                                           : "-";
    const QFileDevice::Permission bits[] = {QFileDevice::ReadOwner, QFileDevice::WriteOwner, QFileDevice::ExeOwner, QFileDevice::ReadGroup, QFileDevice::WriteGroup, QFileDevice::ExeGroup, QFileDevice::ReadOther, QFileDevice::WriteOther, QFileDevice::ExeOther};
    const auto flags = info.permissions();
    for (int i = 0; i < 9; ++i)
        result += flags.testFlag(bits[i]) ? QStringLiteral("rwx").at(i % 3) : QChar('-');
    return result;
}
QVariant DetailedFileModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const auto info = fileInfo(index);
    const bool parent = info.fileName() == "..";
    if (role == QFileSystemModel::FileNameRole)
        return info.fileName();
    if (role == Qt::DecorationRole)
        return index.column() == Name ? sourceModel()->data(mapToSource(index), role) : QVariant();
    if (role == Qt::TextAlignmentRole && index.column() == Size)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::ToolTipRole)
        return info.absoluteFilePath() + (parent ? QString() : "\n" + permissions(info));
    if (role != Qt::DisplayRole)
        return sourceModel()->data(mapToSource(index), role);
    const QString extension = !info.isDir() && !info.completeBaseName().isEmpty() ? info.suffix() : QString();
    switch (index.column())
    {
    case Name:
        return extension.isEmpty() ? info.fileName() : info.completeBaseName();
    case Extension:
        return extension;
    case Size:
        return info.isDir() ? tr("DIR") : QLocale().formattedDataSize(info.size(), 1);
    case Date:
        return parent ? QString() : QLocale().toString(info.lastModified().date(), QLocale::ShortFormat);
    case Time:
        return parent ? QString() : info.lastModified().time().toString("HH:mm:ss");
    case Attributes:
        return parent ? QString() : permissions(info);
    }
    return {};
}
QVariant DetailedFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        const char* labels[] = {"Name", "Ext", "Size", "Date", "Time", "Attr"};
        return section >= 0 && section < ColumnCount ? tr(labels[section]) : QVariant();
    }
    return {};
}
Qt::ItemFlags DetailedFileModel::flags(const QModelIndex& index) const
{
    auto value = sourceModel()->flags(mapToSource(index));
    if (index.isValid() && fileInfo(index).fileName() == "..")
        value &= ~Qt::ItemIsSelectable;
    return value;
}

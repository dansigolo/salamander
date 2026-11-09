// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QAbstractProxyModel>
#include <QFileSystemModel>
class DetailedFileModel : public QAbstractProxyModel
{
public:
    enum Column
    {
        Name,
        Extension,
        Size,
        Date,
        Time,
        Attributes,
        ColumnCount
    };
    explicit DetailedFileModel(QFileSystemModel* files, QObject* parent = nullptr);
    QModelIndex parent(const QModelIndex& index) const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;
    int rowCount(const QModelIndex& parent = {}) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex sibling(int row, int column, const QModelIndex& index) const override;
    QModelIndex mapToSource(const QModelIndex& index) const override;
    QModelIndex mapFromSource(const QModelIndex& index) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QFileInfo fileInfo(const QModelIndex& index) const;
    static QString permissions(const QFileInfo& info);

private:
    QModelIndexList layoutIndexes_;
    QList<QPersistentModelIndex> layoutSources_;
    QList<int> layoutColumns_;
};

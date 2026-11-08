// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QDateTime>
class DirectorySortModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (index.column() == 2 && role == Qt::DisplayRole)
        {
            const auto info = static_cast<const QFileSystemModel*>(sourceModel())->fileInfo(mapToSource(index));
            return info.isDir() ? QString() : info.suffix();
        }
        return QSortFilterProxyModel::data(index, role);
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Horizontal && section == 2 && role == Qt::DisplayRole)
            return tr("Ext");
        return QSortFilterProxyModel::headerData(section, orientation, role);
    }

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        const auto* files = static_cast<const QFileSystemModel*>(sourceModel());
        const auto a = files->fileInfo(left), b = files->fileInfo(right);
        if (a.isDir() != b.isDir())
            return sortOrder() == Qt::AscendingOrder ? a.isDir() : !a.isDir();
        // Directories remain ordered by name when sorting by size/date.
        if (!a.isDir())
        {
            if (left.column() == 1 && a.size() != b.size())
                return a.size() < b.size();
            if (left.column() == 3 && a.lastModified() != b.lastModified())
                return a.lastModified() < b.lastModified();
            if (left.column() == 2)
            {
                const int type = QString::compare(a.suffix(), b.suffix(), Qt::CaseInsensitive);
                if (type)
                    return type < 0;
            }
        }
        int comparison = QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive);
        if (!comparison)
            comparison = QString::compare(a.fileName(), b.fileName(), Qt::CaseSensitive);
        return comparison < 0;
    }
};

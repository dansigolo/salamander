// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "detailedfilemodel.h"
#include <QSortFilterProxyModel>
#include <QDateTime>
class DirectorySortModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        const auto* files = static_cast<const DetailedFileModel*>(sourceModel());
        const auto a = files->fileInfo(left), b = files->fileInfo(right);
        const bool ascending = sortOrder() == Qt::AscendingOrder;
        if ((a.fileName() == "..") != (b.fileName() == ".."))
            return ascending ? a.fileName() == ".." : b.fileName() == "..";
        if (a.isDir() != b.isDir())
            return ascending ? a.isDir() : !a.isDir();
        if (!a.isDir())
        {
            if (left.column() == DetailedFileModel::Size && a.size() != b.size())
                return a.size() < b.size();
            if ((left.column() == DetailedFileModel::Date || left.column() == DetailedFileModel::Time) && a.lastModified() != b.lastModified())
                return a.lastModified() < b.lastModified();
            if (left.column() == DetailedFileModel::Extension)
            {
                const int extension = QString::compare(a.suffix(), b.suffix(), Qt::CaseInsensitive);
                if (extension)
                    return extension < 0;
            }
        }
        if (left.column() == DetailedFileModel::Attributes && DetailedFileModel::permissions(a) != DetailedFileModel::permissions(b))
            return DetailedFileModel::permissions(a) < DetailedFileModel::permissions(b);
        int comparison = QString::compare(a.fileName(), b.fileName(), Qt::CaseInsensitive);
        return comparison ? comparison < 0 : a.fileName() < b.fileName();
    }
};

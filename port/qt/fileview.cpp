// SPDX-License-Identifier: GPL-2.0-or-later
#include "fileview.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFileSystemModel>
#include <QScrollBar>
#include <algorithm>

bool FileView::focusNextPrevChild(bool)
{
    emit panelSwitchRequested();
    return true;
}

void FileView::focusRow(int row)
{
    const auto index = model()->index(row, 0, rootIndex());
    if (!index.isValid())
        return;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    scrollTo(index);
}

void FileView::clearSearch()
{
    if (!prefix_.isEmpty())
    {
        prefix_.clear();
        emit searchChanged(prefix_);
    }
}

void FileView::toggleAndAdvance()
{
    const auto current = currentIndex().siblingAtColumn(0);
    if (!current.isValid())
        return;
    if (current.flags().testFlag(Qt::ItemIsSelectable))
        selectionModel()->select(current, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    focusRow(std::min(current.row() + 1, model()->rowCount(rootIndex()) - 1));
}

bool FileView::findPrefix(const QString& prefix, int start, int direction)
{
    for (int row = start; row >= 0 && row < model()->rowCount(rootIndex()); row += direction)
    {
        const auto index = model()->index(row, 0, rootIndex());
        if (index.flags().testFlag(Qt::ItemIsSelectable) && index.data(QFileSystemModel::FileNameRole).toString().startsWith(prefix, Qt::CaseInsensitive))
        {
            prefix_ = prefix;
            focusRow(row);
            emit searchChanged(prefix_);
            return true;
        }
    }
    return false;
}

QItemSelectionModel::SelectionFlags FileView::selectionCommand(const QModelIndex& index, const QEvent* event) const
{
    // Salamander keeps the focused item independent of marked items.
    if (event && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease))
    {
        const auto* mouse = static_cast<const QMouseEvent*>(event);
        if (mouse->modifiers() == Qt::NoModifier)
            return QItemSelectionModel::NoUpdate;
    }
    return QTreeView::selectionCommand(index, event);
}

void FileView::keyPressEvent(QKeyEvent* event)
{
    const auto modifiers = event->modifiers();
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool plain = !(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    const int key = event->key();
    if (plain && key == Qt::Key_Shift)
    {
        if (!shiftSelecting_)
        {
            shiftSelecting_ = true;
            shiftSelectValue_ = !selectionModel()->isSelected(currentIndex());
        }
        return;
    }
    if (modifiers == Qt::NoModifier && (key == Qt::Key_Insert || (key == Qt::Key_Space && prefix_.isEmpty())))
    {
        clearSearch();
        toggleAndAdvance();
        return;
    }
    if (modifiers == Qt::ControlModifier && key == Qt::Key_D)
    {
        clearSelection();
        return;
    }
    if (modifiers == Qt::NoModifier && key == Qt::Key_Escape)
    {
        clearSearch();
        return;
    }
    if (modifiers == Qt::NoModifier && (key == Qt::Key_Backspace || (!prefix_.isEmpty() && key == Qt::Key_Left)))
    {
        if (prefix_.isEmpty())
            emit parentRequested();
        else
        {
            prefix_.chop(1);
            emit searchChanged(prefix_);
        }
        return;
    }
    if (modifiers == Qt::NoModifier && !prefix_.isEmpty() && key == Qt::Key_Right)
    {
        const QString name = currentIndex().siblingAtColumn(0).data(QFileSystemModel::FileNameRole).toString();
        if (prefix_.size() < name.size())
            findPrefix(name.left(prefix_.size() + 1), currentIndex().row(), 1);
        return;
    }
    const int count = model()->rowCount(rootIndex());
    const int oldRow = currentIndex().isValid() ? currentIndex().row() : 0;
    if (count && (plain || modifiers == Qt::AltModifier) &&
        (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Home || key == Qt::Key_End || key == Qt::Key_PageUp || key == Qt::Key_PageDown))
    {
        int direction = (key == Qt::Key_Up || key == Qt::Key_PageUp || key == Qt::Key_End) ? -1 : 1;
        if (modifiers == Qt::AltModifier)
        {
            const int start = key == Qt::Key_Home ? 0 : key == Qt::Key_End ? count - 1
                                                                           : oldRow + direction;
            for (int row = start; row >= 0 && row < count; row += direction)
                if (selectionModel()->isSelected(model()->index(row, 0, rootIndex())))
                {
                    focusRow(row);
                    break;
                }
            return;
        }
        if (!prefix_.isEmpty() && !shift)
        {
            const int start = key == Qt::Key_Home ? 0 : key == Qt::Key_End ? count - 1
                                                                           : oldRow + direction;
            findPrefix(prefix_, start, direction);
            return;
        }
        const int page = std::max(1, viewport()->height() / std::max(1, sizeHintForRow(oldRow)) - 1);
        int row = oldRow;
        switch (key)
        {
        case Qt::Key_Up:
            row--;
            break;
        case Qt::Key_Down:
            row++;
            break;
        case Qt::Key_Home:
            row = 0;
            break;
        case Qt::Key_End:
            row = count - 1;
            break;
        case Qt::Key_PageUp:
            row -= page;
            break;
        case Qt::Key_PageDown:
            row += page;
            break;
        }
        row = std::clamp(row, 0, count - 1);
        if (shift)
        {
            if (!shiftSelecting_)
            {
                shiftSelecting_ = true;
                shiftSelectValue_ = !selectionModel()->isSelected(currentIndex());
            }
            int target = row;
            if (row != oldRow && key != Qt::Key_Home && key != Qt::Key_End)
                target += row > oldRow ? -1 : 1;
            const auto first = model()->index(std::min(oldRow, target), 0, rootIndex());
            const auto last = model()->index(std::max(oldRow, target), 0, rootIndex());
            for (int item = first.row(); item <= last.row(); ++item)
            {
                const auto index = model()->index(item, 0, rootIndex());
                if (index.flags().testFlag(Qt::ItemIsSelectable))
                    selectionModel()->select(index, (shiftSelectValue_ ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows);
            }
        }
        focusRow(row);
        return;
    }
    if (plain && !event->text().isEmpty() && event->text().at(0).isPrint())
    {
        const QString candidate = prefix_ + event->text();
        findPrefix(candidate, prefix_.isEmpty() ? 0 : oldRow, 1);
        return;
    }
    QTreeView::keyPressEvent(event);
}

void FileView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Shift)
        shiftSelecting_ = false;
    QTreeView::keyReleaseEvent(event);
}

void FileView::focusOutEvent(QFocusEvent* event)
{
    shiftSelecting_ = false;
    clearSearch();
    QTreeView::focusOutEvent(event);
}

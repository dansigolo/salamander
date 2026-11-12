// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QTreeView>
class FileView : public QTreeView
{
    Q_OBJECT
public:
    using QTreeView::QTreeView;
    QString searchPrefix() const { return prefix_; }
    void clearSearch();
    void setFullRowSelect(bool enabled);
    bool fullRowSelect() const { return fullRowSelect_; }
signals:
    void searchChanged(const QString& prefix);
    void parentRequested();
    void panelSwitchRequested();

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index, const QEvent* event) const override;
    void keyboardSearch(const QString&) override {} // All type-to-search goes through the prefix state below.
private:
    QRect nameArea(const QModelIndex& index, const QRect& cell, QStyleOptionViewItem option) const;
    bool acceptsClick(const QMouseEvent* event) const;
    void focusRow(int row);
    void toggleAndAdvance();
    bool findPrefix(const QString& prefix, int start, int direction);
    QString prefix_;
    bool fullRowSelect_ = false;
    bool itemPressed_ = false;
    bool shiftSelecting_ = false;
    bool shiftSelectValue_ = true;
};

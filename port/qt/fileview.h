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
signals:
    void searchChanged(const QString& prefix);
    void parentRequested();
    void panelSwitchRequested();

protected:
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index, const QEvent* event) const override;
    void keyboardSearch(const QString&) override {} // All type-to-search goes through the prefix state below.
private:
    void focusRow(int row);
    void toggleAndAdvance();
    bool findPrefix(const QString& prefix, int start, int direction);
    QString prefix_;
    bool shiftSelecting_ = false;
    bool shiftSelectValue_ = true;
};

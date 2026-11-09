// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QMainWindow>
class FilePanel;
class QAction;
class QComboBox;
class QLineEdit;
class QMenu;
class QToolBar;
class QSettings;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& leftPath, const QString& rightPath, bool inspectVolumes = true, QSettings* settings = nullptr);
    ~MainWindow() override;
    FilePanel* leftPanel() const { return left_; }
    FilePanel* rightPanel() const { return right_; }
    FilePanel* activePanel() const { return active_; }

private:
    void setActivePanel(FilePanel* panel);
    void updateNavigation();
    void addPanelMenu(QMenu* menu, FilePanel* panel);
    void sortActive(int column);
    void loadVolumes();
    QAction* unavailable(const QString& text, const QString& name);
    FilePanel* left_;
    FilePanel* right_;
    FilePanel* active_ = nullptr;
    QAction* back_;
    QAction* forward_;
    QComboBox* volumes_;
    QLineEdit* location_;
    QMenu* favorites_;
    QSettings* settings_;
};

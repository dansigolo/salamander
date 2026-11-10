// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QMainWindow>
#include "fileoperations.h"
class FilePanel;
class QAction;
class QComboBox;
class QLineEdit;
class QMenu;
class QToolBar;
class QSettings;
class QLabel;
class OperationController;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& leftPath, const QString& rightPath, bool inspectVolumes = true, QSettings* settings = nullptr);
    ~MainWindow() override;
    FilePanel* leftPanel() const { return left_; }
    FilePanel* rightPanel() const { return right_; }
    FilePanel* activePanel() const { return active_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createDirectory();
    void renameItems();
    void showRenameDialog();
    void finishOperation(const FileOperations::Result& result);
    void updateOperationActions();
    void showOperationError(const FileOperations::Result& result);
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
    OperationController* controller_;
    QAction* renameAction_;
    QAction* createDirectoryAction_;
    QLabel* operationStatus_;
    FilePanel* operationPanel_ = nullptr;
    QString operationDirectory_;
    QStringList renameNames_;
    int renamePosition_ = 0;
    int renameCompleted_ = 0;
    int statusGeneration_ = 0;
    bool renameSession_ = false;
    bool renameMarked_ = false;
    bool dialogOpen_ = false;
};

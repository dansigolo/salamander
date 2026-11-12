// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "textviewer.h"
#include "appearance.h"
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
#include "namedialog.h"
#include "operationcontroller.h"
#include "copycontroller.h"
#include "copydialogs.h"
#include <QLocale>
#include <QPushButton>
#include <QCloseEvent>
#include <QTimer>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

QAction* MainWindow::unavailable(const QString& text, const QString& name)
{
    auto* action = new QAction(text, this);
    action->setObjectName(name);
    action->setEnabled(false);
    return action;
}

MainWindow::MainWindow(const QString& leftPath, const QString& rightPath, bool inspectVolumes, QSettings* settings) : settings_(settings ? settings : new QSettings(this))
{
    setObjectName("salamanderWindow");
    controller_ = new OperationController(this);
    copyController_ = new CopyController(this);
    connect(copyController_, &CopyController::busyChanged, this, [this]
            { updateOperationActions(); });
    connect(copyController_, &CopyController::progress, this, [this](const CopyOperations::Progress& value)
            { if (copyProgress_) copyProgress_->updateProgress(value); });
    connect(copyController_, &CopyController::conflict, this, &MainWindow::showCopyConflict);
    connect(copyController_, &CopyController::finished, this, &MainWindow::finishCopy);
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto* splitter = new QSplitter(central);
    left_ = new FilePanel(leftPath, splitter);
    right_ = new FilePanel(rightPath, splitter);
    left_->setObjectName("leftPanel");
    right_->setObjectName("rightPanel");
    splitter->setSizes({680, 680});
    layout->addWidget(splitter, 1);
    operationStatus_ = new QLabel(central);
    operationStatus_->setObjectName("operationStatus");
    operationStatus_->setTextFormat(Qt::PlainText);
    operationStatus_->hide();
    layout->addWidget(operationStatus_);
    auto* locationRow = new QHBoxLayout;
    locationRow->setContentsMargins(4, 0, 4, 0);
    locationRow->addWidget(new QLabel(tr("Go to:"), central));
    location_ = new QLineEdit(central);
    location_->setObjectName("goToDirectory");
    location_->setPlaceholderText(tr("Directory path — Enter to navigate (Ctrl+L)"));
    locationRow->addWidget(location_);
    layout->addLayout(locationRow);
    setCentralWidget(central);
    auto* leftMenu = menuBar()->addMenu(tr("&Left"));
    auto* filesMenu = menuBar()->addMenu(tr("&Files"));
    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto* commandsMenu = menuBar()->addMenu(tr("&Commands"));
    auto* pluginsMenu = menuBar()->addMenu(tr("&Plugins"));
    auto* optionsMenu = menuBar()->addMenu(tr("&Options"));
    auto* rightMenu = menuBar()->addMenu(tr("&Right"));
    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    addPanelMenu(leftMenu, left_);
    addPanelMenu(rightMenu, right_);
    auto* top = addToolBar(tr("Top toolbar"));
    top->setObjectName("topToolbar");
    top->setIconSize({16, 16});
    top->setMovable(false);
    auto make = [this](const QString& name, const QString& text, QStyle::StandardPixmap icon)
    {
        auto* action = new QAction(style()->standardIcon(icon), text, this);
        action->setObjectName(name);
        return action;
    };
    back_ = make("navigateBack", tr("Back"), QStyle::SP_ArrowBack);
    forward_ = make("navigateForward", tr("Forward"), QStyle::SP_ArrowForward);
    auto* up = make("navigateUp", tr("Parent directory"), QStyle::SP_ArrowUp);
    auto* home = make("navigateHome", tr("Home"), QStyle::SP_DirHomeIcon);
    auto* root = make("navigateRoot", tr("Filesystem root"), QStyle::SP_DriveHDIcon);
    back_->setShortcut(QKeySequence("Alt+Left"));
    forward_->setShortcut(QKeySequence("Alt+Right"));
    connect(back_, &QAction::triggered, this, [this]
            { active_->goBack(); });
    connect(forward_, &QAction::triggered, this, [this]
            { active_->goForward(); });
    connect(up, &QAction::triggered, this, [this]
            { active_->goUp(); });
    connect(home, &QAction::triggered, this, [this]
            { active_->navigate(QDir::homePath()); });
    connect(root, &QAction::triggered, this, [this]
            { active_->navigate(QDir::rootPath()); });
    for (auto* action : {back_, forward_, up, home, root})
    {
        top->addAction(action);
        commandsMenu->addAction(action);
    }
    top->addSeparator();
    const QString operationNames[] = {"renameFiles", "viewFile", "editFile", "copyFiles", "moveFiles", "makeDirectory", "deleteFiles"};
    const QString operationTexts[] = {tr("Rename"), tr("View"), tr("Edit"), tr("Copy"), tr("Move"), tr("Make directory"), tr("Move to Trash")};
    QList<QAction*> operations;
    for (int i = 0; i < 7; ++i)
    {
        auto* action = (i != 2) ? new QAction(operationTexts[i], this) : unavailable(operationTexts[i], operationNames[i]);
        action->setObjectName(operationNames[i]);
        filesMenu->addAction(action);
        operations.append(action);
    }
    viewAction_ = operations[1];
    viewAction_->setShortcut(QKeySequence(Qt::Key_F3));
    viewAction_->setAutoRepeat(false);
    connect(viewAction_, &QAction::triggered, this, &MainWindow::viewFile);
    copyAction_ = operations[3];
    copyAction_->setShortcut(QKeySequence(Qt::Key_F5));
    connect(copyAction_, &QAction::triggered, this, [this]
            { copyItems(false); });
    moveAction_ = operations[4];
    moveAction_->setShortcut(QKeySequence(Qt::Key_F6));
    connect(moveAction_, &QAction::triggered, this, [this]
            { copyItems(true); });
    renameAction_ = operations[0];
    createDirectoryAction_ = operations[5];
    trashAction_ = operations[6];
    trashAction_->setShortcut(QKeySequence(Qt::Key_F8));
    connect(trashAction_, &QAction::triggered, this, &MainWindow::trashItems);
    renameAction_->setShortcut(QKeySequence(Qt::Key_F2));
    createDirectoryAction_->setShortcut(QKeySequence(Qt::Key_F7));
    connect(renameAction_, &QAction::triggered, this, &MainWindow::renameItems);
    connect(createDirectoryAction_, &QAction::triggered, this, &MainWindow::createDirectory);
    connect(controller_, &OperationController::busyChanged, this, [this](bool busy)
            {
        if (busy) ++statusGeneration_;
        operationStatus_->setVisible(busy);
        updateOperationActions(); });
    connect(controller_, &OperationController::finished, this, &MainWindow::finishOperation);
    for (int i : {0, 1, 3, 4, 5, 6})
    {
        operations[i]->setIcon(style()->standardIcon(i == 6 ? QStyle::SP_TrashIcon : QStyle::SP_FileIcon));
        top->addAction(operations[i]);
    }
    auto* quit = filesMenu->addAction(tr("Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto* all = editMenu->addAction(tr("Select all"));
    all->setObjectName("selectAll");
    auto* none = editMenu->addAction(tr("Unselect all"));
    none->setObjectName("unselectAll");
    connect(all, &QAction::triggered, this, [this]
            { active_->view()->selectAll(); });
    connect(none, &QAction::triggered, this, [this]
            { active_->view()->clearSelection(); });
    auto* copyPath = make("copyDirectoryPath", tr("Copy directory path"), QStyle::SP_FileDialogDetailedView);
    copyPath->setShortcut(QKeySequence("Ctrl+Alt+Insert"));
    connect(copyPath, &QAction::triggered, this, [this]
            { QApplication::clipboard()->setText(active_->currentPath()); });
    editMenu->addAction(copyPath);
    top->addAction(copyPath);
    top->addSeparator();
    auto* hidden = make("showHiddenFiles", tr("Show hidden files"), QStyle::SP_FileDialogContentsView);
    hidden->setCheckable(true);
    hidden->setShortcut(QKeySequence("Ctrl+H"));
    connect(hidden, &QAction::toggled, left_, &FilePanel::setHiddenFilesVisible);
    connect(hidden, &QAction::toggled, right_, &FilePanel::setHiddenFilesVisible);
    top->addAction(hidden);
    optionsMenu->addAction(hidden);
    auto* preserve = new QAction(tr("Preserve selection when changing directories"), this);
    preserve->setObjectName("preserveSelectionOnNavigation");
    preserve->setCheckable(true);
    preserve->setChecked(settings_->value("panels/preserveSelectionOnNavigation", false).toBool());
    left_->setPreserveSelectionOnNavigation(preserve->isChecked());
    right_->setPreserveSelectionOnNavigation(preserve->isChecked());
    connect(preserve, &QAction::toggled, this, [this](bool enabled)
            {
        left_->setPreserveSelectionOnNavigation(enabled);
        right_->setPreserveSelectionOnNavigation(enabled);
        settings_->setValue("panels/preserveSelectionOnNavigation", enabled); });
    optionsMenu->addAction(preserve);
    auto* fullRow = optionsMenu->addAction(tr("Full Row Select"));
    fullRow->setObjectName("fullRowSelect");
    fullRow->setCheckable(true);
    fullRow->setChecked(settings_->value("panels/fullRowSelect", false).toBool());
    left_->view()->setFullRowSelect(fullRow->isChecked());
    right_->view()->setFullRowSelect(fullRow->isChecked());
    connect(fullRow, &QAction::toggled, this, [this](bool enabled)
            {
        left_->view()->setFullRowSelect(enabled);
        right_->view()->setFullRowSelect(enabled);
        settings_->setValue("panels/fullRowSelect", enabled); });
    auto* appearance = optionsMenu->addAction(tr("Appearance…"));
    appearance->setObjectName("appearance");
    connect(appearance, &QAction::triggered, this, [this]
            { Appearance::configure(*settings_, this); });
    auto* sortMenu = commandsMenu->addMenu(tr("Sort by"));
    const QString sortTexts[] = {tr("Sort by name"), tr("Sort by extension"), tr("Sort by date"), tr("Sort by size")};
    const QString sortNames[] = {"sortName", "sortExtension", "sortDate", "sortSize"};
    const int sortColumns[] = {DetailedFileModel::Name, DetailedFileModel::Extension, DetailedFileModel::Date, DetailedFileModel::Size};
    for (int i = 0; i < 4; ++i)
    {
        auto* action = new QAction(sortTexts[i], this);
        action->setObjectName(sortNames[i]);
        action->setShortcut(QKeySequence(QString("Ctrl+F%1").arg(i + 3)));
        connect(action, &QAction::triggered, this, [this, column = sortColumns[i]]
                { sortActive(column); });
        sortMenu->addAction(action);
    }
    pluginsMenu->addAction(unavailable(tr("Plugin manager"), "managePlugins"));
    addToolBarBreak();
    auto* places = addToolBar(tr("Places and volumes"));
    places->setObjectName("placesToolbar");
    places->setIconSize({16, 16});
    places->setMovable(false);
    places->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    places->addWidget(new QLabel(tr("Volumes: "), places));
    volumes_ = new QComboBox(places);
    volumes_->setObjectName("mountedVolumes");
    volumes_->setMinimumContentsLength(14);
    volumes_->addItem(tr("Loading…"));
    volumes_->setEnabled(false);
    places->addWidget(volumes_);
    connect(volumes_, &QComboBox::activated, this, [this](int index)
            { active_->navigate(volumes_->itemData(index).toString()); });
    places->addSeparator();
    favorites_ = new QMenu(tr("Favorite directories"), this);
    auto addPlace = [this, places](const QString& text, const QString& path)
    {
        if (path.isEmpty() || !QDir(path).exists())
            return;
        auto* action = new QAction(style()->standardIcon(QStyle::SP_DirIcon), text, this);
        action->setData(path);
        connect(action, &QAction::triggered, this, [this, path]
                { active_->navigate(path); });
        places->addAction(action);
        favorites_->addAction(action);
    };
    addPlace(tr("Home"), QDir::homePath());
    addPlace(tr("Desktop"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    addPlace(tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    auto* addFavorite = new QAction(tr("Add current directory"), this);
    addFavorite->setObjectName("addFavorite");
    connect(addFavorite, &QAction::triggered, this, [this, places]
            {
        const auto path = active_->currentPath();
        for (auto* action : favorites_->actions())
            if (action->data().toString() == path)
                return;
        auto* action = new QAction(style()->standardIcon(QStyle::SP_DirIcon), QDir(path).dirName().isEmpty() ? path : QDir(path).dirName(), this);
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path] { active_->navigate(path); });
        favorites_->addAction(action);
        places->addAction(action); });
    auto* favoritesButton = new QToolButton(places);
    favoritesButton->setText(tr("Favorites"));
    favoritesButton->setMenu(favorites_);
    favoritesButton->setPopupMode(QToolButton::InstantPopup);
    places->addWidget(favoritesButton);
    places->addAction(addFavorite);
    optionsMenu->addAction(top->toggleViewAction());
    optionsMenu->addAction(places->toggleViewAction());
    auto* help = new QAction(tr("Help"), this);
    help->setObjectName("showHelp");
    help->setShortcut(QKeySequence(Qt::Key_F1));
    connect(help, &QAction::triggered, this, [this]
            { QMessageBox::information(this, tr("Keyboard navigation"), tr("F2: rename marked items, or the focused item\nF3: view the focused file\nF5: copy to the other panel\nF6: move to the other panel\nF7: create directory\nF8: move to Trash\nTab: switch panels\nInsert / Space: mark and advance\nShift + arrows: select or unselect\nCtrl+A / Ctrl+D: select all / none\nType a name: quick search; Esc ends search\nBackspace: parent directory\nAlt+Left / Alt+Right: history\nCtrl+F3/F4/F5/F6: sort by name/extension/date/size\nCtrl+H: hidden files\nCtrl+L: go to directory\nF10: menus")); });
    helpMenu->addAction(help);
    auto* about = helpMenu->addAction(tr("About"));
    connect(about, &QAction::triggered, this, [this]
            { QMessageBox::about(this, tr("Open Salamander Linux"), tr("Experimental native Linux port using Qt.\nBased on Open Salamander; licensed under GPL-2.0-or-later.")); });
    auto* functionBar = new QToolBar(tr("Function keys"), this);
    functionBar->setObjectName("functionToolbar");
    functionBar->setMovable(false);
    functionBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(Qt::BottomToolBarArea, functionBar);
    QList<QAction*> keys = {help};
    keys.append(operations);
    auto* userMenu = unavailable(tr("User menu"), "userMenu");
    keys.append(userMenu);
    auto* menu = new QAction(tr("Menu"), this);
    menu->setObjectName("focusMenu");
    menu->setShortcut(QKeySequence(Qt::Key_F10));
    connect(menu, &QAction::triggered, this, [this]
            { menuBar()->setFocus(); menuBar()->setActiveAction(menuBar()->actions().first()); });
    keys.append(menu);
    keys.append(unavailable(tr("Connect"), "connectNetwork"));
    keys.append(unavailable(tr("Disconnect"), "disconnectNetwork"));
    const QString labels[] = {tr("Help"), tr("Rename"), tr("View"), tr("Edit"), tr("Copy"), tr("Move"), tr("MakeDir"), tr("Trash"), tr("UsrMenu"), tr("Menu"), tr("Connect"), tr("Disconnect")};
    for (int i = 0; i < keys.size(); ++i)
    {
        auto* button = new QToolButton(functionBar);
        button->setDefaultAction(keys[i]);
        const auto text = QString("F%1 %2").arg(i + 1).arg(labels[i]);
        button->setText(text);
        connect(keys[i], &QAction::changed, button, [button, text]
                { button->setText(text); });
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        functionBar->addWidget(button);
    }
    // Widget actions do not register keyboard shortcuts automatically.
    addAction(userMenu);
    addAction(menu);
    optionsMenu->addAction(functionBar->toggleViewAction());
    connect(location_, &QLineEdit::returnPressed, this, [this]
            {
        if (active_->navigate(location_->text()))
        {
            location_->clear();
            active_->view()->setFocus();
        } });
    auto* address = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(address, &QShortcut::activated, location_, [this]
            { location_->setFocus(); location_->selectAll(); });
    for (auto* panel : {left_, right_})
    {
        connect(panel, &FilePanel::panelActivated, this, [this, panel]
                { setActivePanel(panel); });
        connect(panel->view()->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]
                { updateOperationActions(); });
        connect(panel->view()->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
                { updateOperationActions(); });
        connect(panel, &FilePanel::pathChanged, this, [this]
                { updateNavigation(); });
        auto* other = panel == left_ ? right_ : left_;
        connect(panel->view(), &FileView::panelSwitchRequested, other->view(), [other]
                { other->view()->setFocus(); });
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* now)
            {
        if (now && (now == left_ || left_->isAncestorOf(now))) setActivePanel(left_);
        else if (now && (now == right_ || right_->isAncestorOf(now))) setActivePanel(right_); });
    setActivePanel(left_);
    left_->view()->setFocus();
    resize(1380, 780);
    if (inspectVolumes)
        loadVolumes();
}

MainWindow::~MainWindow()
{
    disconnect(qApp, nullptr, this, nullptr);
    disconnect(controller_, nullptr, this, nullptr);
    disconnect(copyController_, nullptr, this, nullptr);
    for (auto* panel : {left_, right_})
    {
        disconnect(panel, nullptr, this, nullptr);
        disconnect(panel->view()->selectionModel(), nullptr, this, nullptr);
    }
}

void MainWindow::setActivePanel(FilePanel* panel)
{
    active_ = panel;
    left_->setActive(panel == left_);
    right_->setActive(panel == right_);
    updateNavigation();
}
void MainWindow::updateNavigation()
{
    if (!active_)
        return;
    updateOperationActions();
    back_->setEnabled(active_->canGoBack());
    forward_->setEnabled(active_->canGoForward());
    setWindowTitle(active_->currentPath() + tr(" — Open Salamander Linux"));
    QSignalBlocker blocker(volumes_);
    const auto path = active_->currentPath();
    int best = -1, length = -1;
    for (int i = 0; i < volumes_->count(); ++i)
    {
        const auto root = volumes_->itemData(i).toString();
        if (!root.isEmpty() && (path == root || path.startsWith(root.endsWith('/') ? root : root + '/')) && root.size() > length)
        {
            best = i;
            length = root.size();
        }
    }
    volumes_->setCurrentIndex(best);
}
void MainWindow::sortActive(int column)
{
    const auto* header = active_->view()->header();
    const auto order = header->sortIndicatorSection() == column && header->sortIndicatorOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    active_->view()->sortByColumn(column, order);
}
void MainWindow::addPanelMenu(QMenu* menu, FilePanel* panel)
{
    auto* home = menu->addAction(tr("Home"));
    connect(home, &QAction::triggered, this, [this, panel]
            { setActivePanel(panel); panel->navigate(QDir::homePath()); });
    auto* parent = menu->addAction(tr("Parent directory"));
    connect(parent, &QAction::triggered, this, [this, panel]
            { setActivePanel(panel); panel->goUp(); });
    auto* sorting = menu->addMenu(tr("Sort by"));
    const QString labels[] = {tr("Name"), tr("Extension"), tr("Size"), tr("Date"), tr("Time"), tr("Attributes")};
    for (int i = 0; i < DetailedFileModel::ColumnCount; ++i)
    {
        auto* action = sorting->addAction(labels[i]);
        connect(action, &QAction::triggered, this, [this, panel, i]
                { setActivePanel(panel); sortActive(i); });
    }
}
void MainWindow::loadVolumes()
{
    auto* watcher = new QFutureWatcher<QList<QStorageInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<QStorageInfo>>::finished, this, [this, watcher]
            {
        volumes_->clear();
        for (const auto& storage : watcher->result())
            if (storage.isValid() && storage.isReady())
                volumes_->addItem(storage.rootPath(), storage.rootPath());
        volumes_->setEnabled(volumes_->count() > 0);
        updateNavigation();
        watcher->deleteLater(); });
    watcher->setFuture(QtConcurrent::run([]
                                         { return QStorageInfo::mountedVolumes(); }));
}

void MainWindow::updateOperationActions()
{
    if (!active_)
        return;
    const bool available = !controller_->isBusy() && !copySession_ && !copyController_->isBusy() && !dialogOpen_ && !renameSession_;
    createDirectoryAction_->setEnabled(available);
    viewAction_->setEnabled(!active_->focusedFile().isEmpty());
    const bool hasItems = available && !active_->operationNames().isEmpty();
    renameAction_->setEnabled(hasItems);
    copyAction_->setEnabled(hasItems);
    moveAction_->setEnabled(hasItems);
    trashAction_->setEnabled(hasItems);
}
void MainWindow::viewFile()
{
    const auto path = active_->focusedFile();
    if (!path.isEmpty())
        (new TextViewer(path, this))->show();
}

void MainWindow::trashItems()
{
    if (!trashAction_->isEnabled())
        return;
    const auto names = active_->operationNames();
    if (names.isEmpty())
        return;

    operationPanel_ = active_;
    const auto directory = active_->currentPath();
    trashFocusedName_ = active_->view()->currentIndex().data(QFileSystemModel::FileNameRole).toString();
    trashNeighbor_ = names.contains(trashFocusedName_) ? active_->neighborName(names) : QString();
    auto* dialog = new QMessageBox(QMessageBox::Question, tr("Move to Trash"), QString(), QMessageBox::Cancel, this);
    dialog->setObjectName("trashConfirmation");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setTextFormat(Qt::PlainText);
    auto preview = names.mid(0, 8).join("\n");
    if (names.size() > 8)
        preview += tr("\n…and %n more", nullptr, names.size() - 8);
    dialog->setText(tr("Move %n item(s) to the Trash?", nullptr, names.size()) + "\n\n" + directory + "\n\n" + preview);
    if (names.size() > 8)
        dialog->setDetailedText(names.join("\n"));
    auto* confirm = dialog->addButton(tr("Move to Trash"), QMessageBox::AcceptRole);
    confirm->setObjectName("confirmTrash");
    dialog->setDefaultButton(QMessageBox::Cancel);
    dialog->setEscapeButton(QMessageBox::Cancel);
    dialogOpen_ = true;
    updateOperationActions();
    connect(dialog, &QMessageBox::finished, this, [this, dialog, confirm, directory, names]
            {
        dialogOpen_ = false;
        if (dialog->clickedButton() == confirm)
        {
            operationStatus_->setText(tr("Moving items to the Trash…"));
            controller_->start({FileOperations::Kind::Trash, directory, {}, {}, names});
        }
        updateOperationActions(); });
    dialog->open();
}

void MainWindow::finishTrash(const FileOperations::Result& result)
{
    for (auto* panel : {left_, right_})
        if (panel->currentPath() == result.request.directory)
            panel->refreshStorageInfo();
    if (result.trashed.contains(trashFocusedName_) && !trashNeighbor_.isEmpty() &&
        operationPanel_->currentPath() == result.request.directory)
    {
        const auto neighbor = QDir(result.request.directory).filePath(trashNeighbor_);
        if (operationPanel_->visibleIndex(neighbor).isValid())
            operationPanel_->focusEntry(trashNeighbor_);
    }
    const auto summary = tr("Moved %n item(s) to the Trash.", nullptr, result.trashed.size());
    operationStatus_->setText(summary);
    operationStatus_->show();
    const int generation = ++statusGeneration_;
    QTimer::singleShot(5000, this, [this, generation]
                       {
        if (generation == statusGeneration_ && !controller_->isBusy() && !copySession_ && !dialogOpen_)
            operationStatus_->hide(); });
    if (!result.succeeded())
    {
        auto* message = new QMessageBox(QMessageBox::Warning, tr("Could not finish moving to Trash"), summary, QMessageBox::Ok, this);
        message->setObjectName("trashError");
        message->setAttribute(Qt::WA_DeleteOnClose);
        message->setWindowModality(Qt::WindowModal);
        message->setTextFormat(Qt::PlainText);
        QStringList pending = result.request.names;
        for (const auto& name : result.trashed)
            pending.removeOne(name);
        const auto failure = result.failedName.isEmpty() ? result.message : tr("Could not move: %1\n%2").arg(result.failedName, result.message);
        message->setText(summary + "\n\n" + failure);
        message->setDetailedText(tr("Source directory: %1\n\nMoved to Trash:\n%2\n\nNot confirmed as moved:\n%3").arg(result.request.directory, result.trashed.join("\n"), pending.join("\n")));
        message->open();
    }
    updateOperationActions();
}

void MainWindow::createDirectory()
{
    if (controller_->isBusy() || copySession_ || dialogOpen_ || renameSession_)
        return;
    operationPanel_ = active_;
    operationDirectory_ = active_->currentPath();
    auto* dialog = new NameDialog(tr("Make directory"), tr("Create a directory in:\n%1").arg(operationDirectory_), {}, this);
    dialogOpen_ = true;
    updateOperationActions();
    connect(dialog, &QDialog::accepted, this, [this, dialog]
            {
        dialogOpen_ = false;
        operationStatus_->setText(tr("Creating directory…"));
        controller_->start({FileOperations::Kind::CreateDirectory, operationDirectory_, {}, dialog->name()}); });
    connect(dialog, &QDialog::rejected, this, [this]
            {
        dialogOpen_ = false;
        updateOperationActions(); });
    dialog->open();
}
void MainWindow::renameItems()
{
    if (controller_->isBusy() || copySession_ || dialogOpen_ || renameSession_)
        return;
    renameNames_ = active_->operationNames();
    if (renameNames_.isEmpty())
        return;
    operationPanel_ = active_;
    operationDirectory_ = active_->currentPath();
    renameMarked_ = !active_->selectedPaths().isEmpty();
    renamePosition_ = 0;
    renameCompleted_ = 0;
    renameSession_ = true;
    showRenameDialog();
}
void MainWindow::showRenameDialog()
{
    if (renamePosition_ >= renameNames_.size())
    {
        renameSession_ = false;
        renameNames_.clear();
        updateOperationActions();
        return;
    }
    auto* dialog = new NameDialog(tr("Rename — %1 of %2").arg(renamePosition_ + 1).arg(renameNames_.size()),
                                  tr("Rename %1 in:\n%2").arg(renameNames_[renamePosition_], operationDirectory_), renameNames_[renamePosition_], this);
    dialogOpen_ = true;
    updateOperationActions();
    connect(dialog, &QDialog::accepted, this, [this, dialog]
            {
        dialogOpen_ = false;
        operationStatus_->setText(tr("Renaming %1…").arg(renameNames_[renamePosition_]));
        controller_->start({FileOperations::Kind::Rename, operationDirectory_, renameNames_[renamePosition_], dialog->name()}); });
    connect(dialog, &QDialog::rejected, this, [this]
            {
        dialogOpen_ = false;
        renameSession_ = false;
        renameNames_.clear();
        updateOperationActions(); });
    dialog->open();
}
void MainWindow::finishOperation(const FileOperations::Result& result)
{
    if (result.request.kind == FileOperations::Kind::Trash)
    {
        finishTrash(result);
        return;
    }
    if (!result.succeeded())
    {
        showOperationError(result);
        renameSession_ = false;
        renameNames_.clear();
        updateOperationActions();
        return;
    }
    if (operationPanel_->currentPath() == result.request.directory)
    {
        operationPanel_->focusEntry(result.request.targetName, result.request.kind == FileOperations::Kind::Rename && renameMarked_);
        if (active_ == operationPanel_)
            operationPanel_->view()->setFocus();
    }
    operationStatus_->setText(result.request.kind == FileOperations::Kind::CreateDirectory ? tr("Directory created: %1").arg(result.request.targetName) : (result.request.sourceName == result.request.targetName ? tr("Name unchanged: %1").arg(result.request.targetName) : tr("Renamed %1 to %2").arg(result.request.sourceName, result.request.targetName)));
    operationStatus_->show();
    const int generation = ++statusGeneration_;
    QTimer::singleShot(5000, this, [this, generation]
                       {
        if (generation == statusGeneration_ && !controller_->isBusy() && !dialogOpen_ && !renameSession_)
            operationStatus_->hide(); });
    if (result.request.kind == FileOperations::Kind::Rename)
    {
        if (result.request.sourceName != result.request.targetName)
            ++renameCompleted_;
        ++renamePosition_;
        showRenameDialog();
    }
    updateOperationActions();
}
void MainWindow::showOperationError(const FileOperations::Result& result)
{
    QString explanation;
    switch (result.error)
    {
    case FileOperations::Error::AlreadyExists:
        explanation = tr("An entry with this name already exists. Nothing was replaced.");
        break;
    case FileOperations::Error::PermissionDenied:
        explanation = tr("Permission denied or the filesystem is read-only.");
        break;
    case FileOperations::Error::NotFound:
        explanation = tr("The source item or parent directory is no longer available.");
        break;
    case FileOperations::Error::Unsupported:
        explanation = tr("This filesystem or system does not support the required operation.");
        break;
    default:
        explanation = result.message;
        break;
    }
    auto* message = new QMessageBox(QMessageBox::Warning, tr("File operation failed"), QString(), QMessageBox::Ok, this);
    message->setObjectName("operationError");
    message->setAttribute(Qt::WA_DeleteOnClose);
    message->setWindowModality(Qt::WindowModal);
    message->setTextFormat(Qt::PlainText);
    QString text = explanation + "\n\n" + QDir(result.request.directory).filePath(result.request.targetName);
    if (result.request.kind == FileOperations::Kind::Rename)
        text += tr("\nOriginal name: %1").arg(result.request.sourceName);
    if (renameSession_ && renameCompleted_ > 0)
        text += tr("\n\nCompleted renames: %1. Remaining items were not changed.").arg(renameCompleted_);
    message->setText(text);
    message->setDetailedText(result.message);
    message->open();
}
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (copyController_->isBusy())
    {
        if (copyProgress_)
            copyProgress_->reject();
        else
            copyController_->cancel();
        event->ignore();
        return;
    }
    if (controller_->isBusy())
    {
        operationStatus_->setText(tr("Please wait for the current file operation to finish."));
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::copyItems(bool moving)
{
    if (controller_->isBusy() || copySession_ || dialogOpen_ || renameSession_)
        return;
    const auto names = active_->operationNames();
    if (names.isEmpty())
        return;
    movingSession_ = moving;
    const auto source = active_->currentPath();
    copyTargetPanel_ = active_ == left_ ? right_ : left_;
    auto* dialog = new CopyDialog(source, names, copyTargetPanel_->currentPath(), this, moving);
    dialogOpen_ = true;
    updateOperationActions();
    connect(dialog, &QDialog::accepted, this, [this, dialog, source, names]
            {
        dialogOpen_ = false;
        copySession_ = true;
        copyProgress_ = new CopyProgressDialog(this, movingSession_);
        connect(copyProgress_, &CopyProgressDialog::cancelRequested, copyController_, &CopyController::cancel);
        copyProgress_->open();
        copyController_->start({source, names, dialog->destination(), movingSession_});
        updateOperationActions(); });
    connect(dialog, &QDialog::rejected, this, [this]
            { dialogOpen_ = false; updateOperationActions(); });
    dialog->open();
}
void MainWindow::showCopyConflict(const CopyOperations::Conflict& value, quint64 token)
{
    if (!copyController_->isBusy() || !copyProgress_)
        return;
    auto* message = new QMessageBox(QMessageBox::Question, tr("Destination already exists"), QString(), QMessageBox::NoButton, copyProgress_);
    copyConflict_ = message;
    message->setObjectName(movingSession_ ? "moveConflict" : "copyConflict");
    message->setAttribute(Qt::WA_DeleteOnClose);
    message->setWindowModality(Qt::WindowModal);
    message->setTextFormat(Qt::PlainText);
    message->setText(value.source + "\n→ " + value.destination + "\n\n" + (value.replaceAllowed ? tr("Replace the existing file or skip this item?") : tr("This destination will not be replaced or merged. Skip this item or cancel.")));
    QPushButton* replace = nullptr;
    QPushButton* replaceAll = nullptr;
    if (value.replaceAllowed)
    {
        replace = message->addButton(tr("Replace"), QMessageBox::AcceptRole);
        replace->setObjectName("replaceCopy");
        replaceAll = message->addButton(tr("Replace all files"), QMessageBox::AcceptRole);
        replaceAll->setObjectName("replaceAllCopy");
    }
    auto* skip = message->addButton(tr("Skip"), QMessageBox::ActionRole);
    skip->setObjectName("skipCopy");
    auto* skipAll = message->addButton(tr("Skip all"), QMessageBox::ActionRole);
    skipAll->setObjectName("skipAllCopy");
    auto* cancel = message->addButton(movingSession_ ? tr("Cancel move") : tr("Cancel copy"), QMessageBox::RejectRole);
    cancel->setObjectName("cancelCopyConflict");
    message->setDefaultButton(cancel);
    message->setEscapeButton(cancel);
    connect(message, &QDialog::finished, this, [this, message, replace, replaceAll, skip, skipAll, token]
            {
        auto decision = CopyOperations::Decision::Cancel;
        auto* clicked = message->clickedButton();
        if (replace && clicked == replace) decision = CopyOperations::Decision::Replace;
        else if (replaceAll && clicked == replaceAll) decision = CopyOperations::Decision::ReplaceAll;
        else if (clicked == skip) decision = CopyOperations::Decision::Skip;
        else if (clicked == skipAll) decision = CopyOperations::Decision::SkipAll;
        copyController_->resolve(decision, token); });
    message->open();
}
void MainWindow::finishCopy(const CopyOperations::Result& result)
{
    if (copyConflict_)
        copyConflict_->reject();
    if (copyProgress_)
        copyProgress_->complete();
    copyProgress_ = nullptr;
    copyConflict_ = nullptr;
    copySession_ = false;
    left_->refreshStorageInfo();
    right_->refreshStorageInfo();
    if (copyTargetPanel_ && copyTargetPanel_->currentPath() == result.request.destinationDirectory && !result.completed.isEmpty())
        copyTargetPanel_->focusEntry(result.completed.last());
    if (result.request.move)
        operationStatus_->setText((result.canceled ? tr("Move stopped. ") : QString()) + tr("Moved: %1; skipped: %2; copied but source retained: %3.").arg(result.completed.size()).arg(result.skipped.size()).arg(result.retained.size()));
    else
        operationStatus_->setText((result.canceled ? tr("Copy canceled. ") : QString()) + tr("Copied entries: %1 (%2); skipped: %3.").arg(result.completed.size()).arg(QLocale().formattedDataSize(result.bytesCopied)).arg(result.skipped.size()));
    operationStatus_->show();
    const int generation = ++statusGeneration_;
    QTimer::singleShot(8000, this, [this, generation]
                       { if (generation == statusGeneration_ && !copySession_ && !controller_->isBusy() && !dialogOpen_ && !renameSession_) operationStatus_->hide(); });
    if (!result.error.isEmpty() || !result.cleanupError.isEmpty() || !result.recoveryPaths.isEmpty())
    {
        auto* message = new QMessageBox(QMessageBox::Warning, result.request.move ? tr("Move incomplete") : tr("Copy incomplete"), QString(), QMessageBox::Ok, this);
        message->setObjectName(result.request.move ? "moveError" : "copyError");
        message->setAttribute(Qt::WA_DeleteOnClose);
        message->setWindowModality(Qt::WindowModal);
        message->setTextFormat(Qt::PlainText);
        QString summary = result.request.move ? tr("\n\nMoved: %1; skipped: %2; copied but source retained: %3.").arg(result.completed.size()).arg(result.skipped.size()).arg(result.retained.size()) : tr("\n\nCompleted selected entries: %1; skipped: %2. Sources were not deleted.").arg(result.completed.size()).arg(result.skipped.size());
        if (!result.partialSources.isEmpty())
            summary += tr("\nSource cleanup was partial for: %1. The completed destination contains the copied data.").arg(result.partialSources.join(", "));
        if (!result.recoveryPaths.isEmpty())
            summary += tr("\nSource data retained at these recovery paths (the original name was not overwritten):\n%1").arg(result.recoveryPaths.join("\n"));
        message->setText(result.error + "\n" + result.cleanupError + summary);
        message->open();
    }
    updateOperationActions();
}

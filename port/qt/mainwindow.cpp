// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "filepanel.h"
#include "fileview.h"
#include "detailedfilemodel.h"
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
    const QString operationTexts[] = {tr("Rename"), tr("View"), tr("Edit"), tr("Copy"), tr("Move"), tr("Make directory"), tr("Delete")};
    QList<QAction*> operations;
    for (int i = 0; i < 7; ++i)
    {
        auto* action = unavailable(operationTexts[i], operationNames[i]);
        filesMenu->addAction(action);
        operations.append(action);
    }
    for (int i : {0, 3, 4, 6})
    {
        operations[i]->setIcon(style()->standardIcon(i == 6 ? QStyle::SP_TrashIcon : QStyle::SP_FileIcon));
        top->addAction(operations[i]);
    }
    auto* quit = filesMenu->addAction(tr("Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
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
            { QMessageBox::information(this, tr("Keyboard navigation"), tr("Tab: switch panels\nInsert / Space: mark and advance\nShift + arrows: select or unselect\nCtrl+A / Ctrl+D: select all / none\nType a name: quick search; Esc ends search\nBackspace: parent directory\nAlt+Left / Alt+Right: history\nCtrl+F3/F4/F5/F6: sort by name/extension/date/size\nCtrl+H: hidden files\nCtrl+L: go to directory\nF10: menus")); });
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
    const QString labels[] = {tr("Help"), tr("Rename"), tr("View"), tr("Edit"), tr("Copy"), tr("Move"), tr("MakeDir"), tr("Delete"), tr("UsrMenu"), tr("Menu"), tr("Connect"), tr("Disconnect")};
    for (int i = 0; i < keys.size(); ++i)
    {
        auto* button = new QToolButton(functionBar);
        button->setDefaultAction(keys[i]);
        button->setText(QString("F%1 %2").arg(i + 1).arg(labels[i]));
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

MainWindow::~MainWindow() { disconnect(qApp, nullptr, this, nullptr); }

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

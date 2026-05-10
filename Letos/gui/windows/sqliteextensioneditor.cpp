#include "sqliteextensioneditor.h"
#include "common/utils.h"
#include "dialogs/settingsexportdialog.h"
#include "dialogs/settingsimportdialog.h"
#include "sqliteextensioneditormodel.h"
#include "ui_sqliteextensioneditor.h"
#include "selectabledbmodel.h"
#include "dbtree/dbtree.h"
#include "dbtree/dbtreemodel.h"
#include "uiutils.h"
#include "uiconfig.h"
#include "db/db.h"
#include "services/dbmanager.h"
#include "common/lazytrigger.h"
#include "common/userinputfilter.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QSortFilterProxyModel>

CFG_KEYS_DEFINE(SqliteExtensionEditor)

SqliteExtensionEditor::SqliteExtensionEditor(QWidget *parent) :
    MdiChild(parent),
    ui(new Ui::SqliteExtensionEditor)
{
    init();
}

SqliteExtensionEditor::~SqliteExtensionEditor()
{
    delete ui;
    probingDb->closeQuiet();
}

bool SqliteExtensionEditor::restoreSessionNextTime()
{
    return false;
}

bool SqliteExtensionEditor::isUncommitted() const
{
    return dataModel->isModified() || currentModified;
}

QString SqliteExtensionEditor::getQuitUncommittedConfirmMessage() const
{
    return tr("Extension manager window has uncommitted modifications.");
}

QVariant SqliteExtensionEditor::saveSession()
{
    return QVariant();
}

bool SqliteExtensionEditor::restoreSession(const QVariant& sessionValue)
{
    Q_UNUSED(sessionValue);
    return true;
}

Icon*SqliteExtensionEditor::getIconNameForMdiWindow()
{
    return ICONS.EXTENSION_EDITOR;
}

QString SqliteExtensionEditor::getTitleForMdiWindow()
{
    return tr("Extension manager");
}

void SqliteExtensionEditor::createActions()
{
    createAction(COMMIT, ICONS.COMMIT, tr("Commit all extension changes"), this, SLOT(commit()), ui->toolbar, this);
    createAction(ROLLBACK, ICONS.ROLLBACK, tr("Rollback all extension changes"), this, SLOT(rollback()), ui->toolbar, this);
    ui->toolbar->addSeparator();
    createAction(ADD, ICONS.EXTENSION_ADD, tr("Add new extension"), this, SLOT(newExtension()), ui->toolbar, this);
    createAction(DELETE, ICONS.EXTENSION_DELETE, tr("Remove selected extension"), this, SLOT(deleteExtension()), ui->toolbar, this);
    ui->toolbar->addSeparator();
    createAction(IMPORT, ICONS.EXTENSION_IMPORT, tr("Import extension list from file"), this, SLOT(importExtensions()), ui->toolbar, this);
    createAction(EXPORT, ICONS.EXTENSION_EXPORT, tr("Export extension list file"), this, SLOT(exportExtensions()), ui->toolbar, this);
    createAction(HELP, ICONS.HELP, tr("Editing extensions manual"), this, SLOT(help()), ui->toolbar, this);
}

void SqliteExtensionEditor::setupDefShortcuts()
{
    // Widget context
    setShortcutContext({COMMIT, ROLLBACK}, Qt::WidgetWithChildrenShortcut);
    BIND_SHORTCUTS(SqliteExtensionEditor, Action);
}

QToolBar* SqliteExtensionEditor::getToolBar(int toolbar) const
{
    Q_UNUSED(toolbar);
    return ui->toolbar;
}

void SqliteExtensionEditor::init()
{
    ui->setupUi(this);
    initActions();

    statusUpdateTrigger = new LazyTrigger(500, this, SLOT(updateCurrentExtensionState()));

    dataModel = new SqliteExtensionEditorModel(this);
    viewModel = new QSortFilterProxyModel(this);
    viewModel->setSourceModel(dataModel);
    ui->extensionList->setModel(viewModel);
    ui->extensionList->horizontalHeader()->setMinimumSectionSize(20);
    ui->extensionList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->splitter->setSizes({1, 1});
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    Cfg::handleSplitterState(ui->splitter);

    new UserInputFilter(ui->extensionFilterEdit, this, SLOT(applyFilter(QString)));
    viewModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    dbListModel = new SelectableDbModel(this);
    dbListModel->setSourceModel(DBTREE->getModel());
    ui->databaseList->setModel(dbListModel);
    ui->databaseList->expandAll();

    dataModel->setData(SQLITE_EXTENSIONS->getAllExtensions());
    connect(SQLITE_EXTENSIONS, SIGNAL(extensionListChanged()), this, SLOT(cfgExtensionListChanged()));
    ui->extensionList->resizeColumnsToContents();

    MAINWINDOW->installToolbarSizeWheelHandler(ui->toolbar);

    connect(ui->extensionList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(extensionSelected(QItemSelection,QItemSelection)));
    connect(ui->extensionList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(updateState()));
    connect(ui->fileEdit, SIGNAL(textChanged(QString)), this, SLOT(updateModified()));
    connect(ui->initEdit, SIGNAL(textChanged(QString)), this, SLOT(updateModified()));
    connect(ui->allDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->selectedDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->fileBrowse, SIGNAL(clicked()), this, SLOT(browseForFile()));
    connect(ui->fileEdit, SIGNAL(textChanged(QString)), statusUpdateTrigger, SLOT(schedule()));
    connect(ui->fileEdit, SIGNAL(textChanged(QString)), this, SLOT(generateName()));
    connect(ui->initEdit, SIGNAL(textChanged(QString)), statusUpdateTrigger, SLOT(schedule()));
    connect(dbListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateModified()));

    probingDb = DBLIST->createInMemDb(true);

    if (!probingDb->openQuiet())
        qWarning() << "Could not open in-memory dtabase for Extension manager window. Probing files will be impossible.";

    initStateForAll();
    updateState();
    updateCurrentExtensionState();
}

QModelIndex SqliteExtensionEditor::getCurrentExtensionIndex() const
{
    QModelIndexList idxList = ui->extensionList->selectionModel()->selectedIndexes();
    if (idxList.size() == 0)
        return QModelIndex();

    return idxList.first();
}

QModelIndex SqliteExtensionEditor::extRowToSrc(const QModelIndex &idx) const
{
    return viewModel->mapToSource(idx);
}

void SqliteExtensionEditor::extensionDeselected(const QModelIndex& idx)
{
    statusUpdateTrigger->cancel();

    viewModel->setData(idx, ui->fileEdit->text(), SqliteExtensionEditorModel::FILE_PATH);
    viewModel->setData(idx, ui->initEdit->text(), SqliteExtensionEditorModel::INIT_FUNC);
    viewModel->setData(idx, ui->allDatabasesRadio->isChecked(), SqliteExtensionEditorModel::ALL_DATABASES);
    viewModel->setData(idx, currentModified, SqliteExtensionEditorModel::MODIFIED);

    if (ui->selectedDatabasesRadio->isChecked())
        viewModel->setData(idx, getCurrentDatabases(), SqliteExtensionEditorModel::DATABASES);

    viewModel->setData(idx, validateExtension(idx), SqliteExtensionEditorModel::VALID);
}

void SqliteExtensionEditor::extensionSelected(const QModelIndex& idx)
{
    updatesForSelection = true;
    ui->fileEdit->setText(idx.data(SqliteExtensionEditorModel::FILE_PATH).toString());
    ui->initEdit->setText(idx.data(SqliteExtensionEditorModel::INIT_FUNC).toString());

    if (ui->fileEdit->text().contains(SqliteExtensionManager::APP_PATH_PREFIX))
        ui->fileEdit->setToolTip(SqliteExtensionManager::resolvePath(ui->fileEdit->text()));
    else
        ui->fileEdit->setToolTip(QString());

    // Databases
    dbListModel->setDatabases(idx.data(SqliteExtensionEditorModel::DATABASES).toStringList());
    ui->databaseList->expandAll();

    if (idx.data(SqliteExtensionEditorModel::ALL_DATABASES).toBool())
        ui->allDatabasesRadio->setChecked(true);
    else
        ui->selectedDatabasesRadio->setChecked(true);

    updatesForSelection = false;
    currentModified = idx.data(SqliteExtensionEditorModel::MODIFIED).toBool();

    updateCurrentExtensionState();
}

void SqliteExtensionEditor::clearEdits()
{
    ui->fileEdit->setText(QString());
    ui->initEdit->setText(QString());
    ui->allDatabasesRadio->setChecked(true);
}

void SqliteExtensionEditor::selectExtension(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    ui->extensionList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
}

QStringList SqliteExtensionEditor::getCurrentDatabases() const
{
    return dbListModel->getDatabases();
}

bool SqliteExtensionEditor::tryToLoad(const QString& filePath, const QString& initFunc, QString* resultError)
{
    if (!probingDb->isOpen())
    {
        qWarning() << "Probing database is closed. Cannot evaluate if file" << filePath << "is loadable.";
        return true;
    }

    bool loadedOk = probingDb->loadExtension(filePath, initFunc.isEmpty() ? QString() : initFunc);
    if (!loadedOk && resultError)
        *resultError = probingDb->getErrorText();

    probingDb->closeQuiet();
    probingDb->openQuiet();

    return loadedOk;
}

bool SqliteExtensionEditor::validateExtension(bool* fileOk, bool* initOk, QString* fileError)
{
    QString filePath = ui->fileEdit->text();
    QString initFunc = ui->initEdit->text();
    return validateExtension(filePath, initFunc, fileOk, initOk, fileError);
}

bool SqliteExtensionEditor::validateExtension(const QModelIndex& idx)
{
    QString filePath = idx.data(SqliteExtensionEditorModel::FILE_PATH).toString();
    QString initFunc = idx.data(SqliteExtensionEditorModel::INIT_FUNC).toString();
    return validateExtension(filePath, initFunc, nullptr, nullptr, nullptr);
}

bool SqliteExtensionEditor::validateCurrentExtension()
{
    QString filePath = ui->fileEdit->text();
    QString initFunc = ui->initEdit->text();
    return validateExtension(filePath, initFunc, nullptr, nullptr, nullptr);
}

bool SqliteExtensionEditor::validateExtension(const QString& filePath, const QString& initFunc, bool* fileOk, bool* initOk, QString* fileError)
{
    bool localFileOk = true;
    bool localInitOk = true;

    QString resolvedPath = SqliteExtensionManager::resolvePath(filePath);
    QFileInfo fileInfo(resolvedPath);
    if (!fileInfo.exists() || !fileInfo.isReadable())
    {
        localFileOk = false;
        if (fileError)
            *fileError = tr("File with given path does not exist or is not readable.");
    }
    else
        localFileOk = tryToLoad(resolvedPath, initFunc, fileError);

    if (!localFileOk && fileError && fileError->isEmpty())
        *fileError = tr("Unable to load extension: %1").arg(resolvedPath);

    static const QRegularExpression initFuncRegExp("^[a-zA-Z0-9_]*$");
    localInitOk = initFuncRegExp.match(initFunc).hasMatch();

    if (fileOk)
        *fileOk = localFileOk;

    if (initOk)
        *initOk = localInitOk;

    return localFileOk && localInitOk;
}

void SqliteExtensionEditor::initStateForAll()
{
    for (int i = 0, total = dataModel->rowCount(); i < total; ++i)
    {
        QModelIndex idx = dataModel->index(i);
        dataModel->setData(idx, QFileInfo(idx.data(SqliteExtensionEditorModel::FILE_PATH).toString()).baseName(), SqliteExtensionEditorModel::NAME);
        dataModel->setData(idx, validateExtension(idx), SqliteExtensionEditorModel::VALID);
    }
}

void SqliteExtensionEditor::help()
{
    static const QString url = QStringLiteral("https://github.com/pawelsalawa/letos/wiki/SQLite-extensions-manager");
    QDesktopServices::openUrl(QUrl(url, QUrl::StrictMode));
}

void SqliteExtensionEditor::commit()
{
    QModelIndex idx = getCurrentExtensionIndex();
    if (idx.isValid())
        extensionDeselected(idx);

    QList<SqliteExtensionManager::ExtensionPtr> extensions = dataModel->getExtensions();

    SQLITE_EXTENSIONS->setExtensions(extensions);
    dataModel->clearModified();
    currentModified = false;

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectExtension(idx);

    updateState();
    ui->extensionList->resizeColumnsToContents();
}

void SqliteExtensionEditor::rollback()
{
    QModelIndex idx = getCurrentExtensionIndex();

    dataModel->setData(SQLITE_EXTENSIONS->getAllExtensions());
    currentModified = false;
    clearEdits();

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectExtension(idx);

    initStateForAll();
    updateState();
}

void SqliteExtensionEditor::newExtension()
{
    dataModel->addExtension(SqliteExtensionManager::ExtensionPtr::create());
    QModelIndex idx = viewModel->index(viewModel->rowCount() - 1, 0);
    selectExtension(idx);
}

void SqliteExtensionEditor::deleteExtension()
{
    nameGenerationActive = false;

    QModelIndex idx = getCurrentExtensionIndex();
    dataModel->deleteExtension(idx);
    clearEdits();

    idx = getCurrentExtensionIndex();
    if (idx.isValid())
        extensionSelected(idx);
    else
        updateCurrentExtensionState();

    nameGenerationActive = true;
    updateState();
}

void SqliteExtensionEditor::updateState()
{
    bool modified = dataModel->isModified() || currentModified;
    bool valid = dataModel->isValid() && (!getCurrentExtensionIndex().isValid() || validateCurrentExtension());

    actionMap[COMMIT]->setEnabled(modified && valid);
    actionMap[ROLLBACK]->setEnabled(modified);
    actionMap[DELETE]->setEnabled(ui->extensionList->selectionModel()->selectedIndexes().size() > 0);
    ui->databaseList->setEnabled(ui->selectedDatabasesRadio->isChecked());
}

void SqliteExtensionEditor::updateCurrentExtensionState()
{
    QModelIndex idx = getCurrentExtensionIndex();
    ui->rightWidget->setEnabled(idx.isValid());
    if (!idx.isValid())
    {
        setValidState(ui->fileEdit, true);
        setValidState(ui->initEdit, true);
        return;
    }

    bool fileOk = true;
    bool initOk = true;
    QString fileError;
    bool allOk = validateExtension(&fileOk, &initOk, &fileError);

    // Display results
    setValidState(ui->fileEdit, fileOk, fileError);
    setValidState(ui->initEdit, initOk, tr("Invalid initialization function name. Function name can contain only alpha-numeric characters and underscore."));
    ui->databasesGroup->setEnabled(allOk);
    dataModel->setData(idx, allOk, SqliteExtensionEditorModel::VALID);

    updateState();
}

void SqliteExtensionEditor::extensionSelected(const QItemSelection& selected, const QItemSelection& deselected)
{
    int deselCnt = deselected.indexes().size();
    int selCnt = selected.indexes().size();

    if (deselCnt > 0)
        extensionDeselected(deselected.indexes().first());

    if (selCnt > 0)
        extensionSelected(selected.indexes().first());

    if (deselCnt > 0 && selCnt == 0)
    {
        currentModified = false;
        clearEdits();
    }
}

void SqliteExtensionEditor::updateModified()
{
    if (updatesForSelection)
        return;

    QModelIndex idx = getCurrentExtensionIndex();
    if (idx.isValid())
    {
        bool fileDiff = idx.data(SqliteExtensionEditorModel::FILE_PATH).toString() != ui->fileEdit->text();
        bool initDiff = idx.data(SqliteExtensionEditorModel::INIT_FUNC).toString() != ui->initEdit->text();
        bool allDatabasesDiff = idx.data(SqliteExtensionEditorModel::ALL_DATABASES).toBool() != ui->allDatabasesRadio->isChecked();
        bool dbDiff = toSet(getCurrentDatabases()) != toSet(idx.data(SqliteExtensionEditorModel::DATABASES).toStringList()); // QSet to ignore order

        currentModified = (fileDiff || initDiff || allDatabasesDiff || dbDiff);
    }

    statusUpdateTrigger->schedule();
}

void SqliteExtensionEditor::generateName()
{
    if (!nameGenerationActive)
        return;

    QModelIndex idx = getCurrentExtensionIndex();
    if (idx.isValid())
        dataModel->setData(idx, QFileInfo(ui->fileEdit->text()).baseName(), SqliteExtensionEditorModel::NAME);
}

void SqliteExtensionEditor::applyFilter(const QString& value)
{
    QModelIndex idx = getCurrentExtensionIndex();
    ui->extensionList->selectionModel()->clearSelection();

    viewModel->setFilterFixedString(value);

    selectExtension(idx);
}

void SqliteExtensionEditor::browseForFile()
{
    QString dir = getFileDialogInitPath();
    QString filter =
#if defined(Q_OS_WIN)
            tr("Dynamic link libraries (*.dll);;All files (*)");
#elif defined(Q_OS_LINUX)
            tr("Shared objects (*.so);;All files (*)");
#elif defined(Q_OS_OSX)
            tr("Dynamic libraries (*.dylib);;All files (*)");
#else
            tr("All files (*)");
#endif
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open file"), dir, filter);
    if (filePath.isNull())
        return;

    setFileDialogInitPathByFile(filePath);

    ui->fileEdit->setText(filePath);
}

void SqliteExtensionEditor::cfgExtensionListChanged()
{
    dataModel->setData(SQLITE_EXTENSIONS->getAllExtensions());
    initStateForAll();
    updateCurrentExtensionState();
}

void SqliteExtensionEditor::importExtensions()
{
    SettingsImportDialog::importFromFile(SettingsImportDialog::EXTENSION);
}

void SqliteExtensionEditor::exportExtensions()
{
    SettingsExportDialog::exportToFile(SettingsExportDialog::EXTENSION);
}

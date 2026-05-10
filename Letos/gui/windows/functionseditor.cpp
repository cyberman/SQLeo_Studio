#include "functionseditor.h"
#include "ui_functionseditor.h"
#include "common/utils.h"
#include "uiutils.h"
#include "functionseditormodel.h"
#include "services/pluginmanager.h"
#include "dbtree/dbtree.h"
#include "dbtree/dbtreemodel.h"
#include "iconmanager.h"
#include "syntaxhighlighterplugin.h"
#include "plugins/scriptingplugin.h"
#include "common/userinputfilter.h"
#include "selectabledbmodel.h"
#include "uiconfig.h"
#include "dialogs/settingsexportdialog.h"
#include "dialogs/settingsimportdialog.h"
#include <QDebug>
#include <QDesktopServices>
#include <QStyleFactory>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtSystemDetection>
#else
#include <qsystemdetection.h>
#endif
#include <QScopedValueRollback>
#include <QSyntaxHighlighter>

// TODO handle plugin loading/unloading to update editor state

CFG_KEYS_DEFINE(FunctionsEditor)

FunctionsEditor::FunctionsEditor(QWidget *parent) :
    MdiChild(parent),
    ui(new Ui::FunctionsEditor)
{
    init();
}

FunctionsEditor::~FunctionsEditor()
{
    delete ui;
}

bool FunctionsEditor::restoreSessionNextTime()
{
    return false;
}

bool FunctionsEditor::restoreSession(const QVariant &sessionValue)
{
    Q_UNUSED(sessionValue);
    return true;
}

Icon* FunctionsEditor::getIconNameForMdiWindow()
{
    return ICONS.FUNCTIONS_EDITOR;
}

QString FunctionsEditor::getTitleForMdiWindow()
{
    return tr("SQL functions editor");
}

void FunctionsEditor::createActions()
{
    createAction(COMMIT, ICONS.COMMIT, tr("Commit all function changes"), this, SLOT(commit()), ui->toolBar, this);
    createAction(ROLLBACK, ICONS.ROLLBACK, tr("Rollback all function changes"), this, SLOT(rollback()), ui->toolBar, this);
    ui->toolBar->addSeparator();
    createAction(ADD, ICONS.NEW_FUNCTION, tr("Create new function"), this, SLOT(newFunction()), ui->toolBar, this);
    createAction(DELETE, ICONS.DELETE_FUNCTION, tr("Delete selected function"), this, SLOT(deleteFunction()), ui->toolBar, this);
    ui->toolBar->addSeparator();
    createAction(IMPORT, ICONS.FUNCTIONS_IMPORT, tr("Import functions from file"), this, SLOT(importFunctions()), ui->toolBar, this);
    createAction(EXPORT, ICONS.FUNCTIONS_EXPORT, tr("Export functions to file"), this, SLOT(exportFunctions()), ui->toolBar, this);
    createAction(HELP, ICONS.HELP, tr("Custom SQL functions manual"), this, SLOT(help()), ui->toolBar, this);

    // Args toolbar
    createAction(ARG_ADD, ICONS.INSERT_FN_ARG, tr("Add function argument"), this, SLOT(addFunctionArg()), ui->argsToolBar, this);
    createAction(ARG_EDIT, ICONS.RENAME_FN_ARG, tr("Rename function argument"), this, SLOT(editFunctionArg()), ui->argsToolBar, this);
    createAction(ARG_DEL, ICONS.DELETE_FN_ARG, tr("Delete function argument"), this, SLOT(delFunctionArg()), ui->argsToolBar, this);
    ui->argsToolBar->addSeparator();
    createAction(ARG_MOVE_UP, ICONS.MOVE_UP, tr("Move function argument up"), this, SLOT(moveFunctionArgUp()), ui->argsToolBar, this);
    createAction(ARG_MOVE_DOWN, ICONS.MOVE_DOWN, tr("Move function argument down"), this, SLOT(moveFunctionArgDown()), ui->argsToolBar, this);

#ifdef Q_OS_MACX
    QStyle *fusion = QStyleFactory::create("Fusion");
    ui->toolBar->setStyle(fusion);
    ui->argsToolBar->setStyle(fusion);
#endif
}

void FunctionsEditor::setupDefShortcuts()
{
    // Widget context
    setShortcutContext({COMMIT, ROLLBACK}, Qt::WidgetWithChildrenShortcut);
    BIND_SHORTCUTS(FunctionsEditor, Action);
}

QToolBar* FunctionsEditor::getToolBar(int toolbar) const
{
    Q_UNUSED(toolbar);
    return ui->toolBar;
}

void FunctionsEditor::init()
{
    ui->setupUi(this);
    clearEdits();
    initCodeTabs();

    setFont(CFG_UI.Fonts.SqlEditor.get());

    dataModel = new FunctionsEditorModel(this);
    viewModel = new QSortFilterProxyModel(this);
    viewModel->setSourceModel(dataModel);
    ui->list->setModel(viewModel);
    ui->list->horizontalHeader()->setMinimumSectionSize(20);
    ui->list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->splitter->setSizes({1, 1});
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    Cfg::handleSplitterState(ui->splitter);
    Cfg::handleSplitterState(ui->splitter_2);

    dbListModel = new SelectableDbModel(this);
    dbListModel->setSourceModel(DBTREE->getModel());
    ui->databasesList->setModel(dbListModel);
    ui->databasesList->expandAll();

    for (auto t : {
         FunctionManager::ScriptFunction::SCALAR,
         FunctionManager::ScriptFunction::AGGREGATE,
         FunctionManager::ScriptFunction::AGG_WINDOW})
    {
        ui->typeCombo->addItem(FunctionManager::FunctionBase::displayString(t), t);
    }

    new UserInputFilter(ui->functionFilterEdit, this, SLOT(applyFilter(QString)));
    viewModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    MAINWINDOW->installToolbarSizeWheelHandler(ui->toolBar);

    initActions();
    setupContextMenu();

    connect(ui->list->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(functionSelected(QItemSelection,QItemSelection)));
    connect(ui->list->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(updateState()));
    connect(ui->initCodeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->inverseCodeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->stepCodeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->scalarCodeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->finalCodeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->nameEdit, SIGNAL(textChanged(QString)), this, SLOT(updateModified()));
    connect(ui->undefArgsCheck, SIGNAL(toggled(bool)), this, SLOT(updateModified()));
    connect(ui->allDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->selDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->langCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateModified()));
    connect(ui->typeCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateModified()));
    connect(ui->deterministicCheck, SIGNAL(toggled(bool)), this, SLOT(updateModified()));

    connect(ui->argsList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(updateArgsState()));
    connect(ui->argsList->model(), SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), this, SLOT(updateModified()));
    connect(ui->argsList->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateModified()));
    connect(ui->argsList->model(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateModified()));
    connect(ui->argsList->model(), SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateModified()));

    connect(dbListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateModified()));
    connect(CFG_UI.Fonts.SqlEditor, SIGNAL(changed(QVariant)), this, SLOT(changeFont(QVariant)));

    dataModel->setData(FUNCTIONS->getAllScriptFunctions());
    connect(FUNCTIONS, SIGNAL(functionListChanged()), this, SLOT(cfgFunctionListChanged()));
    ui->list->resizeColumnsToContents();

    // Language plugins
    for (ScriptingPlugin*& plugin : PLUGINS->getLoadedPlugins<ScriptingPlugin>())
        scriptingPlugins[plugin->getLanguage()] = plugin;

    ui->langCombo->addItems(scriptingPlugins.keys());

    // Syntax highlighting plugins
    for (SyntaxHighlighterPlugin*& plugin : PLUGINS->getLoadedPlugins<SyntaxHighlighterPlugin>())
        highlighterPlugins[plugin->getLanguageName()] = plugin;

    updateCurrentFunctionState();
    updateState();
}

void FunctionsEditor::initCodeTabs()
{
    tabIdx[SCALAR] = ui->codeTabs->indexOf(ui->scalarCodeTab);
    tabIdx[INIT] = ui->codeTabs->indexOf(ui->initCodeTab);
    tabIdx[STEP] = ui->codeTabs->indexOf(ui->stepCodeTab);
    tabIdx[INVERSE] = ui->codeTabs->indexOf(ui->inverseCodeTab);
    tabIdx[FINAL] = ui->codeTabs->indexOf(ui->finalCodeTab);

    ui->codeTabs->setTabVisible(tabIdx[INIT], false);
    ui->codeTabs->setTabVisible(tabIdx[STEP], false);
    ui->codeTabs->setTabVisible(tabIdx[INVERSE], false);
    ui->codeTabs->setTabVisible(tabIdx[FINAL], false);
}

void FunctionsEditor::setupContextMenu()
{
    auto formatPredicateFn = [this](QPlainTextEdit* editor)
    {
        return currentHighlighterLang == "SQL";
    };

    addFormatSqlToContextMenu(ui->scalarCodeEdit, formatPredicateFn);
    addFormatSqlToContextMenu(ui->initCodeEdit, formatPredicateFn);
    addFormatSqlToContextMenu(ui->inverseCodeEdit, formatPredicateFn);
    addFormatSqlToContextMenu(ui->stepCodeEdit, formatPredicateFn);
    addFormatSqlToContextMenu(ui->finalCodeEdit, formatPredicateFn);
}

QModelIndex FunctionsEditor::getCurrentFunctionIndex() const
{
    QModelIndexList idxList = ui->list->selectionModel()->selectedIndexes();
    if (idxList.size() == 0)
        return QModelIndex();

    return idxList.first();
}

void FunctionsEditor::functionDeselected(const QModelIndex& idx)
{
    QModelIndex srcIdx = viewModel->mapToSource(idx);

    viewModel->setData(idx, ui->nameEdit->text(), FunctionsEditorModel::NAME);
    viewModel->setData(idx, ui->undefArgsCheck->isChecked(), FunctionsEditorModel::UNDEF_ARGS);
    if (!ui->undefArgsCheck->isChecked())
        viewModel->setData(idx, getCurrentArgList(), FunctionsEditorModel::ARGUMENTS);

    viewModel->setData(idx, ui->langCombo->currentText(), FunctionsEditorModel::LANG);
    viewModel->setData(idx, getCurrentFunctionType(), FunctionsEditorModel::TYPE);
    viewModel->setData(idx, ui->allDatabasesRadio->isChecked(), FunctionsEditorModel::ALL_DATABASES);
    viewModel->setData(idx, ui->deterministicCheck->isChecked(), FunctionsEditorModel::DETERMINISTIC);
    viewModel->setData(idx, currentModified, FunctionsEditorModel::MODIFIED);

    if (dataModel->isAggregateWindow(srcIdx))
    {
        viewModel->setData(idx, ui->initCodeEdit->toPlainText(), FunctionsEditorModel::INIT_CODE);
        viewModel->setData(idx, ui->stepCodeEdit->toPlainText(), FunctionsEditorModel::STEP_CODE);
        viewModel->setData(idx, ui->inverseCodeEdit->toPlainText(), FunctionsEditorModel::INVERSE_CODE);
        viewModel->setData(idx, ui->finalCodeEdit->toPlainText(), FunctionsEditorModel::FINAL_CODE);
        // Do not clear "code" field in model, as it is shared for step code
    }
    else if (dataModel->isAggregate(srcIdx))
    {
        viewModel->setData(idx, ui->initCodeEdit->toPlainText(), FunctionsEditorModel::INIT_CODE);
        viewModel->setData(idx, ui->stepCodeEdit->toPlainText(), FunctionsEditorModel::STEP_CODE);
        viewModel->setData(idx, QString(), FunctionsEditorModel::INVERSE_CODE);
        viewModel->setData(idx, ui->finalCodeEdit->toPlainText(), FunctionsEditorModel::FINAL_CODE);
        // Do not clear "code" field in model, as it is shared for step code
    }
    else
    {
        viewModel->setData(idx, ui->scalarCodeEdit->toPlainText(), FunctionsEditorModel::CODE);
        viewModel->setData(idx, QString(), FunctionsEditorModel::INIT_CODE);
        viewModel->setData(idx, QString(), FunctionsEditorModel::INVERSE_CODE);
        viewModel->setData(idx, QString(), FunctionsEditorModel::FINAL_CODE);
    }

    if (ui->selDatabasesRadio->isChecked())
        viewModel->setData(idx, getCurrentDatabases(), FunctionsEditorModel::DATABASES);

    dataModel->validateNames();
}

void FunctionsEditor::functionSelected(const QModelIndex& idx)
{
    QScopedValueRollback<bool> selectionGuard(updatesForSelection, true);

    QModelIndex srcIdx = viewModel->mapToSource(idx);

    ui->nameEdit->setText(idx.data(FunctionsEditorModel::NAME).toString());
    if (dataModel->isAnyAggregate(srcIdx))
    {
        ui->stepCodeEdit->setPlainText(idx.data(FunctionsEditorModel::STEP_CODE).toString());
        ui->scalarCodeEdit->clear();
    }
    else
    {
        ui->scalarCodeEdit->setPlainText(idx.data(FunctionsEditorModel::CODE).toString());
        ui->stepCodeEdit->clear();
    }
    ui->initCodeEdit->setPlainText(idx.data(FunctionsEditorModel::INIT_CODE).toString());
    ui->inverseCodeEdit->setPlainText(idx.data(FunctionsEditorModel::INVERSE_CODE).toString());
    ui->finalCodeEdit->setPlainText(idx.data(FunctionsEditorModel::FINAL_CODE).toString());
    ui->undefArgsCheck->setChecked(idx.data(FunctionsEditorModel::UNDEF_ARGS).toBool());
    ui->langCombo->setCurrentText(idx.data(FunctionsEditorModel::LANG).toString());
    ui->deterministicCheck->setChecked(idx.data(FunctionsEditorModel::DETERMINISTIC).toBool());

    // Arguments
    ui->argsList->clear();
    QListWidgetItem* item = nullptr;
    for (const QString& arg : idx.data(FunctionsEditorModel::ARGUMENTS).toStringList())
    {
        item = new QListWidgetItem(arg);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->argsList->addItem(item);
    }

    // Databases
    dbListModel->setDatabases(idx.data(FunctionsEditorModel::DATABASES).toStringList());
    ui->databasesList->expandAll();

    if (idx.data(FunctionsEditorModel::ALL_DATABASES).toBool())
        ui->allDatabasesRadio->setChecked(true);
    else
        ui->selDatabasesRadio->setChecked(true);

    // Type
    int type = idx.data(FunctionsEditorModel::TYPE).toInt();
    for (int i = 0; i < ui->typeCombo->count(); i++)
    {
        if (ui->typeCombo->itemData(i).toInt() == type)
        {
            ui->typeCombo->setCurrentIndex(i);
            break;
        }
    }

    currentModified = idx.data(FunctionsEditorModel::MODIFIED).toBool();

    updateCurrentFunctionState();
}

void FunctionsEditor::clearEdits()
{
    ui->nameEdit->setText(QString());
    ui->scalarCodeEdit->setPlainText(QString());
    ui->initCodeEdit->setPlainText(QString());
    ui->stepCodeEdit->setPlainText(QString());
    ui->inverseCodeEdit->setPlainText(QString());
    ui->finalCodeEdit->setPlainText(QString());
    ui->langCombo->setCurrentText(QString());
    ui->undefArgsCheck->setChecked(true);
    ui->argsList->clear();
    ui->allDatabasesRadio->setChecked(true);
    ui->typeCombo->setCurrentIndex(0);
    ui->langCombo->setCurrentIndex(-1);
    ui->deterministicCheck->setChecked(false);
}

void FunctionsEditor::selectFunction(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    ui->list->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
}

void FunctionsEditor::setFont(const QFont& font)
{
    ui->scalarCodeEdit->setFont(font);
    ui->initCodeEdit->setFont(font);
    ui->stepCodeEdit->setFont(font);
    ui->inverseCodeEdit->setFont(font);
    ui->finalCodeEdit->setFont(font);
}

QModelIndex FunctionsEditor::getSelectedArg() const
{
    QModelIndexList indexes = ui->argsList->selectionModel()->selectedIndexes();
    if (indexes.size() == 0 || !indexes.first().isValid())
        return QModelIndex();

    return indexes.first();

}

QStringList FunctionsEditor::getCurrentArgList() const
{
    QStringList currArgList;
    for (int row = 0; row < ui->argsList->model()->rowCount(); row++)
        currArgList << ui->argsList->item(row)->text();

    return currArgList;
}

QStringList FunctionsEditor::getCurrentDatabases() const
{
    return dbListModel->getDatabases();
}

FunctionManager::ScriptFunction::Type FunctionsEditor::getCurrentFunctionType() const
{
    int intValue = ui->typeCombo->itemData(ui->typeCombo->currentIndex()).toInt();
    return static_cast<FunctionManager::ScriptFunction::Type>(intValue);
}

void FunctionsEditor::safeClearHighlighter(QSyntaxHighlighter*& highlighterPtr)
{
    // A pointers swap with local var - this is necessary, cause deleting highlighter
    // triggers textChanged on QPlainTextEdit, which then calls this method,
    // so it becomes an infinite recursion with deleting the same pointer.
    // We set the pointer to null first, then delete it. That way it's safe.
    QSyntaxHighlighter* highlighter = highlighterPtr;
    highlighterPtr = nullptr;
    delete highlighter;
}

void FunctionsEditor::commit()
{
    QModelIndex idx = getCurrentFunctionIndex();
    if (idx.isValid())
        functionDeselected(idx);

    QList<FunctionManager::ScriptFunction*> functions = dataModel->generateFunctions();

    FUNCTIONS->setScriptFunctions(functions);
    dataModel->clearModified();
    currentModified = false;

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectFunction(idx);

    updateState();
    ui->list->resizeColumnsToContents();
}

void FunctionsEditor::rollback()
{
    QModelIndex idx = getCurrentFunctionIndex();

    dataModel->setData(FUNCTIONS->getAllScriptFunctions());
    currentModified = false;
    clearEdits();

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectFunction(idx);

    updateState();
}

void FunctionsEditor::newFunction()
{
    if (ui->langCombo->currentIndex() == -1 && ui->langCombo->count() > 0)
        ui->langCombo->setCurrentIndex(0);

    FunctionManager::ScriptFunction* func = new FunctionManager::ScriptFunction();
    func->name = generateUniqueName("function", dataModel->getFunctionNames());

    if (ui->langCombo->currentIndex() > -1)
        func->lang = ui->langCombo->currentText();

    dataModel->addFunction(func);

    QModelIndex idx = viewModel->index(viewModel->rowCount() - 1, 0);
    selectFunction(idx);
}

void FunctionsEditor::deleteFunction()
{
    QModelIndex idx = getCurrentFunctionIndex();
    dataModel->deleteFunction(viewModel->mapToSource(idx));

    idx = getCurrentFunctionIndex();
    if (idx.isValid())
        functionSelected(idx);
    else
        clearEdits();

    updateState();
}

void FunctionsEditor::updateModified()
{
    if (updatesForSelection)
        return;

    QModelIndex idx = getCurrentFunctionIndex();
    if (idx.isValid())
    {
        QModelIndex srcIdx = viewModel->mapToSource(idx);

        bool nameDiff = idx.data(FunctionsEditorModel::NAME).toString() != ui->nameEdit->text();
        bool codeDiff = false;
        bool stepCodeDiff = false;
        if (dataModel->isAnyAggregate(srcIdx))
            stepCodeDiff = idx.data(FunctionsEditorModel::STEP_CODE).toString() != ui->stepCodeEdit->toPlainText();
        else
            codeDiff = idx.data(FunctionsEditorModel::CODE).toString() != ui->scalarCodeEdit->toPlainText();

        bool initCodeDiff = idx.data(FunctionsEditorModel::INIT_CODE).toString() != ui->initCodeEdit->toPlainText();
        bool inverseCodeDiff = idx.data(FunctionsEditorModel::INVERSE_CODE).toString() != ui->inverseCodeEdit->toPlainText();
        bool finalCodeDiff = idx.data(FunctionsEditorModel::FINAL_CODE).toString() != ui->finalCodeEdit->toPlainText();
        bool langDiff = idx.data(FunctionsEditorModel::LANG).toString() != ui->langCombo->currentText();
        bool undefArgsDiff = idx.data(FunctionsEditorModel::UNDEF_ARGS).toBool() != ui->undefArgsCheck->isChecked();
        bool allDatabasesDiff = idx.data(FunctionsEditorModel::ALL_DATABASES).toBool() != ui->allDatabasesRadio->isChecked();
        bool argDiff = getCurrentArgList() != idx.data(FunctionsEditorModel::ARGUMENTS).toStringList();
        bool dbDiff = toSet(getCurrentDatabases()) != toSet(idx.data(FunctionsEditorModel::DATABASES).toStringList()); // QSet to ignore order
        bool typeDiff = idx.data(FunctionsEditorModel::TYPE).toInt() != getCurrentFunctionType();
        bool deterministicDiff = idx.data(FunctionsEditorModel::DETERMINISTIC).toBool() != ui->deterministicCheck->isChecked();

        currentModified = (nameDiff || codeDiff || typeDiff || langDiff || undefArgsDiff || allDatabasesDiff || argDiff || dbDiff ||
                           initCodeDiff || finalCodeDiff || stepCodeDiff || inverseCodeDiff || deterministicDiff);

        if (langDiff)
            dataModel->setData(srcIdx, ui->langCombo->currentText(), FunctionsEditorModel::LANG);
    }

    updateCurrentFunctionState();
}

void FunctionsEditor::updateState()
{
    bool modified = dataModel->isModified() || currentModified;
    bool valid = dataModel->isValid();

    actionMap[COMMIT]->setEnabled(modified && valid);
    actionMap[ROLLBACK]->setEnabled(modified);
    actionMap[DELETE]->setEnabled(ui->list->selectionModel()->selectedIndexes().size() > 0);
}

void FunctionsEditor::updateCurrentFunctionState()
{
    QModelIndex idx = getCurrentFunctionIndex();
    ui->rightWidget->setEnabled(idx.isValid());
    if (!idx.isValid())
    {
        setValidState(ui->langCombo, true);
        setValidState(ui->nameEdit, true);
        setValidState(ui->scalarCodeTab, true);
        setValidState(ui->stepCodeTab, true);
        setValidState(ui->finalCodeTab, true);
        return;
    }

    QModelIndex srcIdx = viewModel->mapToSource(idx);

    QString name = ui->nameEdit->text();
    QStringList argList = getCurrentArgList();
    bool undefArgs = ui->undefArgsCheck->isChecked();
    bool nameOk = dataModel->isAllowedName(srcIdx, name, argList, undefArgs) && !name.trimmed().isEmpty();
    setValidState(ui->nameEdit, nameOk, tr("Enter a unique, non-empty function name. Duplicate names are allowed if the number of input parameters differs."));

    bool langOk = ui->langCombo->currentIndex() >= 0;
    ui->codeTabs->setEnabled(langOk);
    ui->argsGroup->setEnabled(langOk);
    ui->deterministicCheck->setEnabled(langOk);
    ui->databasesGroup->setEnabled(langOk);
    ui->nameEdit->setEnabled(langOk);
    ui->nameLabel->setEnabled(langOk);
    ui->typeCombo->setEnabled(langOk);
    ui->typeLabel->setEnabled(langOk);
    setValidState(ui->langCombo, langOk, tr("Pick the implementation language."));

    FunctionManager::FunctionBase::Type funType = getCurrentFunctionType();
    bool aggWindow = funType == FunctionManager::ScriptFunction::AGG_WINDOW;
    bool aggregate = funType == FunctionManager::ScriptFunction::AGGREGATE || aggWindow;
    ui->codeTabs->setTabVisible(tabIdx[SCALAR], !aggregate);
    ui->codeTabs->setTabVisible(tabIdx[INIT], aggregate);
    ui->codeTabs->setTabVisible(tabIdx[STEP], aggregate);
    ui->codeTabs->setTabVisible(tabIdx[INVERSE], aggWindow);
    ui->codeTabs->setTabVisible(tabIdx[FINAL], aggregate);

    ui->databasesList->setEnabled(ui->selDatabasesRadio->isChecked());

    // Declare mandatory code fields
    bool codeOk = true;
    if (!aggregate)
        codeOk = !ui->scalarCodeEdit->toPlainText().trimmed().isEmpty();

    setValidState(ui->scalarCodeTab, codeOk, tr("Enter a non-empty implementation code."));

    bool stepCodeOk = true;
    bool finalCodeOk = true;
    if (aggregate)
    {
        stepCodeOk = !ui->stepCodeEdit->toPlainText().trimmed().isEmpty();
        finalCodeOk = !ui->finalCodeEdit->toPlainText().trimmed().isEmpty();
    }

    setValidState(ui->stepCodeTab, stepCodeOk, tr("Enter a non-empty implementation code."));
    setValidState(ui->finalCodeTab, finalCodeOk, tr("Enter a non-empty implementation code."));

    // Syntax highlighter
    QString lang = ui->langCombo->currentText();
    if (lang != currentHighlighterLang)
    {
        safeClearHighlighter(currentScalarHighlighter);
        safeClearHighlighter(currentInitHighlighter);
        safeClearHighlighter(currentStepHighlighter);
        safeClearHighlighter(currentInverseHighlighter);
        safeClearHighlighter(currentFinalHighlighter);

        if (langOk && highlighterPlugins.contains(lang))
        {
            currentScalarHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->scalarCodeEdit);
            currentInitHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->initCodeEdit);
            currentStepHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->stepCodeEdit);
            currentInverseHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->inverseCodeEdit);
            currentFinalHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->finalCodeEdit);
        }

        currentHighlighterLang = lang;
    }

    bool argsOk = updateArgsState();
    dataModel->setData(srcIdx, langOk && codeOk && stepCodeOk && finalCodeOk && nameOk && argsOk, FunctionsEditorModel::VALID);
    updateState();
}

void FunctionsEditor::functionSelected(const QItemSelection& selected, const QItemSelection& deselected)
{
    int deselCnt = deselected.indexes().size();
    int selCnt = selected.indexes().size();

    if (deselCnt > 0)
        functionDeselected(deselected.indexes().first());

    if (selCnt > 0)
        functionSelected(selected.indexes().first());

    if (deselCnt > 0 && selCnt == 0)
    {
        currentModified = false;
        clearEdits();
    }
}

void FunctionsEditor::addFunctionArg()
{
    QListWidgetItem* item = new QListWidgetItem(tr("argument", "new function argument name in function editor window"));
    item->setFlags(item->flags () | Qt::ItemIsEditable);
    ui->argsList->addItem(item);

    QModelIndex idx = ui->argsList->model()->index(ui->argsList->model()->rowCount() - 1, 0);
    ui->argsList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent);

    ui->argsList->editItem(item);
}

void FunctionsEditor::editFunctionArg()
{
    QModelIndex selected = getSelectedArg();
    if (!selected.isValid())
        return;

    int row = selected.row();
    QListWidgetItem* item = ui->argsList->item(row);
    ui->argsList->editItem(item);
}

void FunctionsEditor::delFunctionArg()
{
    QModelIndex selected = getSelectedArg();
    if (!selected.isValid())
        return;

    int row = selected.row();
    delete ui->argsList->takeItem(row);
}

void FunctionsEditor::moveFunctionArgUp()
{
    QModelIndex selected = getSelectedArg();
    if (!selected.isValid())
        return;

    int row = selected.row();
    if (row <= 0)
        return;

    ui->argsList->insertItem(row - 1, ui->argsList->takeItem(row));

    QModelIndex idx = ui->argsList->model()->index(row - 1, 0);
    ui->argsList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent);
}

void FunctionsEditor::moveFunctionArgDown()
{
    QModelIndex selected = getSelectedArg();
    if (!selected.isValid())
        return;

    int row = selected.row();
    if (row >= ui->argsList->model()->rowCount() - 1)
        return;

    ui->argsList->insertItem(row + 1, ui->argsList->takeItem(row));

    QModelIndex idx = ui->argsList->model()->index(row + 1, 0);
    ui->argsList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent);
}

bool FunctionsEditor::updateArgsState()
{
    bool argsEnabled = !ui->undefArgsCheck->isChecked();
    QModelIndexList indexes = ui->argsList->selectionModel()->selectedIndexes();
    bool argSelected = indexes.size() > 0;

    bool canMoveUp = false;
    bool canMoveDown = false;
    if (argSelected)
    {
        canMoveUp = indexes.first().row() > 0;
        canMoveDown = (indexes.first().row() + 1) < ui->argsList->count();
    }

    actionMap[ARG_ADD]->setEnabled(argsEnabled);
    actionMap[ARG_EDIT]->setEnabled(argsEnabled && argSelected);
    actionMap[ARG_DEL]->setEnabled(argsEnabled && argSelected);
    actionMap[ARG_MOVE_UP]->setEnabled(argsEnabled && canMoveUp);
    actionMap[ARG_MOVE_DOWN]->setEnabled(argsEnabled && canMoveDown);
    ui->argsList->setEnabled(argsEnabled);

    if (argsEnabled)
    {
        bool argsOk = true;
        QSet<QString> usedNames;
        for (int rowIdx = 0; rowIdx < ui->argsList->model()->rowCount(); rowIdx++)
        {
            QListWidgetItem* item = ui->argsList->item(rowIdx);
            QString argName = item->text().toLower();
            if (argName.isEmpty())
            {
                argsOk = false;
                break;
            }
            if (usedNames.contains(argName))
            {
                argsOk = false;
                break;
            }
            usedNames << argName;
        }
        setValidState(ui->argsList, argsOk, tr("Function argument cannot be empty and it cannot have duplicated name."));
        return argsOk;
    }
    else
        return true;
}

void FunctionsEditor::applyFilter(const QString& value)
{
    // Remembering old selection, clearing it and restoring afterwards is a workaround for a problem,
    // which causes application to crash, when the item was selected, but after applying filter string,
    // item was about to disappear.
    // This must have something to do with the underlying model (FunctionsEditorModel) implementation,
    // but for now I don't really know what is that.
    // I have tested simple Qt application with the same routine, but the underlying model was QStandardItemModel
    // and everything worked fine.
    QModelIndex idx = getCurrentFunctionIndex();
    ui->list->selectionModel()->clearSelection();

    viewModel->setFilterFixedString(value);

    selectFunction(idx);
}

void FunctionsEditor::help()
{
    static const QString url = QStringLiteral("https://github.com/pawelsalawa/letos/wiki/Custom-SQL-Functions");
    QDesktopServices::openUrl(QUrl(url, QUrl::StrictMode));
}

void FunctionsEditor::changeFont(const QVariant& font)
{
    setFont(font.value<QFont>());
}

void FunctionsEditor::cfgFunctionListChanged()
{
    if (dataModel->isModified())
        return; // Don't update list if there are uncommitted changes, because it would be disruptive for user. Changes will be visible after commit or rollback.

    dataModel->setData(FUNCTIONS->getAllScriptFunctions());
    updateCurrentFunctionState();
}

void FunctionsEditor::importFunctions()
{
    SettingsImportDialog::importFromFile(SettingsImportDialog::FUNCTION);
}

void FunctionsEditor::exportFunctions()
{
    SettingsExportDialog::exportToFile(SettingsExportDialog::FUNCTION);
}

QVariant FunctionsEditor::saveSession()
{
    return QVariant();
}


bool FunctionsEditor::isUncommitted() const
{
    return dataModel->isModified() || currentModified;
}

QString FunctionsEditor::getQuitUncommittedConfirmMessage() const
{
    return tr("Functions editor window has uncommitted modifications.");
}

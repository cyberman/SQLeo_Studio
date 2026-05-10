#include "collationseditor.h"
#include "dialogs/settingsexportdialog.h"
#include "dialogs/settingsimportdialog.h"
#include "ui_collationseditor.h"
#include "selectabledbmodel.h"
#include "dbtree/dbtree.h"
#include "collationseditormodel.h"
#include "common/utils.h"
#include "uiutils.h"
#include "services/pluginmanager.h"
#include "syntaxhighlighterplugin.h"
#include "plugins/scriptingplugin.h"
#include "uiconfig.h"
#include "common/userinputfilter.h"
#include "dbtree/dbtreemodel.h"
#include <QDesktopServices>
#include <QSyntaxHighlighter>

CFG_KEYS_DEFINE(CollationsEditor)

CollationsEditor::CollationsEditor(QWidget *parent) :
    MdiChild(parent),
    ui(new Ui::CollationsEditor)
{
    init();
}

CollationsEditor::~CollationsEditor()
{
    delete ui;
}

bool CollationsEditor::restoreSessionNextTime()
{
    return false;
}

QVariant CollationsEditor::saveSession()
{
    return QVariant();
}

bool CollationsEditor::restoreSession(const QVariant& sessionValue)
{
    Q_UNUSED(sessionValue);
    return true;
}

Icon* CollationsEditor::getIconNameForMdiWindow()
{
    return ICONS.COLLATIONS_EDITOR;
}

QString CollationsEditor::getTitleForMdiWindow()
{
    return tr("Collations editor");
}

void CollationsEditor::createActions()
{
    createAction(COMMIT, ICONS.COMMIT, tr("Commit all collation changes"), this, SLOT(commit()), ui->toolbar, this);
    createAction(ROLLBACK, ICONS.ROLLBACK, tr("Rollback all collation changes"), this, SLOT(rollback()), ui->toolbar, this);
    ui->toolbar->addSeparator();
    createAction(ADD, ICONS.NEW_COLLATION, tr("Create new collation"), this, SLOT(newCollation()), ui->toolbar, this);
    createAction(DELETE, ICONS.DELETE_COLLATION, tr("Delete selected collation"), this, SLOT(deleteCollation()), ui->toolbar, this);
    ui->toolbar->addSeparator();
    createAction(IMPORT, ICONS.COLLATIONS_IMPORT, tr("Import collations from file"), this, SLOT(importCollations()), ui->toolbar, this);
    createAction(EXPORT, ICONS.COLLATIONS_EXPORT, tr("Export collations to file"), this, SLOT(exportCollations()), ui->toolbar, this);
    createAction(HELP, ICONS.HELP, tr("Editing collations manual"), this, SLOT(help()), ui->toolbar, this);
}

void CollationsEditor::setupDefShortcuts()
{
    // Widget context
    setShortcutContext({COMMIT, ROLLBACK}, Qt::WidgetWithChildrenShortcut);
    BIND_SHORTCUTS(CollationsEditor, Action);
}

QToolBar* CollationsEditor::getToolBar(int toolbar) const
{
    Q_UNUSED(toolbar);
    return ui->toolbar;
}

void CollationsEditor::init()
{
    ui->setupUi(this);
    initActions();
    setupContextMenu();

    setFont(CFG_UI.Fonts.SqlEditor.get());

    dataModel = new CollationsEditorModel(this);
    viewModel = new QSortFilterProxyModel(this);
    viewModel->setSourceModel(dataModel);
    ui->collationList->setModel(viewModel);
    ui->collationList->horizontalHeader()->setMinimumSectionSize(20);
    ui->collationList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->splitter->setSizes({1, 1});
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    Cfg::handleSplitterState(ui->splitter);
    Cfg::handleSplitterState(ui->splitter_2);

    dbListModel = new SelectableDbModel(this);
    dbListModel->setSourceModel(DBTREE->getModel());
    ui->databaseList->setModel(dbListModel);
    ui->databaseList->expandAll();

    dataModel->setData(COLLATIONS->getAllCollations());
    ui->collationList->resizeColumnsToContents();

    MAINWINDOW->installToolbarSizeWheelHandler(ui->toolbar);

    new UserInputFilter(ui->collationFilterEdit, this, SLOT(applyFilter(QString)));
    viewModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(ui->collationList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(collationSelected(QItemSelection,QItemSelection)));
    connect(ui->collationList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(updateState()));
    connect(ui->codeEdit, SIGNAL(textChanged()), this, SLOT(updateModified()));
    connect(ui->nameEdit, SIGNAL(textChanged(QString)), this, SLOT(updateModified()));
    connect(ui->allDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->selectedDatabasesRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->functionBasedRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->extensionBasedRadio, SIGNAL(clicked()), this, SLOT(updateModified()));
    connect(ui->langCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(updateModified()));

    connect(dbListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateModified()));
    connect(CFG_UI.Fonts.SqlEditor, SIGNAL(changed(QVariant)), this, SLOT(changeFont(QVariant)));
    connect(COLLATIONS, SIGNAL(collationListChanged()), this, SLOT(cfgCollationListChanged()));

    updateLangCombo();

    // Syntax highlighting plugins
    for (SyntaxHighlighterPlugin* plugin : PLUGINS->getLoadedPlugins<SyntaxHighlighterPlugin>())
        highlighterPlugins[plugin->getLanguageName()] = plugin;

    updateCurrentCollationState();
    updateState();
}

QModelIndex CollationsEditor::getCurrentCollationIdx() const
{
    QModelIndexList idxList = ui->collationList->selectionModel()->selectedIndexes();
    if (idxList.size() == 0)
        return QModelIndex();

    return idxList.first();
}

CollationManager::CollationType CollationsEditor::getCurrentType() const
{
    return ui->extensionBasedRadio->isChecked() ? CollationManager::CollationType::EXTENSION_BASED
                                                : CollationManager::CollationType::FUNCTION_BASED;
}

void CollationsEditor::collationDeselected(const QModelIndex& idx)
{
    viewModel->setData(idx, ui->nameEdit->text(), CollationsEditorModel::NAME);
    viewModel->setData(idx, getCurrentType(), CollationsEditorModel::TYPE);
    viewModel->setData(idx, ui->langCombo->currentText(), CollationsEditorModel::LANG);
    viewModel->setData(idx, ui->allDatabasesRadio->isChecked(), CollationsEditorModel::ALL_DATABASES);
    viewModel->setData(idx, ui->codeEdit->toPlainText(), CollationsEditorModel::CODE);
    viewModel->setData(idx, currentModified, CollationsEditorModel::MODIFIED);

    if (ui->selectedDatabasesRadio->isChecked())
        viewModel->setData(idx, getCurrentDatabases(), CollationsEditorModel::DATABASES);

    dataModel->validateNames();
}

void CollationsEditor::collationSelected(const QModelIndex& idx)
{
    updatesForSelection = true;

    int type = idx.data(CollationsEditorModel::TYPE).toInt();

    ui->nameEdit->setText(idx.data(CollationsEditorModel::NAME).toString());
    ui->codeEdit->setPlainText(idx.data(CollationsEditorModel::CODE).toString());
    ui->functionBasedRadio->setChecked(type == CollationManager::CollationType::FUNCTION_BASED);
    ui->extensionBasedRadio->setChecked(type == CollationManager::CollationType::EXTENSION_BASED);
    ui->langCombo->setCurrentText(idx.data(CollationsEditorModel::LANG).toString());

    // Databases
    dbListModel->setDatabases(idx.data(CollationsEditorModel::DATABASES).toStringList());
    ui->databaseList->expandAll();

    if (idx.data(CollationsEditorModel::ALL_DATABASES).toBool())
        ui->allDatabasesRadio->setChecked(true);
    else
        ui->selectedDatabasesRadio->setChecked(true);

    updatesForSelection = false;
    currentModified = idx.data(CollationsEditorModel::MODIFIED).toBool();

    updateCurrentCollationState();
}

void CollationsEditor::clearEdits()
{
    ui->nameEdit->setText(QString());
    ui->codeEdit->setPlainText(QString());
    ui->langCombo->setCurrentText(QString());
    ui->allDatabasesRadio->setChecked(true);
    ui->langCombo->setCurrentIndex(-1);
}

void CollationsEditor::selectCollation(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    ui->collationList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear|QItemSelectionModel::SelectCurrent|QItemSelectionModel::Rows);
}

QStringList CollationsEditor::getCurrentDatabases() const
{
    return dbListModel->getDatabases();
}

void CollationsEditor::setFont(const QFont& font)
{
    ui->codeEdit->setFont(font);
}

void CollationsEditor::help()
{
    static const QString url = QStringLiteral("https://github.com/pawelsalawa/letos/wiki/Custom-collation-sequences");
    QDesktopServices::openUrl(QUrl(url, QUrl::StrictMode));
}

void CollationsEditor::commit()
{
    QModelIndex idx = getCurrentCollationIdx();
    if (idx.isValid())
        collationDeselected(idx);

    QList<CollationManager::CollationPtr> collations = dataModel->getCollations();

    COLLATIONS->setCollations(collations);
    dataModel->clearModified();
    currentModified = false;

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectCollation(idx);

    updateState();
    ui->collationList->resizeColumnsToContents();
}

void CollationsEditor::rollback()
{
    QModelIndex idx = getCurrentCollationIdx();

    dataModel->setData(COLLATIONS->getAllCollations());
    currentModified = false;
    clearEdits();

    idx = viewModel->index(idx.row(), idx.column());
    if (idx.isValid())
        selectCollation(idx);

    updateState();
}

void CollationsEditor::newCollation()
{
    if (ui->langCombo->currentIndex() == -1 && ui->langCombo->count() > 0)
        ui->langCombo->setCurrentIndex(0);

    CollationManager::CollationPtr coll = CollationManager::CollationPtr::create();
    coll->name = generateUniqueName("collation", dataModel->getCollationNames());
    coll->type = getCurrentType();
    if (ui->langCombo->currentIndex() > -1)
        coll->lang = ui->langCombo->currentText();

    dataModel->addCollation(coll);

    QModelIndex idx = viewModel->index(viewModel->rowCount() - 1, 0);
    selectCollation(idx);
}

void CollationsEditor::deleteCollation()
{
    QModelIndex idx = getCurrentCollationIdx();
    dataModel->deleteCollation(idx);
    clearEdits();

    idx = getCurrentCollationIdx();
    if (idx.isValid())
        collationSelected(idx);

    updateState();
}

void CollationsEditor::updateState()
{
    bool modified = dataModel->isModified() || currentModified;
    bool valid = dataModel->isValid();

    actionMap[COMMIT]->setEnabled(modified && valid);
    actionMap[ROLLBACK]->setEnabled(modified);
    actionMap[DELETE]->setEnabled(ui->collationList->selectionModel()->selectedIndexes().size() > 0);
}

void CollationsEditor::updateCurrentCollationState()
{
    QModelIndex idx = getCurrentCollationIdx();
    ui->rightWidget->setEnabled(idx.isValid());
    if (!idx.isValid())
    {
        setValidState(ui->langCombo, true);
        setValidState(ui->nameEdit, true);
        setValidState(ui->codeEdit, true);
        return;
    }

    QString name = ui->nameEdit->text();
    bool nameOk = dataModel->isAllowedName(idx, name) && !name.trimmed().isEmpty();
    setValidState(ui->nameEdit, nameOk, tr("Enter a non-empty, unique name of the collation."));

    updateLangCombo();

    bool langOk = ui->langCombo->currentIndex() >= 0;
    ui->codeGroup->setEnabled(langOk);
    ui->databasesGroup->setEnabled(langOk);
    ui->nameEdit->setEnabled(langOk);
    ui->nameLabel->setEnabled(langOk);
    ui->databaseList->setEnabled(ui->selectedDatabasesRadio->isChecked());
    setValidState(ui->langCombo, langOk, tr("Pick the implementation language."));

    bool codeOk = !ui->codeEdit->toPlainText().trimmed().isEmpty();
    if (ui->extensionBasedRadio->isChecked())
    {
        ui->codeGroup->setTitle(tr("Registration code"));
        setValidState(ui->codeEdit, codeOk, tr("Enter a non-empty registration code."));
        ui->codeEdit->setToolTip(R"(<html><head/><body><p>
            Code executed to register the collation in a database. It is SQL code that should contain a single statement that creates collation, for example:
            <span style=" font-family:'monospace';">SELECT icu_load_collation('pl_PL', 'POLSKI');</span>
            </p></body></html>)");
    }
    else
    {
        ui->codeGroup->setTitle(tr("Implementation code"));
        setValidState(ui->codeEdit, codeOk, tr("Enter a non-empty implementation code."));
        ui->codeEdit->setToolTip(R"(<html><head/><body><p>
            Code executed when the collation is applied. It receives two arguments, <span style=" font-family:'monospace';">first</span>
            and <span style=" font-family:'monospace';">second</span> (named according to the scripting language conventions),
            representing the values to compare. The code should compare these values and return an integer indicating the result:
            negative if <span style=" font-family:'monospace';">first &lt; second</span>, zero if equal,
            and positive if <span style=" font-family:'monospace';">first &gt; second</span>.
            </p></body></html>)");
    }

    // Syntax highlighter
    QString lang = ui->langCombo->currentText();
    if (lang != currentHighlighterLang)
    {
        QSyntaxHighlighter* highlighter = nullptr;
        if (currentHighlighter)
        {
            // A pointers swap with local var - this is necessary, cause deleting highlighter
            // triggers textChanged on QPlainTextEdit, which then calls this method,
            // so it becomes an infinite recursion with deleting the same pointer.
            // We set the pointer to null first, then delete it. That way it's safe.
            highlighter = currentHighlighter;
            currentHighlighter = nullptr;
            delete highlighter;
        }

        if (langOk && highlighterPlugins.contains(lang))
        {
            currentHighlighter = highlighterPlugins[lang]->createSyntaxHighlighter(ui->codeEdit);
        }

        currentHighlighterLang = lang;
    }
    dataModel->setData(idx, langOk && codeOk && nameOk, CollationsEditorModel::VALID);
    updateState();
}

void CollationsEditor::collationSelected(const QItemSelection& selected, const QItemSelection& deselected)
{
    int deselCnt = deselected.indexes().size();
    int selCnt = selected.indexes().size();

    if (deselCnt > 0)
        collationDeselected(deselected.indexes().first());

    if (selCnt > 0)
        collationSelected(selected.indexes().first());

    if (deselCnt > 0 && selCnt == 0)
    {
        currentModified = false;
        clearEdits();
    }
}

void CollationsEditor::updateLangCombo()
{
    QComboBox *combo = ui->langCombo;
    bool alreadyInternalUpdate = updatesForSelection;
    updatesForSelection = true;
    if (ui->extensionBasedRadio->isChecked())
    {
        if (combo->isEnabled())
        {
            combo->setEnabled(false);
            combo->clear();
            combo->addItem("SQL");
            combo->setCurrentIndex(0);
        }
    }
    else
    {
        if (!combo->isEnabled() || combo->count() == 0)
        {
            combo->clear();
            for (ScriptingPlugin* plugin : PLUGINS->getLoadedPlugins<ScriptingPlugin>())
                combo->addItem(plugin->getLanguage());
            combo->setEnabled(true);
        }
    }
    updatesForSelection = alreadyInternalUpdate;
}

void CollationsEditor::setupContextMenu()
{
    auto formatPredicateFn = [this](QPlainTextEdit* editor)
    {
        return currentHighlighterLang == "SQL";
    };

    addFormatSqlToContextMenu(ui->codeEdit, formatPredicateFn);
}

void CollationsEditor::updateModified()
{
    if (updatesForSelection)
        return;

    QModelIndex idx = getCurrentCollationIdx();
    if (idx.isValid())
    {
        bool nameDiff = idx.data(CollationsEditorModel::NAME).toString() != ui->nameEdit->text();
        bool codeDiff = idx.data(CollationsEditorModel::CODE).toString() != ui->codeEdit->toPlainText();
        bool typeDiff = idx.data(CollationsEditorModel::TYPE).toInt() != getCurrentType();
        bool langDiff = idx.data(CollationsEditorModel::LANG).toString() != ui->langCombo->currentText();
        bool allDatabasesDiff = idx.data(CollationsEditorModel::ALL_DATABASES).toBool() != ui->allDatabasesRadio->isChecked();
        bool dbDiff = toSet(getCurrentDatabases()) != toSet(idx.data(CollationsEditorModel::DATABASES).toStringList()); // QSet to ignore order

        currentModified = (nameDiff || codeDiff || typeDiff || langDiff || allDatabasesDiff || dbDiff);
    }

    updateCurrentCollationState();
}

void CollationsEditor::applyFilter(const QString& value)
{
    //
    // See FunctionsEditor::applyFilter() for details why we remember current selection and restore it at the end.
    //

    QModelIndex idx = getCurrentCollationIdx();
    ui->collationList->selectionModel()->clearSelection();

    viewModel->setFilterFixedString(value);

    selectCollation(idx);
}

void CollationsEditor::changeFont(const QVariant& font)
{
    setFont(font.value<QFont>());
}

void CollationsEditor::cfgCollationListChanged()
{
    if (dataModel->isModified())
        return; // Don't update list if there are uncommitted changes, because it would be disruptive for user. Changes will be visible after commit or rollback.

    dataModel->setData(COLLATIONS->getAllCollations());
    updateCurrentCollationState();
}

void CollationsEditor::importCollations()
{
    SettingsImportDialog::importFromFile(SettingsImportDialog::COLLATION);
}

void CollationsEditor::exportCollations()
{
    SettingsExportDialog::exportToFile(SettingsExportDialog::COLLATION);
}

bool CollationsEditor::isUncommitted() const
{
    return dataModel->isModified() || currentModified;
}

QString CollationsEditor::getQuitUncommittedConfirmMessage() const
{
    return tr("Collations editor window has uncommitted modifications.");
}

#include "execfromfiledialog.h"
#include "dblistmodel.h"
#include "services/config.h"
#include "ui_execfromfiledialog.h"
#include "common/utils.h"
#include "uiconfig.h"
#include "uiutils.h"
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>

static const QString EXEC_SQL_FILE_DIALOG_CFG_GROUP = "ExecFromFileDialog";
static const QString EXEC_SQL_FILE_DIALOG_CFG_CODEC = "codec";
static const QString EXEC_SQL_FILE_DIALOG_CFG_FILE = "inputFileName";
static const QString EXEC_SQL_FILE_DIALOG_CFG_IGNORE_ERR = "ignoreErrors";
static const QString EXEC_SQL_FILE_DIALOG_CFG_MODE = "mode";

ExecFromFileDialog::ExecFromFileDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExecFromFileDialog)
{
    init();
}

ExecFromFileDialog::~ExecFromFileDialog()
{
    delete ui;
}

bool ExecFromFileDialog::ignoreErrors() const
{
    return ui->skipErrorsCheck->isChecked();
}

QString ExecFromFileDialog::filePath() const
{
    return ui->fileEdit->text();
}

QString ExecFromFileDialog::codec() const
{
    return ui->encodingCombo->currentText();
}

SqlFileExecutor::ExecutionMode ExecFromFileDialog::getExecutionMode() const
{
    if (ui->permModeRadio->isChecked())
        return SqlFileExecutor::PERMISSIVE;

    if (ui->strictModeRadio->isChecked())
        return SqlFileExecutor::STRICT_MODE;

    if (ui->extModeRadio->isChecked())
        return SqlFileExecutor::EXTENDED;

    qCritical() << "Unknown execution mode in ExecFromFileDialog";
    return SqlFileExecutor::PERMISSIVE;
}

void ExecFromFileDialog::selectDb(Db *db)
{
    if (!db->isOpen())
        return;

    ui->dbCombo->setCurrentIndex(dbListModel->getIndexForDb(db));
}

Db *ExecFromFileDialog::selectedDb() const
{
    return dbListModel->getDb(ui->dbCombo->currentIndex());
}

void ExecFromFileDialog::init()
{
    ui->setupUi(this);

    dbListModel = new DbListModel(this);
    dbListModel->setCombo(ui->dbCombo);
    dbListModel->setSortMode(DbListModel::SortMode::AlphabeticalCaseInsensitive);
    ui->dbCombo->setModel(dbListModel);

    ui->permModeRadio->setChecked(true);

    connect(ui->fileBrowse, SIGNAL(clicked()), this, SLOT(browseForInputFile()));
    connect(ui->fileEdit, SIGNAL(textChanged(const QString&)), this, SLOT(updateState()));

    ui->encodingCombo->addItems(textCodecNames());
    ui->encodingCombo->setCurrentText(defaultCodecName());

    readStdConfig();
}

void ExecFromFileDialog::storeStdConfig()
{
    CFG->begin();
    CFG->set(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_CODEC, ui->encodingCombo->currentText());
    CFG->set(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_FILE, ui->fileEdit->text());
    CFG->set(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_IGNORE_ERR, ui->skipErrorsCheck->isChecked());
    CFG->set(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_MODE, static_cast<int>(getExecutionMode()));
    CFG->commit();
}

void ExecFromFileDialog::readStdConfig()
{
    int mode = CFG->get(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_MODE, -1).toInt();
    switch (static_cast<SqlFileExecutor::ExecutionMode>(mode))
    {
        case SqlFileExecutor::STRICT_MODE:
            ui->strictModeRadio->setChecked(true);
            break;
        case SqlFileExecutor::PERMISSIVE:
            ui->permModeRadio->setChecked(true);
            break;
        case SqlFileExecutor::EXTENDED:
            ui->extModeRadio->setChecked(true);
            break;
        default:
            break;
    }

    ui->fileEdit->setText(CFG->get(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_FILE, QString()).toString());
    ui->skipErrorsCheck->setChecked(CFG->get(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_IGNORE_ERR, false).toBool());

    QString codec = CFG->get(EXEC_SQL_FILE_DIALOG_CFG_GROUP, EXEC_SQL_FILE_DIALOG_CFG_CODEC).toString();
    QString defaultCodec = defaultCodecName();
    if (codec.isNull())
        codec = defaultCodec;

    int codecIdx = ui->encodingCombo->findText(codec);
    if (codecIdx == -1 && codec != defaultCodec)
    {
        codec = defaultCodec;
        codecIdx = ui->encodingCombo->findText(codec);
    }
    if (codecIdx > -1)
        ui->encodingCombo->setCurrentIndex(codecIdx);
}

void ExecFromFileDialog::accept()
{
    storeStdConfig();
    QDialog::accept();
}

void ExecFromFileDialog::browseForInputFile()
{
    QString dir = getFileDialogInitPath();
    QString filters = tr("SQL scripts (*.sql);;All files (*)");
    QString path = QFileDialog::getOpenFileName(nullptr, tr("Execute SQL file"), dir, filters);
    if (path.isNull())
        return;

    setFileDialogInitPathByFile(path);
    ui->fileEdit->setText(path);
    updateState();
}

void ExecFromFileDialog::updateState()
{
    bool fileOk = true;
    QString path = ui->fileEdit->text();
    if (path.isEmpty())
    {
        setValidState(ui->fileEdit, false, tr("Please provide file to be executed."));
        fileOk = false;
    }

    if (fileOk)
    {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isReadable())
        {
            setValidState(ui->fileEdit, false, tr("Provided file does not exist or cannot be read."));
            fileOk = false;
        }
    }

    if (fileOk)
        setValidState(ui->fileEdit, true);

    bool dbOk = ui->dbCombo->currentIndex() >= 0;
    if (dbOk)
        setValidState(ui->dbCombo, true);
    else
        setValidState(ui->dbCombo, false, tr("Select a database to execute the SQL file on."));

   ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(dbOk && fileOk);
}

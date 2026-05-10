#include "sqliteextensioneditormodel.h"
#include "iconmanager.h"
#include <QFileInfo>

SqliteExtensionEditorModel::SqliteExtensionEditorModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

void SqliteExtensionEditorModel::clearModified()
{
    beginResetModel();
    for (Extension* ext : extensionList)
        ext->modified = false;

    listModified = false;
    originalExtensionList = extensionList;
    endResetModel();
}

bool SqliteExtensionEditorModel::isModified() const
{
    if (extensionList != originalExtensionList)
        return true;

    for (Extension* ext : extensionList)
    {
        if (ext->modified)
            return true;
    }
    return false;
}

bool SqliteExtensionEditorModel::isValid() const
{
    for (Extension* ext : extensionList)
    {
        if (ext->modified && !ext->valid)
            return false;
    }
    return true;
}

void SqliteExtensionEditorModel::setData(const QList<SqliteExtensionManager::ExtensionPtr>& extensions)
{
    beginResetModel();

    for (Extension* extPtr : extensionList)
        delete extPtr;

    extensionList.clear();

    for (const SqliteExtensionManager::ExtensionPtr& ext : extensions)
        extensionList << new Extension(ext);

    listModified = false;
    originalExtensionList = extensionList;

    endResetModel();
}

void SqliteExtensionEditorModel::addExtension(const SqliteExtensionManager::ExtensionPtr& extension)
{
    int row = extensionList.size();

    beginInsertRows(QModelIndex(), row, row);

    extensionList << new Extension(extension);
    listModified = true;

    endInsertRows();
}

void SqliteExtensionEditorModel::deleteExtension(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());

    delete extensionList[idx.row()];
    extensionList.removeAt(idx.row());

    listModified = true;

    endRemoveRows();
}

QList<SqliteExtensionManager::ExtensionPtr> SqliteExtensionEditorModel::getExtensions() const
{
    QList<SqliteExtensionManager::ExtensionPtr> results;
    for (Extension* ext : extensionList)
        results << ext->data;

    return results;
}

int SqliteExtensionEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return extensionList.size();
}

int SqliteExtensionEditorModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant SqliteExtensionEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (index.column() < 0 || index.column() > 1)
        return {};

    if (index.row() < 0 || index.row() >= extensionList.size())
        return {};

    auto* ext = extensionList[index.row()];
    switch (index.column())
    {
        case 0:
        {
            switch (role)
            {
                case Qt::DisplayRole:
                    return ext->name;
                case Qt::DecorationRole:
                    return ext->valid ? ICONS.EXTENSION : ICONS.EXTENSION_ERROR;
                case NAME:
                    return ext->name;
                case MODIFIED:
                    return ext->modified;
                case VALID:
                    return ext->valid;
                case INIT_FUNC:
                    return ext->data->initFunc;
                case FILE_PATH:
                    return ext->data->filePath;
                case DATABASES:
                    return ext->data->databases;
                case ALL_DATABASES:
                    return ext->data->allDatabases;
            }
            break;
        }
        case 1:
        {
            if (role == Qt::DisplayRole)
                return ext->data->allDatabases ? "*" : QString::number(ext->data->databases.size());

            break;
        }
    }

    if (role == Qt::ToolTipRole)
    {
        QString dbPart = ext->data->allDatabases ? tr("all databases") : ext->data->databases.join(", ");

        static_qstring(rowTpl, "<tr><td align='right'>%1</td><td><b>%2</b></td></tr>");
        return "<table>" +
               rowTpl.arg(tr("Extension:"), ext->name) +
               rowTpl.arg(tr("File:"), ext->data->filePath) +
               rowTpl.arg(tr("Init function:"), ext->data->initFunc) +
               rowTpl.arg(tr("Registered in:"), dbPart) +
               "</table>";
    }

    return QVariant();
}

bool SqliteExtensionEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (index.column() != 0)
        return false;

    if (index.row() < 0 || index.row() >= extensionList.size())
        return false;

    auto* ext = extensionList[index.row()];
    switch (role)
    {
        case NAME:
            ext->name = value.toString();
            break;
        case MODIFIED:
            ext->modified = value.toBool();
            break;
        case VALID:
            ext->valid = value.toBool();
            break;
        case INIT_FUNC:
            ext->data->initFunc = value.toString();
            break;
        case FILE_PATH:
            ext->data->filePath = value.toString();
            break;
        case DATABASES:
            ext->data->databases = value.toStringList();
            break;
        case ALL_DATABASES:
            ext->data->allDatabases = value.toBool();
            break;
        defaut:
            return true;
    }

    emit dataChanged(index, index);
    return true;
}

SqliteExtensionEditorModel::Extension::Extension()
{
    data = SqliteExtensionManager::ExtensionPtr::create();
}

SqliteExtensionEditorModel::Extension::Extension(const SqliteExtensionManager::ExtensionPtr& other)
{
    data = SqliteExtensionManager::ExtensionPtr::create(*other);
}

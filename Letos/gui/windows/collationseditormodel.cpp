#include "collationseditormodel.h"
#include "common/strhash.h"
#include "iconmanager.h"
#include "services/pluginmanager.h"
#include "plugins/scriptingplugin.h"
#include "icon.h"

CollationsEditorModel::CollationsEditorModel(QObject *parent) :
    QAbstractListModel(parent)
{
    init();
}

void CollationsEditorModel::clearModified()
{
    beginResetModel();
    for (Collation* coll : collationList)
        coll->modified = false;

    listModified = false;
    originalCollationList = collationList;

    endResetModel();
}

bool CollationsEditorModel::isModified() const
{
    if (collationList != originalCollationList)
        return true;

    for (Collation* coll : collationList)
    {
        if (coll->modified)
            return true;
    }
    return false;
}

bool CollationsEditorModel::isValid() const
{
    for (Collation* coll : collationList)
    {
        if (!coll->valid)
            return false;
    }
    return true;
}

void CollationsEditorModel::setData(const QList<CollationManager::CollationPtr>& collations)
{
    beginResetModel();

    for (Collation* collationPtr : collationList)
        delete collationPtr;

    collationList.clear();

    for (const CollationManager::CollationPtr& coll : collations)
        collationList << new Collation(coll);

    listModified = false;
    originalCollationList = collationList;

    endResetModel();
}

void CollationsEditorModel::addCollation(const CollationManager::CollationPtr& collation)
{
    int row = collationList.size();

    beginInsertRows(QModelIndex(), row, row);

    collationList << new Collation(collation);
    listModified = true;

    endInsertRows();
}

void CollationsEditorModel::deleteCollation(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());

    delete collationList[idx.row()];
    collationList.removeAt(idx.row());

    listModified = true;

    endRemoveRows();
}

QList<CollationManager::CollationPtr> CollationsEditorModel::getCollations() const
{
    QList<CollationManager::CollationPtr> results;
    for (Collation* coll : collationList)
        results << coll->data;

    return results;
}

QStringList CollationsEditorModel::getCollationNames() const
{
    QStringList names;
    for (Collation* coll : collationList)
        names << coll->data->name;

    return names;
}

void CollationsEditorModel::validateNames()
{
    StrHash<QList<int>> counter;

    int row = 0;
    for (Collation* coll : collationList)
    {
        coll->valid &= true;
        counter[coll->data->name] << row++;
    }

    QHashIterator<QString,QList<int>> cntIt = counter.iterator();
    while (cntIt.hasNext())
    {
        cntIt.next();
        if (cntIt.value().size() > 1)
        {
            for (int cntRow : cntIt.value())
                setData(index(cntRow), false, VALID);
        }
    }

    QModelIndex idx;
    for (int i = 0; i < collationList.size(); i++)
    {
        idx = index(i);
        emit dataChanged(idx, idx);
    }
}

bool CollationsEditorModel::isAllowedName(const QModelIndex& idx, const QString& nameToValidate)
{
    QStringList names = getCollationNames();
    names.removeAt(idx.row());
    return !names.contains(nameToValidate, Qt::CaseInsensitive);
}

int CollationsEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return collationList.size();
}

int CollationsEditorModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant CollationsEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (index.column() < 0 || index.column() > 1)
        return {};

    if (index.row() < 0 || index.row() >= collationList.size())
        return {};

    auto* coll = collationList[index.row()];
    switch (index.column())
    {
        case 0:
        {
            switch (role)
            {
                case Qt::DisplayRole:
                    return coll->data->name;
                case Qt::DecorationRole:
                {
                    bool isExtension = coll->data->type == CollationManager::CollationType::EXTENSION_BASED;
                    if (isExtension || langToIcon.contains(coll->data->lang))
                        return isExtension ? ICONS.CONSTRAINT_COLLATION: langToIcon[coll->data->lang];

                    break;
                }
                case CODE:
                    return coll->data->code;
                case MODIFIED:
                    return coll->modified;
                case VALID:
                    return coll->valid;
                case TYPE:
                    return coll->data->type;
                case NAME:
                    return coll->data->name;
                case LANG:
                    return coll->data->lang;
                case DATABASES:
                    return coll->data->databases;
                case ALL_DATABASES:
                    return coll->data->allDatabases;
            }
            break;
        }
        case 1:
        {
            if (role == Qt::DisplayRole)
                return coll->data->allDatabases ? "*" : QString::number(coll->data->databases.size());

            break;
        }
    }

    if (role == Qt::ToolTipRole)
    {
        QString dbPart = coll->data->allDatabases ? tr("all databases") : coll->data->databases.join(", ");
        QString typeStr = CollationManager::typeDisplayString(coll->data->type);

        static_qstring(rowTpl, "<tr><td align='right'>%1</td><td><b>%2</b></td></tr>");
        return "<table>" +
               rowTpl.arg(tr("Collation:"), coll->data->name) +
               rowTpl.arg(tr("Type:"), typeStr) +
               rowTpl.arg(tr("Language:"), coll->data->lang) +
               rowTpl.arg(tr("Registered in:"), dbPart) +
               "</table>";
    }

    return QVariant();

}

bool CollationsEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (index.column() != 0)
        return false;

    if (index.row() < 0 || index.row() >= collationList.size())
        return false;

    auto* coll = collationList[index.row()];
    switch (role)
    {
        case CODE:
            coll->data->code = value.toString();
            break;
        case MODIFIED:
            coll->modified = value.toBool();
            break;
        case VALID:
            coll->valid = value.toBool();
            break;
        case TYPE:
            coll->data->type = static_cast<CollationManager::CollationType>(value.toInt());
            break;
        case NAME:
            coll->data->name = value.toString();
            break;
        case LANG:
            coll->data->lang = value.toString();
            break;
        case DATABASES:
            coll->data->databases = value.toStringList();
            break;
        case ALL_DATABASES:
            coll->data->allDatabases = value.toBool();
            break;
        defaut:
            return true;
    }

    emit dataChanged(index, index);
    return true;
}

void CollationsEditorModel::init()
{
    for (ScriptingPlugin* plugin : PLUGINS->getLoadedPlugins<ScriptingPlugin>())
        langToIcon[plugin->getLanguage()] = QIcon(plugin->getIconPath());
}

CollationsEditorModel::Collation::Collation()
{
    data = CollationManager::CollationPtr::create();
}

CollationsEditorModel::Collation::Collation(const CollationManager::CollationPtr& other)
{
    data = CollationManager::CollationPtr::create(*other);
    originalName = data->name;
}

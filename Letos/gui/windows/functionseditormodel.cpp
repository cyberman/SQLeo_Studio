#include "functionseditormodel.h"
#include "services/pluginmanager.h"
#include "plugins/scriptingplugin.h"
#include "iconmanager.h"
#include <QDebug>

FunctionsEditorModel::FunctionsEditorModel(QObject *parent) :
    QAbstractListModel(parent)
{
    init();
}

void FunctionsEditorModel::clearModified()
{
    beginResetModel();
    for (Function*& func : functionList)
        func->modified = false;

    listModified = false;
    originalFunctionList = functionList;

    endResetModel();
}

bool FunctionsEditorModel::isModified() const
{
    if (functionList != originalFunctionList)
        return true;

    for (Function* func : functionList)
    {
        if (func->modified)
            return true;
    }
    return false;
}

bool FunctionsEditorModel::isValid() const
{
    for (Function* func : functionList)
    {
        if (!func->valid)
            return false;
    }
    return true;
}

bool FunctionsEditorModel::isAggregate(const QModelIndex& idx) const
{
    return data(idx, TYPE).toInt() == FunctionManager::ScriptFunction::AGGREGATE;
}

bool FunctionsEditorModel::isAggregateWindow(const QModelIndex& idx) const
{
    return data(idx, TYPE).toInt() == FunctionManager::ScriptFunction::AGG_WINDOW;
}

bool FunctionsEditorModel::isAnyAggregate(const QModelIndex& idx) const
{
    int type = data(idx, TYPE).toInt();
    return type == FunctionManager::ScriptFunction::AGGREGATE || type == FunctionManager::ScriptFunction::AGG_WINDOW;
}

bool FunctionsEditorModel::isScalar(const QModelIndex& idx) const
{
    return data(idx, TYPE).toInt() == FunctionManager::ScriptFunction::SCALAR;
}

void FunctionsEditorModel::setData(const QList<FunctionManager::ScriptFunction*>& functions)
{
    beginResetModel();

    for (Function*& functionPtr : functionList)
        delete functionPtr;

    functionList.clear();

    for (FunctionManager::ScriptFunction* func : functions)
        functionList << new Function(func);

    listModified = false;
    originalFunctionList = functionList;

    endResetModel();
}

void FunctionsEditorModel::addFunction(FunctionManager::ScriptFunction* function)
{
    int row = functionList.size();

    beginInsertRows(QModelIndex(), row, row);

    functionList << new Function(function);
    listModified = true;

    endInsertRows();
}

void FunctionsEditorModel::deleteFunction(const QModelIndex& idx)
{
    if (!idx.isValid())
        return;

    beginRemoveRows(QModelIndex(), idx.row(), idx.row());

    delete functionList[idx.row()];
    functionList.removeAt(idx.row());

    listModified = true;

    endRemoveRows();
}

QList<FunctionManager::ScriptFunction*> FunctionsEditorModel::generateFunctions() const
{
    QList<FunctionManager::ScriptFunction*> results;

    for (Function* func : functionList)
        results << new FunctionManager::ScriptFunction(func->data);

    return results;
}

QStringList FunctionsEditorModel::getFunctionNames() const
{
    QStringList names;
    for (Function* func : functionList)
        names << func->data.name;

    return names;
}

void FunctionsEditorModel::validateNames()
{
    QHash<UniqueFunctionName, QList<int>> counter;

    int row = 0;
    for (Function*& func : functionList)
    {
        func->valid &= true;
        UniqueFunctionName uniqueName = func->toUniqueName();
        counter[uniqueName] << row++;
    }

    QHashIterator<UniqueFunctionName, QList<int>> cntIt(counter);
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
    for (int i = 0; i < functionList.size(); i++)
    {
        idx = index(i);
        emit dataChanged(idx, idx);
    }
}

bool FunctionsEditorModel::isAllowedName(const QModelIndex& idx, const QString& nameToValidate, const QStringList& argList, bool undefinedArgs)
{
    QList<UniqueFunctionName> names = getUniqueFunctionNames();
    names.removeAt(idx.row());

    UniqueFunctionName validatedName;
    validatedName.name = nameToValidate.toLower();
    validatedName.undefArg = undefinedArgs;
    if (!undefinedArgs)
        validatedName.arguments = argList;

    return !names.contains(validatedName);
}

int FunctionsEditorModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return functionList.size();
}

int FunctionsEditorModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant FunctionsEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (index.column() < 0 || index.column() > 1)
        return {};

    if (index.row() < 0 || index.row() >= functionList.size())
        return {};

    Function* fn = functionList[index.row()];
    switch (index.column())
    {
        case 0:
        {
            switch (role)
            {
                case Qt::DisplayRole:
                    return fn->data.toString();
                case Qt::DecorationRole:
                {
                    if (fn->valid)
                        return langToIcon.contains(fn->data.lang) ? langToIcon[fn->data.lang] : ICONS.LIST_ITEM_OTHER;
                    else
                        return ICONS.FUNCTION_ERROR;
                }
                case CODE:
                    return fn->data.code;
                case MODIFIED:
                    return fn->modified;
                case VALID:
                    return fn->valid;
                case TYPE:
                    return fn->data.type;
                case FINAL_CODE:
                    return fn->data.finalCode;
                case STEP_CODE:
                    return fn->data.code;
                case INVERSE_CODE:
                    return fn->data.inverseCode;
                case INIT_CODE:
                    return fn->data.initCode;
                case NAME:
                    return fn->data.name;
                case LANG:
                    return fn->data.lang;
                case DATABASES:
                    return fn->data.databases;
                case ALL_DATABASES:
                    return fn->data.allDatabases;
                case ARGUMENTS:
                    return fn->data.arguments;
                case UNDEF_ARGS:
                    return fn->data.undefinedArgs;
                case DETERMINISTIC:
                    return fn->data.deterministic;
            }
            break;
        }
        case 1:
        {
            if (role == Qt::DisplayRole)
                return fn->data.allDatabases ? "*" : QString::number(fn->data.databases.size());

            break;
        }
    }

    if (role == Qt::ToolTipRole)
    {
        QString dbPart = fn->data.allDatabases ? tr("all databases") : fn->data.databases.join(", ");
        QString typeStr = FunctionManager::FunctionBase::displayString(fn->data.type);

        static_qstring(rowTpl, "<tr><td align='right'>%1</td><td><b>%2</b></td></tr>");
        return "<table>" +
               rowTpl.arg(tr("Function:"), fn->data.toString()) +
               rowTpl.arg(tr("Type:"), typeStr) +
               rowTpl.arg(tr("Deterministic:"), fn->data.deterministic ? tr("Yes") : tr("No")) +
               rowTpl.arg(tr("Language:"), fn->data.lang) +
               rowTpl.arg(tr("Registered in:"), dbPart) +
               "</table>";
    }

    return QVariant();
}

bool FunctionsEditorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (index.column() != 0)
        return false;

    if (index.row() < 0 || index.row() >= functionList.size())
        return false;

    Function* fn = functionList[index.row()];
    switch (role)
    {
        case CODE:
            fn->data.code = value.toString();
            break;
        case MODIFIED:
            fn->modified = value.toBool();
            break;
        case VALID:
            fn->valid = value.toBool();
            break;
        case TYPE:
            fn->data.type = static_cast<FunctionManager::ScriptFunction::Type>(value.toInt());
            break;
        case FINAL_CODE:
            fn->data.finalCode = value.toString();
            break;
        case STEP_CODE:
            fn->data.code = value.toString();
            break;
        case INVERSE_CODE:
            fn->data.inverseCode = value.toString();
            break;
        case INIT_CODE:
            fn->data.initCode = value.toString();
            break;
        case NAME:
            fn->data.name = value.toString();
            break;
        case LANG:
            fn->data.lang = value.toString();
            break;
        case DATABASES:
            fn->data.databases = value.toStringList();
            break;
        case ALL_DATABASES:
            fn->data.allDatabases = value.toBool();
            break;
        case ARGUMENTS:
            fn->data.arguments = value.toStringList();
            break;
        case UNDEF_ARGS:
            fn->data.undefinedArgs = value.toBool();
            break;
        case DETERMINISTIC:
            fn->data.deterministic = value.toBool();
            break;
        defaut:
            return true;
    }

    emit dataChanged(index, index);
    return true;
}

void FunctionsEditorModel::init()
{
    for (ScriptingPlugin*& plugin : PLUGINS->getLoadedPlugins<ScriptingPlugin>())
        langToIcon[plugin->getLanguage()] = QIcon(plugin->getIconPath());
}

QList<FunctionsEditorModel::UniqueFunctionName> FunctionsEditorModel::getUniqueFunctionNames() const
{
    QList<UniqueFunctionName> names;
    for (Function* func : functionList)
        names << func->toUniqueName();

    return names;
}

FunctionsEditorModel::Function::Function()
{
}

FunctionsEditorModel::Function::Function(FunctionManager::ScriptFunction* other)
{
    data = FunctionManager::ScriptFunction(*other);
    originalName = data.name;
}

FunctionsEditorModel::UniqueFunctionName FunctionsEditorModel::Function::toUniqueName() const
{
    UniqueFunctionName uniqName;
    uniqName.name = data.name.toLower();
    uniqName.undefArg = data.undefinedArgs;
    if (!data.undefinedArgs)
        uniqName.arguments = data.arguments;

    return uniqName;
}

int FunctionsEditorModel::UniqueFunctionName::argCount() const
{
    return undefArg ? -1 : arguments.size();
}

bool FunctionsEditorModel::UniqueFunctionName::operator==(const UniqueFunctionName &other) const
{
    return name == other.name && argCount() == other.argCount();
}

int qHash(FunctionsEditorModel::UniqueFunctionName fnName)
{
    return qHash(fnName.name) ^ fnName.argCount();
}

#ifndef FUNCTIONSEDITORMODEL_H
#define FUNCTIONSEDITORMODEL_H

#include "services/functionmanager.h"
#include "gui_global.h"
#include <QIcon>
#include <QAbstractListModel>

class GUI_API_EXPORT FunctionsEditorModel : public QAbstractListModel
{
        Q_OBJECT

    public:
        enum Role
        {
            CODE = 1000,
            MODIFIED = 1001,
            VALID = 1002,
            TYPE = 1003,
            FINAL_CODE = 1004,
            STEP_CODE = 1005,
            INVERSE_CODE = 1006,
            INIT_CODE = 1007,
            NAME = 1008,
            LANG = 1009,
            DATABASES = 1010,
            ALL_DATABASES = 1011,
            ARGUMENTS = 1012,
            UNDEF_ARGS = 1013,
            DETERMINISTIC = 1014,
        };

        explicit FunctionsEditorModel(QObject *parent = 0);

        void clearModified();
        bool isModified() const;
        bool isValid() const;
        bool isAggregate(const QModelIndex& idx) const;
        bool isAggregateWindow(const QModelIndex& idx) const;
        bool isAnyAggregate(const QModelIndex& idx) const;
        bool isScalar(const QModelIndex& idx) const;
        void setData(const QList<FunctionManager::ScriptFunction*>& functions);
        void addFunction(FunctionManager::ScriptFunction* function);
        void deleteFunction(const QModelIndex& idx);
        QList<FunctionManager::ScriptFunction*> generateFunctions() const;
        QStringList getFunctionNames() const;
        void validateNames();
        bool isAllowedName(const QModelIndex& idx, const QString& nameToValidate, const QStringList &argList, bool undefinedArgs);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    private:
        struct UniqueFunctionName
        {
            QString name;
            QStringList arguments;
            bool undefArg;

            int argCount() const;
            bool operator==(const UniqueFunctionName& other) const;
        };

        struct Function
        {
            Function();
            Function(FunctionManager::ScriptFunction* other);
            UniqueFunctionName toUniqueName() const;

            FunctionManager::ScriptFunction data;
            bool modified = false;
            bool valid = true;
            QString originalName;
        };

        void init();
        QList<UniqueFunctionName> getUniqueFunctionNames() const;

        friend int qHash(FunctionsEditorModel::UniqueFunctionName fnName);

        QList<Function*> functionList;

        /**
         * @brief List of function pointers before modifications.
         *
         * This list is kept to check for modifications in the overall list of functions.
         * Pointers on this list may be already deleted, so don't use them!
         * It's only used to compare list of pointers to functionList, so it can tell you
         * if the list was modified in regards of adding or deleting functions.
         */
        QList<Function*> originalFunctionList;
        QHash<QString,QIcon> langToIcon;
        bool listModified = false;
};

int qHash(FunctionsEditorModel::UniqueFunctionName fnName);

#endif // FUNCTIONSEDITORMODEL_H

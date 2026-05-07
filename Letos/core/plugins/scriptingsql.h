#ifndef SCRIPTINGSQL_H
#define SCRIPTINGSQL_H

#include "builtinplugin.h"
#include "scriptingplugin.h"

class ScriptingSql : public BuiltInPlugin, public DbAwareScriptingPlugin
{
    Q_OBJECT

    LETOS_PLUGIN_TITLE("SQL scripting")
    LETOS_PLUGIN_DESC("SQL scripting support.")
    LETOS_PLUGIN_VERSION(10000)
    LETOS_PLUGIN_AUTHOR("letos.org")

    public:
        class SqlContext : public Context
        {
            public:
                QString errorText;
                QHash<QString,QVariant> variables;
        };

        ScriptingSql();
        ~ScriptingSql();

        QString getLanguage() const;
        Context* createContext();
        void releaseContext(Context* context);
        void resetContext(Context* context);
        QVariant evaluate(Context* context, const QString& code, const FunctionInfo& funcInfo,
                          const QList<QVariant>& args, Db* db, bool locking);
        QVariant evaluate(const QString& code, const FunctionInfo& funcInfo,
                          const QList<QVariant>& args, Db* db, bool locking, QString* errorMessage);
        void setVariable(Context* context, const QString& name, const QVariant& value);
        QVariant getVariable(Context* context, const QString& name);
        bool hasError(Context* context) const;
        QString getErrorMessage(Context* context) const;
        QString getIconPath() const;
        bool init();
        void deinit();

    private:
        void replaceNamedArgs(QString& sql, const FunctionInfo& funcInfo, const QList<QVariant>& args);

        QList<Context*> contexts;
        Db* memDb = nullptr;
};

#endif // SCRIPTINGSQL_H

#ifndef POPULATECONSTANT_H
#define POPULATECONSTANT_H

#include "builtinplugin.h"
#include "populateplugin.h"
#include "config_builder.h"

CFG_CATEGORIES(PopulateConstantConfig,
    CFG_CATEGORY(PopulateConstant,
        CFG_ENTRY(QString, Value, QString())
    )
)

class PopulateConstant : public BuiltInPlugin, public PopulatePlugin
{
        Q_OBJECT

        LETOS_PLUGIN_TITLE("Constant")
        LETOS_PLUGIN_DESC("Support for populating tables with a constant value.")
        LETOS_PLUGIN_VERSION(10001)
        LETOS_PLUGIN_AUTHOR("letos.org")

    public:
        PopulateConstant();

        QString getTitle() const;
        PopulateEngine* createEngine();
};

class PopulateConstantEngine : public PopulateEngine
{
    public:
        bool beforePopulating(Db* db, const QString& table);
        QVariant nextValue(bool& nextValueError);
        void afterPopulating();
        CfgMain* getConfig();
        QString getPopulateConfigFormName() const;
        bool validateOptions();

    private:
        CFG_LOCAL(PopulateConstantConfig, cfg)
};

#endif // POPULATECONSTANT_H

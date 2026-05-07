#ifndef POPULATERANDOM_H
#define POPULATERANDOM_H

#include "builtinplugin.h"
#include "populateplugin.h"
#include "config_builder.h"
#include <QRandomGenerator>

CFG_CATEGORIES(PopulateRandomConfig,
    CFG_CATEGORY(PopulateRandom,
        CFG_ENTRY(int,     MinValue, 0)
        CFG_ENTRY(int,     MaxValue, 99999999)
        CFG_ENTRY(QString, Prefix,   QString())
        CFG_ENTRY(QString, Suffix,   QString())
    )
)

class PopulateRandom : public BuiltInPlugin, public PopulatePlugin
{
        Q_OBJECT

        LETOS_PLUGIN_TITLE("Random")
        LETOS_PLUGIN_DESC("Support for populating tables with random numbers.")
        LETOS_PLUGIN_VERSION(10001)
        LETOS_PLUGIN_AUTHOR("letos.org")

    public:
        PopulateRandom();

        QString getTitle() const;
        PopulateEngine* createEngine();
};

class PopulateRandomEngine : public PopulateEngine
{
    public:
        bool beforePopulating(Db* db, const QString& table);
        QVariant nextValue(bool& nextValueError);
        void afterPopulating();
        CfgMain* getConfig();
        QString getPopulateConfigFormName() const;
        bool validateOptions();

    private:
        CFG_LOCAL(PopulateRandomConfig, cfg)
        int range;
        QRandomGenerator randomGenerator;
};
#endif // POPULATERANDOM_H

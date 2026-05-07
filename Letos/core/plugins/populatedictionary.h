#ifndef POPULATEDICTIONARY_H
#define POPULATEDICTIONARY_H

#include "builtinplugin.h"
#include "populateplugin.h"
#include "config_builder.h"

class QFile;
class QTextStream;

CFG_CATEGORIES(PopulateDictionaryConfig,
    CFG_CATEGORY(PopulateDictionary,
        CFG_ENTRY(QString, File,   QString())
        CFG_ENTRY(bool,    Lines,  false)
        CFG_ENTRY(bool,    Random, false)
    )
)

class PopulateDictionary : public BuiltInPlugin, public PopulatePlugin
{
        Q_OBJECT

        LETOS_PLUGIN_TITLE("Dictionary")
        LETOS_PLUGIN_DESC("Support for populating tables with values from a dictionary file.")
        LETOS_PLUGIN_VERSION(10001)
        LETOS_PLUGIN_AUTHOR("letos.org")

    public:
        PopulateDictionary();

        QString getTitle() const;
        PopulateEngine* createEngine();
};

class PopulateDictionaryEngine : public PopulateEngine
{
    public:
        bool beforePopulating(Db* db, const QString& table);
        QVariant nextValue(bool& nextValueError);
        void afterPopulating();
        CfgMain* getConfig();
        QString getPopulateConfigFormName() const;
        bool validateOptions();

    private:
        CFG_LOCAL(PopulateDictionaryConfig, cfg)
        QStringList dictionary;
        int dictionarySize = 0;
        int dictionaryPos = 0;
};

#endif // POPULATEDICTIONARY_H

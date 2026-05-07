#ifndef MULTIEDITORDATE_H
#define MULTIEDITORDATE_H

#include "multieditordatetime.h"

class GUI_API_EXPORT MultiEditorDate : public MultiEditorDateTime
{
        Q_OBJECT

    public:
        explicit MultiEditorDate(QWidget *parent = 0);

        static void staticInit();

    protected:
        QStringList getParsingFormats();

    private:
        static QStringList formats;
};

class GUI_API_EXPORT MultiEditorDatePlugin : public BuiltInPlugin, public MultiEditorWidgetPlugin
{
    Q_OBJECT

    LETOS_PLUGIN_AUTHOR("letos.org")
    LETOS_PLUGIN_DESC("Date data editor.")
    LETOS_PLUGIN_TITLE("Date")
    LETOS_PLUGIN_VERSION(10000)

    public:
        MultiEditorWidget* getInstance();
        bool validFor(const DataType& dataType);
        int getPriority(const QVariant& value, const DataType& dataType);
        QString getTabLabel();
};

#endif // MULTIEDITORDATE_H

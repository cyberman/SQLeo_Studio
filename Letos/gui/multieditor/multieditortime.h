#ifndef MULTIEDITORTIME_H
#define MULTIEDITORTIME_H

#include "multieditordatetime.h"
#include "gui_global.h"

class GUI_API_EXPORT MultiEditorTime : public MultiEditorDateTime
{
        Q_OBJECT

    public:
        explicit MultiEditorTime(QWidget *parent = 0);

        static void staticInit();

    protected:
        QStringList getParsingFormats();

    private:
        static QStringList formats;
};

class GUI_API_EXPORT MultiEditorTimePlugin : public BuiltInPlugin, public MultiEditorWidgetPlugin
{
    Q_OBJECT

    LETOS_PLUGIN_AUTHOR("letos.org")
    LETOS_PLUGIN_DESC("Time data editor.")
    LETOS_PLUGIN_TITLE("Time")
    LETOS_PLUGIN_VERSION(10000)

    public:
        MultiEditorWidget* getInstance();
        bool validFor(const DataType& dataType);
        int getPriority(const QVariant& value, const DataType& dataType);
        QString getTabLabel();
};

#endif // MULTIEDITORTIME_H

#ifndef MULTIEDITORHEX_H
#define MULTIEDITORHEX_H

#include "multieditorwidget.h"
#include "multieditorwidgetplugin.h"
#include "plugins/builtinplugin.h"
#include <QVariant>
#include <QSharedPointer>

class QHexEdit;
class QBuffer;

class GUI_API_EXPORT MultiEditorHex : public MultiEditorWidget
{
        Q_OBJECT
    public:
        explicit MultiEditorHex();
        ~MultiEditorHex();

        void setValue(const QVariant& value);
        QVariant getValue();
        void setReadOnly(bool value);
        void focusThisWidget();

        QList<QWidget*> getNoScrollWidgets();

    private:
        QHexEdit* hexEdit = nullptr;

    private slots:
        void modificationChanged();
};

class GUI_API_EXPORT MultiEditorHexPlugin : public BuiltInPlugin, public MultiEditorWidgetPlugin
{
    Q_OBJECT

    LETOS_PLUGIN_AUTHOR("letos.org")
    LETOS_PLUGIN_DESC("Hexadecimal data editor.")
    LETOS_PLUGIN_TITLE("Hexadecimal")
    LETOS_PLUGIN_VERSION(10000)

    public:
        MultiEditorWidget* getInstance();
        bool validFor(const DataType& dataType);
        int getPriority(const QVariant& value, const DataType& dataType);
        QString getTabLabel();
};

#endif // MULTIEDITORHEX_H

#ifndef MULTIEDITORNUMERIC_H
#define MULTIEDITORNUMERIC_H

#include "multieditorwidget.h"
#include "multieditorwidgetplugin.h"
#include "plugins/builtinplugin.h"

class NumericSpinBox;

class GUI_API_EXPORT MultiEditorNumeric : public MultiEditorWidget
{
        Q_OBJECT

    public:
        explicit MultiEditorNumeric(QWidget *parent = 0);

        void setValue(const QVariant& value);
        QVariant getValue();
        void setReadOnly(bool value);
        void focusThisWidget();

        QList<QWidget*> getNoScrollWidgets();

    private:
        NumericSpinBox* spinBox = nullptr;
};

class GUI_API_EXPORT MultiEditorNumericPlugin : public BuiltInPlugin, public MultiEditorWidgetPlugin
{
    Q_OBJECT

    LETOS_PLUGIN_AUTHOR("letos.org")
    LETOS_PLUGIN_DESC("Numeric data editor.")
    LETOS_PLUGIN_TITLE("Numeric types")
    LETOS_PLUGIN_VERSION(10000)

    public:
        MultiEditorWidget* getInstance();
        bool validFor(const DataType& dataType);
        int getPriority(const QVariant& value, const DataType& dataType);
        QString getTabLabel();
};

#endif // MULTIEDITORNUMERIC_H

#ifndef SQLITEEXTENSIONEDITORMODEL_H
#define SQLITEEXTENSIONEDITORMODEL_H

#include "gui_global.h"
#include "services/sqliteextensionmanager.h"
#include <QIcon>
#include <QHash>
#include <QAbstractListModel>

class GUI_API_EXPORT SqliteExtensionEditorModel : public QAbstractListModel
{
    Q_OBJECT

    public:
        explicit SqliteExtensionEditorModel(QObject* parent = nullptr);

        enum Role
        {
            NAME = 1000,
            MODIFIED = 1001,
            VALID = 1002,
            FILE_PATH = 1003,
            INIT_FUNC = 1004,
            DATABASES = 1005,
            ALL_DATABASES = 1006,
        };

        void clearModified();
        bool isModified() const;
        bool isValid() const;
        void setData(const QList<SqliteExtensionManager::ExtensionPtr>& extensions);
        void addExtension(const SqliteExtensionManager::ExtensionPtr& extension);
        void deleteExtension(const QModelIndex& idx);
        QList<SqliteExtensionManager::ExtensionPtr> getExtensions() const;

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    private:
        struct Extension
        {
            Extension();
            Extension(const SqliteExtensionManager::ExtensionPtr& other);

            SqliteExtensionManager::ExtensionPtr data;
            QString name;
            bool modified = false;
            bool valid = true;
        };

        QList<Extension*> extensionList;
        QList<Extension*> originalExtensionList;
        QHash<QString, QIcon> langToIcon;
        bool listModified = false;
};

#endif // SQLITEEXTENSIONEDITORMODEL_H

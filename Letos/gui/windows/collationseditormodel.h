#ifndef COLLATIONSEDITORMODEL_H
#define COLLATIONSEDITORMODEL_H

#include "services/collationmanager.h"
#include "gui_global.h"
#include <QIcon>
#include <QHash>
#include <QAbstractListModel>

class GUI_API_EXPORT CollationsEditorModel : public QAbstractListModel
{
        Q_OBJECT
    public:
        enum Role
        {
            CODE = 1000,
            MODIFIED = 1001,
            VALID = 1002,
            TYPE = 1003,
            NAME = 1004,
            LANG = 1005,
            DATABASES = 1006,
            ALL_DATABASES = 1007,
        };

        explicit CollationsEditorModel(QObject *parent = nullptr);

        void clearModified();
        bool isModified() const;
        bool isValid() const;
        void setData(const QList<CollationManager::CollationPtr>& collations);
        void addCollation(const CollationManager::CollationPtr& collation);
        void deleteCollation(const QModelIndex& idx);
        QList<CollationManager::CollationPtr> getCollations() const;
        QStringList getCollationNames() const;
        void validateNames();
        bool isAllowedName(const QModelIndex& idx, const QString& nameToValidate);

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    private:
        struct Collation
        {
            Collation();
            Collation(const CollationManager::CollationPtr& other);

            CollationManager::CollationPtr data;
            bool modified = false;
            bool valid = true;
            QString originalName;
        };

        void init();
        void emitDataChanged(int row);

        QList<Collation*> collationList;

        /**
         * @brief List of collation pointers before modifications.
         *
         * This list is kept to check for modifications in the overall list of collations.
         * Pointers on this list may be already deleted, so don't use them!
         * It's only used to compare list of pointers to collationList, so it can tell you
         * if the list was modified in regards of adding or deleting collations.
         */
        QList<Collation*> originalCollationList;
        QHash<QString,QIcon> langToIcon;
        bool listModified = false;
};

#endif // COLLATIONSEDITORMODEL_H

#include "queryexecutorattaches.h"
#include "dbattacher.h"
#include "letos.h"
#include <QScopedPointer>

bool QueryExecutorAttaches::exec()
{
    QScopedPointer<DbAttacher> attacher(LETOS->createDbAttacher(db));
    if (!attacher->attachDatabases(context->parsedQueries))
        return false;

    context->dbNameToAttach = attacher->getDbNameToAttach();
    context->nativeDbPathToAttachName = attacher->getNativePathToAttachName();
    updateQueries();

    return true;
}

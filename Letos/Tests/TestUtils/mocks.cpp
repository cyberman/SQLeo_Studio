#include "mocks.h"
#include "common/global.h"
#include "letos.h"
#include "configmock.h"
#include "pluginmanagermock.h"
#include "functionmanagermock.h"
#include "collationmanagermock.h"
#include "dbattachermock.h"
#include "dbmanagermock.h"
#include "extensionmanagermock.h"

MockRepository* mockRepository = nullptr;

MockRepository& mockRepo()
{
    if (!mockRepository)
    {
        mockRepository = new MockRepository;
        mockRepository->autoExpect = false;
    }

    return *mockRepository;
}

void deleteMockRepo()
{
    safe_delete(mockRepository);
}

void initMocks()
{
    LETOS->setConfig(new ConfigMock());
    LETOS->setFunctionManager(new FunctionManagerMock());
    LETOS->setPluginManager(new PluginManagerMock());
    LETOS->setDbAttacherFactory(new DbAttacherFactoryMock());
    LETOS->setDbManager(new DbManagerMock());
    LETOS->setCollationManager(new CollationManagerMock());
    LETOS->setSqliteExtensionManager(new ExtensionManagerMock());
}

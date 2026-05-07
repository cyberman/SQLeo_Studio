add_executable(tst_sqlqueryexecutortest
    tst_sqlqueryexecutortest.cpp
)

qt_add_resources(_rcc testfiles.qrc)
target_sources(tst_sqlqueryexecutortest PRIVATE ${_rcc})

letos_set_test_properties(tst_sqlqueryexecutortest)


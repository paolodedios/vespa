# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(messagebus_servicepool_test_app TEST
    SOURCES
    servicepool.cpp
    DEPENDS
    vespa_messagebus-test
    vespa_messagebus
    GTest::gtest
)
vespa_add_test(NAME messagebus_servicepool_test_app COMMAND messagebus_servicepool_test_app)

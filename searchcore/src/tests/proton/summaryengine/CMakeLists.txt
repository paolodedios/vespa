# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(searchcore_summaryengine_test_app TEST
    SOURCES
    summaryengine_test.cpp
    DEPENDS
    searchcore_summaryengine
    searchcore_pcommon
    GTest::gtest
)
vespa_add_test(NAME searchcore_summaryengine_test_app COMMAND searchcore_summaryengine_test_app)

# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(searchcore_resource_usage_write_filter_test_app TEST
    SOURCES
    resource_usage_write_filter_test.cpp
    DEPENDS
    searchcore_server
    GTest::gtest
)
vespa_add_test(NAME searchcore_resource_usage_write_filter_test_app COMMAND searchcore_resource_usage_write_filter_test_app)

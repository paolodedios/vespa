# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(vespalib_smart_buffer_test_app TEST
    SOURCES
    smart_buffer_test.cpp
    DEPENDS
    vespalib
    GTest::gtest
)
vespa_add_test(NAME vespalib_smart_buffer_test_app COMMAND vespalib_smart_buffer_test_app)

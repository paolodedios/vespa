# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(searchlib_verify_feature_test_app TEST
    SOURCES
    verify_feature_test.cpp
    DEPENDS
    vespa_searchlib
    GTest::gtest
)
vespa_add_test(NAME searchlib_verify_feature_test_app COMMAND searchlib_verify_feature_test_app)

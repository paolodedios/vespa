# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(vsm_textutil_test_app TEST
    SOURCES
    textutil_test.cpp
    DEPENDS
    vespa_streamingvisitors
    GTest::gtest
)
vespa_add_test(NAME vsm_textutil_test_app COMMAND vsm_textutil_test_app)

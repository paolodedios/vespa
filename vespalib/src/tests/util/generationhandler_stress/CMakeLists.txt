# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(vespalib_generation_handler_stress_test_app
    SOURCES
    generation_handler_stress_test.cpp
    DEPENDS
    vespalib
    GTest::gtest
)
vespa_add_test(NAME vespalib_generation_handler_stress_test_app NO_VALGRIND COMMAND vespalib_generation_handler_stress_test_app --smoke-test)

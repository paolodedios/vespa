# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(eval_remove_trivial_dimension_optimizer_test_app TEST
    SOURCES
    remove_trivial_dimension_optimizer_test.cpp
    DEPENDS
    vespaeval
    GTest::gtest
)
vespa_add_test(NAME eval_remove_trivial_dimension_optimizer_test_app COMMAND eval_remove_trivial_dimension_optimizer_test_app)

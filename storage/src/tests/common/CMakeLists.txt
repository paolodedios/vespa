# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_library(storage_testcommon TEST
    SOURCES
    dummystoragelink.cpp
    message_sender_stub.cpp
    storage_config_set.cpp
    testnodestateupdater.cpp
    teststorageapp.cpp
    DEPENDS
    vespa_storage
)

vespa_add_executable(storage_common_gtest_runner_app TEST
    SOURCES
    bucket_stripe_utils_test.cpp
    bucket_utils_test.cpp
    gtest_runner.cpp
    metricstest.cpp
    storagelinktest.cpp
    DEPENDS
    storage_testcommon
    vespa_storage
    GTest::gtest
)

vespa_add_test(
    NAME storage_common_gtest_runner_app
    COMMAND storage_common_gtest_runner_app
)

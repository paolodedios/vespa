# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(searchlib_hnsw_index_test_app TEST
    SOURCES
    hnsw_index_test.cpp
    DEPENDS
    searchlib_test
    vespa_searchlib
    GTest::gtest
)
vespa_add_test(NAME searchlib_hnsw_index_test_app COMMAND searchlib_hnsw_index_test_app)

vespa_add_executable(mt_stress_hnsw_app TEST
    SOURCES
    stress_hnsw_mt.cpp
    DEPENDS
    vespa_searchlib
    GTest::gtest
)

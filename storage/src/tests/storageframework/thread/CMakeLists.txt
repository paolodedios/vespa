# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_library(storageframework_testthread
    SOURCES
    tickingthreadtest.cpp
    taskthreadtest.cpp
    DEPENDS
    vespa_storage
    GTest::gtest
)

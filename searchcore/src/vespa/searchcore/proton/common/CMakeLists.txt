# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_library(searchcore_pcommon STATIC
    SOURCES
    alloc_config.cpp
    alloc_strategy.cpp
    attribute_updater.cpp
    attributefieldvaluenode.cpp
    cachedselect.cpp
    commit_time_tracker.cpp
    dbdocumentid.cpp
    doctypename.cpp
    document_type_inspector.cpp
    eventlogger.cpp
    feeddebugger.cpp
    feedtoken.cpp
    hw_info_sampler.cpp
    indexschema_inspector.cpp
    ipendinglidtracker.cpp
    memory_usage_logger.cpp
    operation_rate_tracker.cpp
    pendinglidtracker.cpp
    replay_feed_token_factory.cpp
    replay_feedtoken_state.cpp
    scheduled_forward_executor.cpp
    scheduledexecutor.cpp
    select_utils.cpp
    selectcontext.cpp
    selectpruner.cpp
    state_reporter_utils.cpp
    statusreport.cpp
    DEPENDS
    searchcore_proton_metrics
)

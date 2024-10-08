// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "handle.h"
#include <string>

namespace vespalib::metrics {

struct MetricIdTag {};

/**
 * Opaque handle representing an uniquely named metric.
 **/
struct MetricId : Handle<MetricIdTag>
{
    explicit MetricId(size_t id) : Handle(id) {}
    static MetricId from_name(const std::string& name);
    const std::string& as_name() const;
};

} // namespace vespalib::metrics

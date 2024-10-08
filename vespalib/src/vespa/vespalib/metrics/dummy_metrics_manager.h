// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "name_collection.h"
#include "current_samples.h"
#include "snapshots.h"
#include "metrics_manager.h"
#include "clock.h"
#include <memory>
#include <string>
#include <thread>

namespace vespalib::metrics {

/**
 * Dummy manager that discards everything, use
 * for unit tests where you don't care about
 * metrics.
 **/
class DummyMetricsManager : public MetricsManager
{
protected:
    DummyMetricsManager() noexcept {}
public:
    ~DummyMetricsManager() override;

    static std::shared_ptr<MetricsManager> create() {
        return std::shared_ptr<MetricsManager>(new DummyMetricsManager());
    }

    Counter counter(const std::string &, const std::string &) override {
        return Counter(shared_from_this(), MetricId(0));
    }
    Gauge gauge(const std::string &, const std::string &) override {
        return Gauge(shared_from_this(), MetricId(0));
    }

    Dimension dimension(const std::string &) override {
        return Dimension(0);
    }
    Label label(const std::string &) override {
        return Label(0);
    }
    PointBuilder pointBuilder(Point) override {
        return PointBuilder(shared_from_this());
    }
    Point pointFrom(PointMap) override {
        return Point(0);
    }

    Snapshot snapshot() override;
    Snapshot totalSnapshot() override;

    // for use from Counter only
    void add(Counter::Increment) override {}
    // for use from Gauge only
    void sample(Gauge::Measurement) override {}
};

}

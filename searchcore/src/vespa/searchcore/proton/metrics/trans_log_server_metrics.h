// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/metrics/metricset.h>
#include <vespa/metrics/valuemetric.h>
#include <vespa/searchlib/transactionlog/domainconfig.h>

namespace proton {

/**
 * Metric set for all metrics reported by transaction log server.
 */
class TransLogServerMetrics
{
public:
    struct DomainMetrics : public metrics::MetricSet
    {
        metrics::LongValueMetric entries;
        metrics::LongValueMetric diskUsage;
        metrics::DoubleValueMetric replayTime;

        using UP = std::unique_ptr<DomainMetrics>;
        DomainMetrics(metrics::MetricSet *parent, const std::string &documentType);
        ~DomainMetrics() override;
        void update(const search::transactionlog::DomainInfo &stats);
    };

private:
    metrics::MetricSet *_parent;
    std::map<std::string, DomainMetrics::UP> _domainMetrics;

    void considerAddDomains(const search::transactionlog::DomainStats &stats);
    void considerRemoveDomains(const search::transactionlog::DomainStats &stats);
    void updateDomainMetrics(const search::transactionlog::DomainStats &stats);

public:
    TransLogServerMetrics(metrics::MetricSet *parent);
    ~TransLogServerMetrics();
    void update(const search::transactionlog::DomainStats &stats);
};

} // namespace proton

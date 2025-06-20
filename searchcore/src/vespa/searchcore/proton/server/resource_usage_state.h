// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "resource_usage_with_limit.h"
#include <algorithm>

namespace proton {

/**
 * Class used to describe state of resource usage relative to configured limits.
 * In addition relative transient disk and memory usage are tracked.
 */
class ResourceUsageState
{
    ResourceUsageWithLimit _diskState;
    ResourceUsageWithLimit _memoryState;
    double _transient_disk_usage;
    double _transient_memory_usage;

public:
    ResourceUsageState() = default;
    ResourceUsageState(const ResourceUsageWithLimit &diskState_,
                       const ResourceUsageWithLimit &memoryState_,
                       double transient_disk_usage_ = 0,
                       double transient_memory_usage_ = 0)
        : _diskState(diskState_),
          _memoryState(memoryState_),
          _transient_disk_usage(transient_disk_usage_),
          _transient_memory_usage(transient_memory_usage_)
    {
    }
    bool operator==(const ResourceUsageState &rhs) const {
        return ((_diskState == rhs._diskState) &&
                (_memoryState == rhs._memoryState) &&
                (_transient_disk_usage == rhs._transient_disk_usage) &&
                (_transient_memory_usage == rhs._transient_memory_usage));
    }
    bool operator!=(const ResourceUsageState &rhs) const {
        return ! ((*this) == rhs);
    }
    const ResourceUsageWithLimit &diskState() const noexcept { return _diskState; }
    const ResourceUsageWithLimit &memoryState() const noexcept { return _memoryState; }
    double transient_disk_usage() const noexcept { return _transient_disk_usage; }
    double transient_memory_usage() const noexcept { return _transient_memory_usage; }
    double non_transient_disk_usage() const { return std::max(0.0, _diskState.usage() - _transient_disk_usage); }
    double non_transient_memory_usage() const { return std::max(0.0, _memoryState.usage() - _transient_memory_usage); }
    bool aboveDiskLimit(double resourceLimitFactor) const { return diskState().aboveLimit(resourceLimitFactor); }
    bool aboveMemoryLimit(double resourceLimitFactor) const { return memoryState().aboveLimit(resourceLimitFactor); }
};

} // namespace proton

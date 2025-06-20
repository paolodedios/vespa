// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/searchcore/proton/attribute/attribute_usage_stats.h>
#include <vespa/searchcore/proton/attribute/i_attribute_usage_listener.h>
#include <vespa/searchcore/proton/server/i_resource_usage_listener.h>
#include <vespa/persistence/spi/resource_usage.h>
#include <vespa/vespalib/stllike/hash_map.h>
#include <mutex>
#include <memory>
#include <vector>

namespace storage::spi { class IResourceUsageListener; }
namespace vespalib { class IDestructorCallback; }

namespace proton {

class ResourceUsageState;
class IResourceUsageNotifier;

/*
 * Class tracking resource usage for persistence provider.
 */
class ResourceUsageTracker : public std::enable_shared_from_this<ResourceUsageTracker>,
                             public IResourceUsageListener,
                             public IAttributeUsageListener
{
    class ListenerGuard;
    mutable std::mutex          _lock;
    storage::spi::ResourceUsage _resource_usage;
    storage::spi::IResourceUsageListener* _listener;
    IResourceUsageNotifier&      _resource_usage_notifier;
    vespalib::hash_map<std::string, AttributeUsageStats> _attribute_usage;
    std::string            _attribute_address_space_max_document_type;
    void remove_listener();
public:
    ResourceUsageTracker(IResourceUsageNotifier& resource_usage_notifier);
    ~ResourceUsageTracker() override;
    storage::spi::ResourceUsage get_resource_usage() const;
    void notify_resource_usage(const ResourceUsageState& state) override;
    std::unique_ptr<vespalib::IDestructorCallback> set_listener(storage::spi::IResourceUsageListener& listener);
    void notify_attribute_usage(const AttributeUsageStats &attribute_usage) override;
};

}

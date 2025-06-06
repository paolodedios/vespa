// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "content_node_message_stats_tracker.h"
#include "nodeinfo.h"
#include <vespa/storageapi/message/bucket.h>
#include <vespa/storageframework/generic/component/component.h>
#include <vespa/storageframework/generic/component/componentregister.h>
#include <vespa/storageframework/generic/status/htmlstatusreporter.h>
#include <vespa/vespalib/stllike/hash_map.h>
#include <vespa/vespalib/stllike/hash_set.h>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <unordered_map>

namespace storage::distributor {

/**
 * Since the state a deferred task depends on may have changed between the
 * time a task was scheduled and when it's actually executed, this enum provides
 * a means of communicating if a task should be started as normal.
 */
enum class TaskRunState {
    OK,        // Task may be started as normal
    Aborted,   // Task should trigger an immediate abort behavior (distributor is shutting down)
    BucketLost // Task should trigger an immediate abort behavior (bucket no longer present on node)
};

/**
 * Represents an arbitrary task whose execution may be deferred until no
 * further pending operations are present.
 */
struct DeferredTask {
    virtual ~DeferredTask() = default;
    virtual void run(TaskRunState state) = 0;
};

template <typename Func>
class LambdaDeferredTask : public DeferredTask {
    Func _func;
public:
    explicit LambdaDeferredTask(Func&& f) : _func(std::move(f)) {}
    LambdaDeferredTask(const LambdaDeferredTask&) = delete;
    LambdaDeferredTask(LambdaDeferredTask&&) = delete;
    ~LambdaDeferredTask() override = default;

    void run(TaskRunState state) override {
        _func(state);
    }
};

template <typename Func>
std::unique_ptr<DeferredTask> make_deferred_task(Func&& f) {
    return std::make_unique<LambdaDeferredTask<std::decay_t<Func>>>(std::forward<Func>(f));
}

class PendingMessageTracker : public framework::HtmlStatusReporter {
public:
    class Checker {
    public:
        virtual ~Checker() = default;
        virtual bool check(uint32_t messageType, uint16_t node, uint8_t priority) = 0;
    };

    using TimePoint = vespalib::system_time;

    PendingMessageTracker(framework::ComponentRegister&, uint32_t stripe_index);
    ~PendingMessageTracker() override;

    void insert(const std::shared_ptr<api::StorageMessage>&);
    document::Bucket reply(const api::StorageReply& reply);
    void reportHtmlStatus(std::ostream&, const framework::HttpUrlPath&) const override;

    void print(std::ostream& out, bool verbose, const std::string& indent) const;

    /**
     * Goes through each pending message for the given node+bucket pair,
     * passing it to the given type checker.
     * Breaks when the checker returns false.
     */
    void checkPendingMessages(uint16_t node, const document::Bucket& bucket, Checker& checker) const;

    /**
     * Goes through each pending message (across all nodes) for the given bucket
     * and invokes the given checker with the node, message type and priority.
     * Breaks when the checker returns false.
     */
    void checkPendingMessages(const document::Bucket& bucket, Checker& checker) const;

    /**
     * Utility function for checking if there's a message of type
     * messageType pending to bucket bid on the given node.
     */
    bool hasPendingMessage(uint16_t node, const document::Bucket& bucket, uint32_t messageType) const;

    /**
     * Returns a vector containing the number of pending messages to each storage node.
     * The vector might be smaller than a given node index. In that case, that storage
     * node has never had any pending messages.
     */
    const NodeInfo& getNodeInfo() const noexcept { return _nodeInfo; }
    NodeInfo& getNodeInfo() noexcept { return _nodeInfo; }

    [[nodiscard]] ContentNodeMessageStatsTracker::NodeStats content_node_stats() const;

    /**
     * Clears all pending messages for the given node, and returns
     * the messages erased.
     */
    std::vector<uint64_t> clearMessagesForNode(uint16_t node);

    void setNodeBusyDuration(vespalib::duration duration) noexcept {
        _nodeBusyDuration = duration;
    }

    void run_once_no_pending_for_bucket(const document::Bucket& bucket, std::unique_ptr<DeferredTask> task);
    void abort_deferred_tasks();

    /**
     * For each distinct bucket with at least one pending message towards it:
     *
     * Iff `bucket_predicate(bucket) == true`, `msg_id_callback` is invoked once for _each_
     * message towards `bucket`, with the message ID as the argument.
     *
     * Note: `bucket_predicate` is only invoked once per distinct bucket.
     */
    void enumerate_matching_pending_bucket_ops(const std::function<bool(const document::Bucket&)>& bucket_predicate,
                                               const std::function<void(uint64_t)>& msg_id_callback) const;
private:
    struct MessageEntry {
        TimePoint        timeStamp;
        uint32_t         msgType;
        uint32_t         priority;
        uint64_t         msgId;
        document::Bucket bucket;
        uint16_t         nodeIdx;

        MessageEntry(TimePoint timeStamp, uint32_t msgType, uint32_t priority,
                     uint64_t msgId, document::Bucket bucket, uint16_t nodeIdx) noexcept;
        [[nodiscard]] std::string toHtml() const;
    };

    struct MessageIdKey : boost::multi_index::member<MessageEntry, uint64_t, &MessageEntry::msgId> {};

    /**
     * Each entry has a separate composite keyed index on node+bucket id+type.
     * This makes it efficient to find all messages for a node, for a bucket
     * on that node and specific message types to an exact bucket on the node.
     */
    struct CompositeNodeBucketKey
        : boost::multi_index::composite_key<
              MessageEntry,
              boost::multi_index::member<MessageEntry, uint16_t, &MessageEntry::nodeIdx>,
              boost::multi_index::member<MessageEntry, document::Bucket, &MessageEntry::bucket>,
              boost::multi_index::member<MessageEntry, uint32_t, &MessageEntry::msgType>
          >
    {
    };

    struct CompositeBucketMsgNodeKey
        : boost::multi_index::composite_key<
              MessageEntry,
              boost::multi_index::member<MessageEntry, document::Bucket, &MessageEntry::bucket>,
              boost::multi_index::member<MessageEntry, uint32_t, &MessageEntry::msgType>,
              boost::multi_index::member<MessageEntry, uint16_t, &MessageEntry::nodeIdx>
          >
    {
    };

    using Messages = boost::multi_index::multi_index_container <
        MessageEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<MessageIdKey>,
            boost::multi_index::ordered_non_unique<CompositeNodeBucketKey>,
            boost::multi_index::ordered_non_unique<CompositeBucketMsgNodeKey>
        >
    >;

    // Must match Messages::nth_index<N>
    static constexpr uint32_t IndexByMessageId     = 0;
    static constexpr uint32_t IndexByNodeAndBucket = 1;
    static constexpr uint32_t IndexByBucketAndType = 2;

    using DeferredBucketTaskMap = std::unordered_multimap<
            document::Bucket,
            std::unique_ptr<DeferredTask>,
            document::Bucket::hash
        >;

    Messages                       _messages;
    framework::Component           _component;
    NodeInfo                       _nodeInfo;
    vespalib::duration             _nodeBusyDuration;
    DeferredBucketTaskMap          _deferred_read_tasks;
    ContentNodeMessageStatsTracker _node_message_stats_tracker;
    mutable std::atomic<bool>      _trackTime;

    // Protects sampling of content node statistics and status page rendering, as this can happen
    // from arbitrary other threads than the owning stripe's worker thread.
    mutable std::mutex _lock;

    void getStatusStartPage(std::ostream& out) const;
    void getStatusPerNode(std::ostream& out) const;
    void getStatusPerBucket(std::ostream& out) const;
    TimePoint currentTime() const;

    [[nodiscard]] bool bucket_has_no_pending_write_ops(const document::Bucket& bucket) const noexcept;
    [[nodiscard]] std::vector<std::unique_ptr<DeferredTask>> get_deferred_ops_if_bucket_writes_drained(const document::Bucket&);
    void get_and_erase_deferred_tasks_for_bucket(const document::Bucket&, std::vector<std::unique_ptr<DeferredTask>>&);
};

}

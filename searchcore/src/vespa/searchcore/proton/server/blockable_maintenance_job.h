// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "i_blockable_maintenance_job.h"
#include "i_move_operation_limiter.h"
#include <mutex>
#include <unordered_set>

namespace proton {

class BlockableMaintenanceJobConfig;
class ResourceUsageState;
class IMaintenanceJobRunner;
struct IMoveOperationLimiter;
class MaintenanceJobTokenSource;

/**
 * Implementation of a maintenance job that can be blocked and unblocked due to various external reasons.
 * A blocked job is not executed by the IMaintenanceJobRunner wrapping the job.
 * When unblocked for a given reason, the job is scheduled for execution again if it is totally unblocked.
 */
class BlockableMaintenanceJob : public IBlockableMaintenanceJob {
private:
    using LockGuard = std::lock_guard<std::mutex>;
    using ReasonSet = std::unordered_set<BlockedReason>;

    mutable std::mutex     _mutex;
    ReasonSet              _blockReasons;
    bool                   _blocked;
    IMaintenanceJobRunner *_runner;
    double                 _resourceLimitFactor;
    std::shared_ptr<IMoveOperationLimiter> _moveOpsLimiter;
protected:
    std::shared_ptr<MaintenanceJobToken>       _token;
    std::shared_ptr<MaintenanceJobTokenSource> _token_source;

private:
    void updateBlocked(const LockGuard &guard);
protected:
    void internal_notify_resource_usage(const ResourceUsageState &state);

public:
    BlockableMaintenanceJob(const std::string &name,
                            vespalib::duration delay,
                            vespalib::duration interval);

    BlockableMaintenanceJob(const std::string &name,
                            vespalib::duration delay,
                            vespalib::duration interval,
                            const BlockableMaintenanceJobConfig &config);

    ~BlockableMaintenanceJob() override;

    bool isBlocked(BlockedReason reason);
    void considerRun();

    void onStop() override;
    void setBlocked(BlockedReason reason) override;
    void unBlock(BlockedReason reason) override;
    bool isBlocked() const override;
    void got_token(std::shared_ptr<MaintenanceJobToken> token, bool sync) override;
    void registerRunner(IMaintenanceJobRunner *runner) override { _runner = runner; }
    IMoveOperationLimiter & getLimiter() { return *_moveOpsLimiter; }
    const IMoveOperationLimiter & getLimiter() const { return *_moveOpsLimiter; }
};

}

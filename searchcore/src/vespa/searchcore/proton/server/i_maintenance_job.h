// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <vespa/vespalib/util/time.h>
#include <atomic>
#include <memory>
#include <string>

namespace proton {

class IBlockableMaintenanceJob;
class IMaintenanceJobRunner;
struct DocumentDBTaggedMetrics;

/**
 * Interface for a maintenance job that is executed after "delay" seconds and
 * then every "interval" seconds.
 */
class IMaintenanceJob
{
private:
    const std::string     _name;
    const vespalib::duration   _delay;
    const vespalib::duration   _interval;
    std::atomic<bool>          _stopped;
protected:
    virtual void onStop() = 0;
public:
    using UP = std::unique_ptr<IMaintenanceJob>;
    using SP = std::shared_ptr<IMaintenanceJob>;

    IMaintenanceJob(const std::string &name,
                    vespalib::duration delay,
                    vespalib::duration interval)
        : _name(name),
          _delay(delay),
          _interval(interval),
          _stopped(false)
    {}

    virtual ~IMaintenanceJob() = default;

    virtual const std::string &getName() const { return _name; }
    virtual vespalib::duration getDelay() const { return _delay; }
    virtual vespalib::duration getInterval() const { return _interval; }
    virtual bool isBlocked() const { return false; }
    virtual IBlockableMaintenanceJob *asBlockable() { return nullptr; }
    virtual void updateMetrics(DocumentDBTaggedMetrics &) const {}
    void stop() {
        _stopped = true;
        onStop();
    }
    bool stopped() const { return _stopped.load(std::memory_order_relaxed); }
    /**
     * Register maintenance job runner, in case event passed to the
     * job causes it to want to be run again.
     */
    virtual void registerRunner(IMaintenanceJobRunner *runner) {
        (void) runner;
    }

    /**
     * Run this maintenance job every "interval" seconds in an external executor thread.
     * It is first executed after "delay" seconds.
     *
     * Return true if the job was finished (it will be executed again in "interval" seconds).
     * Return false if the job was not finished and need to be executed again immediately. This
     * should be used to split up a large job to avoid starvation of other tasks that also are
     * executed on the external executor thread.
     */
    virtual bool run() = 0;
};

} // namespace proton


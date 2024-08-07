// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.provision.maintenance;

import com.yahoo.jdisc.Metric;
import com.yahoo.vespa.hosted.provision.Node;
import com.yahoo.vespa.hosted.provision.NodeList;
import com.yahoo.vespa.hosted.provision.NodeRepository;
import com.yahoo.vespa.hosted.provision.provisioning.HostProvisioner;
import com.yahoo.vespa.hosted.provision.provisioning.ThrottleProvisioningException;

import java.time.Duration;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Deprovisions hosts that are no longer needed in dynamically provisioned systems.
 *
 * @author freva
 */
public class HostDeprovisioner extends NodeRepositoryMaintainer {

    private static final Logger log = Logger.getLogger(HostDeprovisioner.class.getName());

    private final HostProvisioner hostProvisioner;

    HostDeprovisioner(NodeRepository nodeRepository, Duration interval, Metric metric, HostProvisioner hostProvisioner) {
        super(nodeRepository, interval, metric);
        this.hostProvisioner = hostProvisioner;
    }

    @Override
    protected double maintain() {
        NodeList allNodes = nodeRepository().nodes().list();
        NodeList hosts = allNodes.parents().matching(HostCapacityMaintainer::canDeprovision);

        int failures = 0;
        for (Node host : hosts) {
            // This shouldn't be possible since failed, parked, and wantToDeprovision should be recursive
            if (!allNodes.childrenOf(host).stream().allMatch(HostCapacityMaintainer::canDeprovision))
                continue;

            try {
                // Technically we should do this under application lock, but
                // * HostProvisioner::deprovision may take some time since we are waiting for request(s) against
                //   the cloud provider
                // * Because the application lock is shared between all hosts of the same type we want to avoid
                //   holding it over longer periods
                // * We are about to remove these hosts anyway, so only reason we'd want to hold the lock is
                //   if we want to support aborting deprovision if operator manually intervenes
                if (hostProvisioner.deprovision(host))
                    nodeRepository().nodes().removeRecursively(host, true);
            } catch (ThrottleProvisioningException e) {
                log.log(Level.INFO, "Failed to deprovision " + host.hostname() + ", will retry in " + interval() + ": " + e.getMessage());
                break;
            } catch (RuntimeException e) {
                failures++;
                log.log(Level.WARNING, "Failed to deprovision " + host.hostname() + ", will retry in " + interval(), e);
            }
        }
        return asSuccessFactorDeviation(hosts.size(), failures);
    }

}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.model.test;

import com.yahoo.config.application.api.ApplicationPackage;
import com.yahoo.config.model.ConfigModelRegistry;
import com.yahoo.config.model.NullConfigModelRegistry;
import com.yahoo.config.model.api.ApplicationClusterEndpoint;
import com.yahoo.config.model.api.ContainerEndpoint;
import com.yahoo.config.model.api.HostProvisioner;
import com.yahoo.config.model.deploy.DeployState;
import com.yahoo.config.model.deploy.TestProperties;
import com.yahoo.config.model.provision.Host;
import com.yahoo.config.model.provision.Hosts;
import com.yahoo.config.model.provision.InMemoryProvisioner;
import com.yahoo.config.model.provision.SingleNodeProvisioner;
import com.yahoo.config.provision.ApplicationId;
import com.yahoo.config.provision.Capacity;
import com.yahoo.config.provision.ClusterSpec;
import com.yahoo.config.provision.Flavor;
import com.yahoo.config.provision.HostSpec;
import com.yahoo.config.provision.NodeResources;
import com.yahoo.config.provision.ProvisionLogger;
import com.yahoo.config.provision.Zone;
import com.yahoo.vespa.model.VespaModel;
import com.yahoo.vespa.model.test.utils.VespaModelCreatorWithMockPkg;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

import static com.yahoo.vespa.model.test.utils.ApplicationPackageUtils.generateSchemas;

/**
 * Helper class which sets up a system with multiple hosts.
 * Usage:
 * <code>
 *     VespaModelTester tester = new VespaModelTester();
 *     tester.addHosts(count, flavor);
 *     ... add more nodes
 *     VespaModel model = tester.createModel(servicesString);
 *     ... assert on model
 * </code>
 * 
 * @author bratseth
 */
public class VespaModelTester {

    private final ConfigModelRegistry configModelRegistry;

    private boolean hosted = true;
    private TestProperties modelProperties = new TestProperties();
    private final Map<NodeResources, Collection<Host>> hostsByResources = new HashMap<>();
    private ApplicationId applicationId = ApplicationId.defaultId();
    private boolean useDedicatedNodeForLogserver = false;
    private HostProvisioner provisioner;

    public VespaModelTester() {
        this(new NullConfigModelRegistry());
    }

    public VespaModelTester(ConfigModelRegistry configModelRegistry) {
        this.configModelRegistry = configModelRegistry;
    }

    public HostProvisioner provisioner() {
        if (provisioner instanceof ProvisionerAdapter)
            return ((ProvisionerAdapter)provisioner).provisioner();
        return provisioner;
    }

    /** Adds some nodes with resources 1, 3, 10 */
    public Hosts addHosts(int count) { return addHosts(InMemoryProvisioner.defaultHostResources, count); }

    public Hosts addHosts(NodeResources resources, int count) {
        return addHosts(Optional.of(new Flavor(resources)), resources, count);
    }

    private Hosts addHosts(Optional<Flavor> flavor, NodeResources resources, int count) {
        List<Host> hosts = new ArrayList<>();

        for (int i = 0; i < count; ++i) {
            // Let host names sort in the opposite order of the order the hosts are added
            // This allows us to test index vs. name order selection when subsets of hosts are selected from a cluster
            // (for e.g cluster controllers and slobrok nodes)
            String hostname = String.format("%s-%02d",
                                            "node" + "-" + Math.round(resources.vcpu()) +
                                                     "-" + Math.round(resources.memoryGiB()) +
                                                     "-" + Math.round(resources.diskGb()),
                                            count - i);
            hosts.add(new Host(hostname, List.of(), flavor));
        }
        this.hostsByResources.put(resources, hosts);

        if (hosts.size() > 100)
            throw new IllegalStateException("The host naming scheme is nameNN. To test more than 100 hosts, change to nameNNN");
        return new Hosts(hosts);
    }

    /** Sets whether this sets up a model for a hosted system. Default: true */
    public void setHosted(boolean hosted) { this.hosted = hosted; }

    /** Sets whether this sets up a model for a hosted system. Default: true */
    public void setModelProperties(TestProperties testProperties) { this.modelProperties = testProperties; }

    /** Sets the tenant, application name, and instance name of the model being built. */
    public void setApplicationId(String tenant, String applicationName, String instanceName) {
        applicationId = ApplicationId.from(tenant, applicationName, instanceName);
    }

    public void useDedicatedNodeForLogserver(boolean useDedicatedNodeForLogserver) {
        this.useDedicatedNodeForLogserver = useDedicatedNodeForLogserver;
    }

    /** Creates a model which uses 0 as start index and fails on out of capacity */
    public VespaModel createModel(String services, String ... retiredHostNames) {
        return createModel(Zone.defaultZone(), services, true, retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, boolean failOnOutOfCapacity, String ... retiredHostNames) {
        return createModel(services, null, failOnOutOfCapacity, retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, String hosts, boolean failOnOutOfCapacity, String ... retiredHostNames) {
        return createModel(Zone.defaultZone(), services, hosts, failOnOutOfCapacity, false, false,
                           NodeResources.unspecified(), 0,
                           Optional.empty(), new DeployState.Builder(), retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, boolean failOnOutOfCapacity, DeployState.Builder builder) {
        return createModel(Zone.defaultZone(), services, failOnOutOfCapacity, false, false,
                           NodeResources.unspecified(), 0, Optional.empty(), builder);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, boolean failOnOutOfCapacity, boolean useMaxResources, String ... retiredHostNames) {
        return createModel(Zone.defaultZone(), services, failOnOutOfCapacity, useMaxResources, false,
                           NodeResources.unspecified(), 0,
                           Optional.empty(), new DeployState.Builder(), retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, boolean failOnOutOfCapacity, boolean useMaxResources, boolean alwaysReturnOneNode, String ... retiredHostNames) {
        return createModel(Zone.defaultZone(), services, failOnOutOfCapacity, useMaxResources, alwaysReturnOneNode,
                           NodeResources.unspecified(), 0,
                           Optional.empty(), new DeployState.Builder(), retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(String services, boolean failOnOutOfCapacity, NodeResources defaultResources,
                                  int startIndexForClusters, String ... retiredHostNames) {
        return createModel(Zone.defaultZone(), services, failOnOutOfCapacity, false, false,
                           defaultResources, startIndexForClusters,
                           Optional.empty(), new DeployState.Builder(), retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(Zone zone, String services, boolean failOnOutOfCapacity, String ... retiredHostNames) {
        return createModel(zone, services, failOnOutOfCapacity, false, false, NodeResources.unspecified(), 0,
                           Optional.empty(), new DeployState.Builder(), retiredHostNames);
    }

    /** Creates a model which uses 0 as start index */
    public VespaModel createModel(Zone zone, String services, boolean failOnOutOfCapacity,
                                  DeployState.Builder deployStateBuilder, String ... retiredHostNames) {
        return createModel(zone, services, failOnOutOfCapacity, false, false,
                           NodeResources.unspecified(),0,
                           Optional.empty(), deployStateBuilder, retiredHostNames);
    }

    public VespaModel createModel(Zone zone, String services, boolean failOnOutOfCapacity, boolean useMaxResources,
                                  boolean alwaysReturnOneNode,
                                  NodeResources defaultResources,
                                  int startIndexForClusters, Optional<VespaModel> previousModel,
                                  DeployState.Builder deployStatebuilder, String ... retiredHostNames) {
        return createModel(zone, services, null, failOnOutOfCapacity, useMaxResources, alwaysReturnOneNode,
                           defaultResources,
                           startIndexForClusters, previousModel, deployStatebuilder, retiredHostNames);
    }
    /**
     * Creates a model using the hosts already added to this
     *
     * @param services the services xml string
     * @param hosts the hosts xml string, or null if none
     * @param useMaxResources false to use the minimal resources (when given a range), true to use max
     * @param failOnOutOfCapacity whether we should get an exception when not enough hosts of the requested flavor
     *        is available or if we should just silently receive a smaller allocation
     * @return the resulting model
     */
    public VespaModel createModel(Zone zone, String services, String hosts, boolean failOnOutOfCapacity, boolean useMaxResources,
                                  boolean alwaysReturnOneNode,
                                  NodeResources defaultResources,
                                  int startIndexForClusters, Optional<VespaModel> previousModel,
                                  DeployState.Builder deployStateBuilder, String ... retiredHostNames) {
        VespaModelCreatorWithMockPkg modelCreatorWithMockPkg = new VespaModelCreatorWithMockPkg(hosts, services, generateSchemas("type1"));
        ApplicationPackage appPkg = modelCreatorWithMockPkg.appPkg;

        Set<ContainerEndpoint> containerEndpoints = deployStateBuilder.build().getEndpoints();
        if (hosted) {
            InMemoryProvisioner provisioner = new InMemoryProvisioner(hostsByResources,
                                                                      failOnOutOfCapacity,
                                                                      useMaxResources,
                                                                      alwaysReturnOneNode,
                                                                      false,
                                                                      defaultResources,
                                                                      startIndexForClusters,
                                                                      retiredHostNames);
            provisioner.setEnvironment(zone.environment());
            this.provisioner = new ProvisionerAdapter(provisioner);
            if (containerEndpoints.isEmpty()) {
                containerEndpoints = Set.of(new ContainerEndpoint("default", ApplicationClusterEndpoint.Scope.zone, List.of("default.example.com")));
            }
        } else {
            provisioner = new SingleNodeProvisioner();
        }

        TestProperties properties = modelProperties
                .setMultitenant(hosted) // Note: system tests are multitenant but not hosted
                .setHostedVespa(hosted)
                .setApplicationId(applicationId)
                .setUseDedicatedNodeForLogserver(useDedicatedNodeForLogserver);

        DeployState.Builder deployState = deployStateBuilder
                .applicationPackage(appPkg)
                .modelHostProvisioner(provisioner)
                .properties(properties)
                .endpoints(containerEndpoints)
                .zone(zone);
        previousModel.ifPresent(deployState::previousModel);
        return modelCreatorWithMockPkg.create(false, deployState.build(), configModelRegistry);
    }

    /** To verify that we don't call allocateHost(alias) in hosted environments */
    private static class ProvisionerAdapter implements HostProvisioner {

        private final InMemoryProvisioner provisioner;

        public ProvisionerAdapter(InMemoryProvisioner provisioner) {
            this.provisioner = provisioner;
        }

        public InMemoryProvisioner provisioner() { return provisioner; }

        @Override
        public HostSpec allocateHost(String alias) {
            throw new UnsupportedOperationException("Allocating hosts using <node> tags is not supported in hosted environments, " +
                                                    "use <nodes count='N'> instead, see https://docs.vespa.ai/en/reference/services.html#nodes");
        }

        @Override
        public List<HostSpec> prepare(ClusterSpec cluster, Capacity capacity, ProvisionLogger logger) {
            return provisioner.prepare(cluster, capacity, logger);
        }

    }
}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.provision.node;

import com.yahoo.component.Version;
import com.yahoo.config.provision.DockerImage;
import com.yahoo.vespa.hosted.provision.backup.Snapshot;

import java.time.Instant;
import java.util.Objects;
import java.util.Optional;

/**
 * Information about current status of a node. This is immutable.
 *
 * @author bratseth
 */
public class Status {

    private final Generation reboot;
    private final Optional<Version> vespaVersion;
    private final Optional<DockerImage> containerImage;
    private final int failCount;
    private final boolean wantToRetire;
    private final boolean wantToDeprovision;
    private final boolean wantToRebuild;
    private final boolean bootingAfterRebuild;
    private final boolean preferToRetire;
    private final boolean wantToFail;
    private final boolean wantToUpgradeFlavor;
    private final OsVersion osVersion;
    private final Optional<Instant> firmwareVerifiedAt;
    private final Optional<Snapshot> snapshot;

    public Status(Generation generation,
                  Optional<Version> vespaVersion,
                  Optional<DockerImage> containerImage,
                  int failCount,
                  boolean wantToRetire,
                  boolean wantToDeprovision,
                  boolean wantToRebuild,
                  boolean bootingAfterRebuild,
                  boolean preferToRetire,
                  boolean wantToFail,
                  boolean wantToUpgradeFlavor,
                  OsVersion osVersion,
                  Optional<Instant> firmwareVerifiedAt,
                  Optional<Snapshot> snapshot) {
        this.reboot = Objects.requireNonNull(generation, "Generation must be non-null");
        this.vespaVersion = Objects.requireNonNull(vespaVersion, "Vespa version must be non-null").filter(v -> !Version.emptyVersion.equals(v));
        this.containerImage = Objects.requireNonNull(containerImage, "Container image must be non-null").filter(d -> !DockerImage.EMPTY.equals(d));
        this.failCount = failCount;
        this.wantToUpgradeFlavor = wantToUpgradeFlavor;
        this.snapshot = Objects.requireNonNull(snapshot, "snapshot must be non-null");
        if (wantToDeprovision && wantToRebuild) {
            throw new IllegalArgumentException("Node cannot be marked both wantToDeprovision and wantToRebuild");
        }
        if (wantToDeprovision && !wantToRetire) {
            throw new IllegalArgumentException("Node cannot be marked wantToDeprovision unless it's also marked wantToRetire");
        }
        if (wantToUpgradeFlavor && !wantToRetire) {
            throw new IllegalArgumentException("Node cannot be marked wantToUpgradeFlavor unless it's also marked wantToRetire");
        }
        this.wantToRetire = wantToRetire;
        this.wantToDeprovision = wantToDeprovision;
        this.wantToRebuild = wantToRebuild;
        this.bootingAfterRebuild = bootingAfterRebuild;
        this.preferToRetire = preferToRetire;
        this.wantToFail = wantToFail;
        this.osVersion = Objects.requireNonNull(osVersion, "OS version must be non-null");
        this.firmwareVerifiedAt = Objects.requireNonNull(firmwareVerifiedAt, "Firmware check instant must be non-null");
    }

    /** Returns a copy of this with the reboot generation changed */
    public Status withReboot(Generation reboot) { return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    /** Returns the reboot generation of this node */
    public Generation reboot() { return reboot; }

    /** Returns a copy of this with the vespa version changed */
    public Status withVespaVersion(Version version) { return new Status(reboot, Optional.of(version), containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    /** Returns the Vespa version installed on the node, if known */
    public Optional<Version> vespaVersion() { return vespaVersion; }

    /** Returns a copy of this with the container image changed */
    public Status withContainerImage(DockerImage containerImage) { return new Status(reboot, vespaVersion, Optional.of(containerImage), failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    /** Returns the container image the node is running, if any */
    public Optional<DockerImage> containerImage() { return containerImage; }

    public Status withIncreasedFailCount() { return new Status(reboot, vespaVersion, containerImage, failCount + 1, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    public Status withDecreasedFailCount() { return new Status(reboot, vespaVersion, containerImage, failCount - 1, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    public Status withFailCount(int value) { return new Status(reboot, vespaVersion, containerImage, value, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot); }

    /** Returns how many times this node has been moved to the failed state. */
    public int failCount() { return failCount; }

    /** Returns a copy of this with the want to retire/deprovision/rebuild flags changed */
    public Status withWantToRetire(boolean wantToRetire, boolean wantToDeprovision, boolean wantToRebuild, boolean bootingAfterRebuild, boolean wantToUpgradeFlavor) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot);
    }

    /**
     * Returns whether this node is requested to retire. This is a hard request to retire, which allows any replacement
     * to increase node skew in the cluster.
     */
    public boolean wantToRetire() { return wantToRetire; }

    /**
     * Returns whether this node should be de-provisioned when possible.
     */
    public boolean wantToDeprovision() { return wantToDeprovision; }

    /** Returns whether this node should be rebuilt when possible. */
    public boolean wantToRebuild() { return wantToRebuild; }

    /** Returns whether this node is currently starting a rebuild. */
    public boolean bootingAfterRebuild() { return bootingAfterRebuild; }

    /**
     * Returns whether this node is requested to retire. Unlike {@link Status#wantToRetire()}, this is a soft
     * request to retire, which will not allow any replacement to increase node skew in the cluster.
     */
    public boolean preferToRetire() { return preferToRetire; }

    /** Returns whether the flavor of is node is required to be of the latest generation */
    public boolean wantToUpgradeFlavor() {
        return wantToUpgradeFlavor;
    }

    /** Returns a copy of this with want to fail set to the given value */
    public Status withWantToFail(boolean wantToFail) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot);
    }

    /** Returns whether this node should be failed */
    public boolean wantToFail() { return wantToFail; }

    /** Returns a copy of this with prefer-to-retire set to given value */
    public Status withPreferToRetire(boolean preferToRetire) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot);
    }

    /** Returns a copy of this with the OS version set to given version */
    public Status withOsVersion(OsVersion version) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, version, firmwareVerifiedAt, snapshot);
    }

    /** Returns the OS version of this node */
    public OsVersion osVersion() {
        return osVersion;
    }

    /** Returns a copy of this with the firmwareVerifiedAt set to the given instant. */
    public Status withFirmwareVerifiedAt(Instant instant) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, Optional.of(instant), snapshot);
    }

    /** Returns the last time this node had firmware that was verified to be up to date. */
    public Optional<Instant> firmwareVerifiedAt() {
        return firmwareVerifiedAt;
    }

    /** Returns the current backup snapshot this node should consider, if any */
    public Optional<Snapshot> snapshot() {
        return snapshot;
    }

    /** Returns a copy of this with backup snapshot set to given value */
    public Status withSnapshot(Optional<Snapshot> snapshot) {
        return new Status(reboot, vespaVersion, containerImage, failCount, wantToRetire, wantToDeprovision, wantToRebuild, bootingAfterRebuild, preferToRetire, wantToFail, wantToUpgradeFlavor, osVersion, firmwareVerifiedAt, snapshot);
    }

    /** Returns the initial status of a newly provisioned node */
    public static Status initial() {
        return new Status(Generation.initial(), Optional.empty(), Optional.empty(), 0, false,
                          false, false, false, false, false, false, OsVersion.EMPTY, Optional.empty(), Optional.empty());
    }

}
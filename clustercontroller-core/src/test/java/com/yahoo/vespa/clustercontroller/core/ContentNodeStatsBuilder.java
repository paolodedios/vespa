// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.clustercontroller.core;

import java.util.HashMap;
import java.util.Map;

/**
 * Builder used for testing only.
 */
public class ContentNodeStatsBuilder {

    private final int nodeIndex;
    private final Map<String, ContentNodeStats.BucketSpaceStats> stats = new HashMap<>();

    private ContentNodeStatsBuilder(int nodeIndex) {
        this.nodeIndex = nodeIndex;
    }

    public static ContentNodeStatsBuilder forNode(int nodeIndex) {
        return new ContentNodeStatsBuilder(nodeIndex);
    }

    public ContentNodeStatsBuilder add(String bucketSpace, long bucketsTotal, long bucketsPending) {
        return add(bucketSpace, ContentNodeStats.BucketSpaceStats.of(bucketsTotal, bucketsPending));
    }

    public ContentNodeStatsBuilder addInvalid(String bucketSpace, long bucketsTotal, long bucketsPending) {
        return add(bucketSpace, ContentNodeStats.BucketSpaceStats.invalid(bucketsTotal, bucketsPending));
    }

    public ContentNodeStatsBuilder add(String bucketSpace, ContentNodeStats.BucketSpaceStats bucketSpaceStats) {
        stats.put(bucketSpace, bucketSpaceStats);
        return this;
    }

    public ContentNodeStats build() {
        return new ContentNodeStats(nodeIndex, stats);
    }
}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.athenz.api;

import java.time.Instant;

/**
 * @author freva
 */
public record AzureTemporaryCredentials(String azureSubscription,
                                        String azureTenant,
                                        String accessToken,
                                        Instant expiration) {
}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.http;

import org.junit.jupiter.api.Test;

import java.util.Optional;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

/**
 * @author jonmv
 */
class DomainNameTest {

    @Test
    void testNames() {
        DomainName.of("name-123.0.321-eman");
        DomainName.of(("." + "a".repeat(63)).repeat(4).substring(1));
        DomainName.of("123");
        DomainName.of("foo.");

        assertThrows(IllegalArgumentException.class, () -> DomainName.of(""));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("_"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("-"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("."));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("-foo"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("foo-"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of(".foo"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("foo..bar"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("foo.-.bar"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("foo/"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("foo%"));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of(("." + "a".repeat(32)).repeat(8).substring(1, 257)));
        assertThrows(IllegalArgumentException.class, () -> DomainName.of("a".repeat(64)));

        assertLeafAndDomain("foo", "foo", null);
        assertLeafAndDomain("foo.", "foo", null);
        assertLeafAndDomain("foo.com", "foo", "com");
        assertLeafAndDomain("foo.com.", "foo", "com.");
        assertLeafAndDomain("foo.bar.baz.com", "foo", "bar.baz.com");
        assertLeafAndDomain("foo.bar.baz.com.", "foo", "bar.baz.com.");
    }

    private void assertLeafAndDomain(String dns, String leaf, String domainOrNull) {
        DomainName dnsDomainName = DomainName.of(dns);
        assertEquals(leaf, dnsDomainName.leafLabel());
        assertEquals(Optional.ofNullable(domainOrNull), dnsDomainName.parent().map(DomainName::value));
    }

}

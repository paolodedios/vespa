// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.feed.client;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.SSLContext;
import java.net.URI;
import java.nio.file.Path;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.time.Duration;
import java.util.Collection;
import java.util.List;
import java.util.function.Supplier;

/**
 * Builder for creating a {@link FeedClient} instance.
 *
 * @author bjorncs
 * @author jonmv
 */
public interface FeedClientBuilder {

    String PREFERRED_IMPLEMENTATION_PROPERTY = "vespa.feed.client.builder.implementation";

    /**
     * Creates a builder for a single feed container endpoint.
     * This is for feeding against a container cluster with a load balancer in front of it.
     **/
    static FeedClientBuilder create(URI endpoint) { return create(List.of(endpoint)); }

    /**
     * Creates a builder which <em>distributes</em> the feed across the given feed container endpoints.
     * This is for feeding directly against container nodes, i.e., when no load balancer sits in front of these.
     * Each feed operation is sent to <em>one</em> of the endpoints, <strong>not all of them</strong>!
     */
    static FeedClientBuilder create(List<URI> endpoints) {
        return Helper.getFeedClientBuilderSupplier().get().setEndpointUris(endpoints);
    }

    /** Override FeedClientBuilder. This will be preferred in {@link #create} */
    static void setFeedClientBuilderSupplier(Supplier<FeedClientBuilder> supplier) {
        Helper.setFeedClientBuilderSupplier(supplier);
    }
    /**
     * Sets the number of connections this client will use per endpoint.
     *
     * A reasonable value here is a value that lets all feed clients (if more than one)
     * collectively have a number of connections which is a small multiple of the numbers
     * of containers in the cluster to feed, so load can be balanced across these containers.
     * In general, this value should be kept as low as possible, but poor connectivity
     * between feeder and cluster may also warrant a higher number of connections.
     */
    FeedClientBuilder setConnectionsPerEndpoint(int max);

    /**
     * Sets the maximum number of streams per HTTP/2 connection for this client.
     *
     * This determines the maximum number of concurrent, inflight requests for this client,
     * which is {@code maxConnections * maxStreamsPerConnection}. Prefer more streams over
     * more connections, when possible.
     * The feed client automatically throttles load to achieve the best throughput, and the
     * actual number of streams per connection is usually lower than the maximum.
     */
    FeedClientBuilder setMaxStreamPerConnection(int max);

    /** Sets a duration after which this client will recycle active connections. This is off ({@code Duration.ZERO}) by default. */
    FeedClientBuilder setConnectionTimeToLive(Duration ttl);

    /** Sets {@link SSLContext} instance. */
    FeedClientBuilder setSslContext(SSLContext context);

    /** Sets {@link HostnameVerifier} instance (e.g for disabling default SSL hostname verification). */
    FeedClientBuilder setHostnameVerifier(HostnameVerifier verifier);

    /** Sets {@link HostnameVerifier} instance for proxy (e.g for disabling default SSL hostname verification). */
    FeedClientBuilder setProxyHostnameVerifier(HostnameVerifier verifier);

    /** Turns off benchmarking. Attempting to get {@link FeedClient#stats()} will result in an exception. */
    FeedClientBuilder noBenchmarking();

    /** Adds HTTP request header to all client requests. */
    FeedClientBuilder addRequestHeader(String name, String value);

    /**
     * Adds HTTP request header to all client requests. Value {@link Supplier} is invoked for each HTTP request,
     * i.e. value can be dynamically updated during a feed.
     */
    FeedClientBuilder addRequestHeader(String name, Supplier<String> valueSupplier);

    /** Adds HTTP request header to all proxy requests. */
    FeedClientBuilder addProxyRequestHeader(String name, String value);

    /**
     * Adds HTTP request header to all proxy requests. Value {@link Supplier} is invoked for each HTTP request,
     * i.e. value can be dynamically updated for each new proxy connection.
     */
    FeedClientBuilder addProxyRequestHeader(String name, Supplier<String> valueSupplier);

    /**
     * Overrides default retry strategy.
     * @see FeedClient.RetryStrategy
     */
    FeedClientBuilder setRetryStrategy(FeedClient.RetryStrategy strategy);

    /**
     * Overrides default circuit breaker.
     * @see FeedClient.CircuitBreaker
     */
    FeedClientBuilder setCircuitBreaker(FeedClient.CircuitBreaker breaker);

    /** Sets path to client SSL certificate/key PEM files */
    FeedClientBuilder setCertificate(Path certificatePemFile, Path privateKeyPemFile);

    /** Sets client SSL certificates/key */
    FeedClientBuilder setCertificate(Collection<X509Certificate> certificate, PrivateKey privateKey);

    /** Sets client SSL certificate/key */
    FeedClientBuilder setCertificate(X509Certificate certificate, PrivateKey privateKey);

    /** Turns on dryrun mode, where each operation succeeds after a given delay, rather than being sent across the network. */
    FeedClientBuilder setDryrun(boolean enabled);

    /** Turns on speed test mode, where all feed operations are immediately acknowledged by the server. */
    FeedClientBuilder setSpeedTest(boolean enabled);

    /**
     * Overrides JVM default SSL truststore
     * @param caCertificatesFile Path to PEM encoded file containing trusted certificates
     */
    FeedClientBuilder setCaCertificatesFile(Path caCertificatesFile);

    /**
     * Overrides JVM default SSL truststore for proxy
     * @param caCertificatesFile Path to PEM encoded file containing trusted certificates
     */
    FeedClientBuilder setProxyCaCertificatesFile(Path caCertificatesFile);

    /** Overrides JVM default SSL truststore */
    FeedClientBuilder setCaCertificates(Collection<X509Certificate> caCertificates);

    /** Overrides JVM default SSL truststore for proxy */
    FeedClientBuilder setProxyCaCertificates(Collection<X509Certificate> caCertificates);

    /** Overrides endpoint URIs for this client */
    FeedClientBuilder setEndpointUris(List<URI> endpoints);

    /** Specify HTTP(S) proxy for all endpoints */
    FeedClientBuilder setProxy(URI uri);

    /** What compression to use for request bodies; default {@code auto}. */
    FeedClientBuilder setCompression(Compression compression);

    enum Compression { auto, none, gzip }

    /**
     * Sets the initial inflight factor for this client.
     *
     * This determines the initial targetInflight,
     * which is {@code minInflight * initialInflightFactor}.
     */
    FeedClientBuilder setInitialInflightFactor(int factor);

    /** Constructs instance of {@link FeedClient} from builder configuration */
    FeedClient build();

}

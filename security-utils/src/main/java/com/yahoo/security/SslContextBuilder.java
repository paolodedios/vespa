// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.security;

import com.yahoo.security.tls.TlsContext;

import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509ExtendedKeyManager;
import javax.net.ssl.X509ExtendedTrustManager;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.List;

/**
 * A builder for {@link SSLContext}.
 *
 * @author bjorncs
 */
public class SslContextBuilder {

    private KeyStoreSupplier trustStoreSupplier = () -> null;
    private KeyStoreSupplier keyStoreSupplier = () -> null;
    private char[] keyStorePassword;
    private TrustManagerFactory trustManagerFactory = TrustManagerUtils::createDefaultX509TrustManager;
    private KeyManagerFactory keyManagerFactory = KeyManagerUtils::createDefaultX509KeyManager;
    private X509ExtendedKeyManager keyManager;
    private X509ExtendedTrustManager trustManager;

    public SslContextBuilder() {}

    public SslContextBuilder withTrustStore(Path file, KeyStoreType trustStoreType) {
        this.trustStoreSupplier = () -> KeyStoreBuilder.withType(trustStoreType).fromFile(file).build();
        return this;
    }

    public SslContextBuilder withTrustStore(KeyStore trustStore) {
        this.trustStoreSupplier = () -> trustStore;
        return this;
    }

    public SslContextBuilder withTrustStore(X509Certificate caCertificate) {
        return withTrustStore(List.of(caCertificate));
    }

    public SslContextBuilder withTrustStore(List<X509Certificate> caCertificates) {
        this.trustStoreSupplier = () -> createTrustStore(caCertificates);
        return this;
    }

    public SslContextBuilder withTrustStore(Path pemEncodedCaCertificates) {
        this.trustStoreSupplier = () -> {
            List<X509Certificate> caCertificates =
                    X509CertificateUtils.certificateListFromPem(new String(Files.readAllBytes(pemEncodedCaCertificates)));
            return createTrustStore(caCertificates);
        };
        return this;
    }

    public SslContextBuilder withKeyStore(PrivateKey privateKey, X509Certificate certificate) {
        return withKeyStore(privateKey, List.of(certificate));
    }

    public SslContextBuilder withKeyStore(PrivateKey privateKey, List<X509Certificate> certificates) {
        return withKeyStore(List.of(new X509CertificateWithKey(certificates, privateKey)));
    }

    public SslContextBuilder withKeyStore(List<X509CertificateWithKey> clientCertificatesAndKeys) {
        if (clientCertificatesAndKeys.isEmpty()) {
            throw new IllegalArgumentException("clientCertificatesAndKeys cannot be empty");
        }
        this.keyStoreSupplier = () -> {
            KeyStoreBuilder keyStore = KeyStoreBuilder.withType(KeyStoreType.JKS);
            for (int i = 0; i < clientCertificatesAndKeys.size(); i++) {
                X509CertificateWithKey certWithKey = clientCertificatesAndKeys.get(i);
                keyStore = keyStore.withKeyEntry("key"+i, certWithKey.privateKey(), certWithKey.certificateWithIntermediates());
            }
            return keyStore.build();
        };
        this.keyStorePassword = new char[0];
        return this;
    }

    public SslContextBuilder withKeyStore(KeyStore keyStore, char[] password) {
        this.keyStoreSupplier = () -> keyStore;
        this.keyStorePassword = password;
        return this;
    }

    public SslContextBuilder withKeyStore(Path file, char[] password, KeyStoreType keyStoreType) {
        this.keyStoreSupplier = () -> KeyStoreBuilder.withType(keyStoreType).fromFile(file, password).build();
        this.keyStorePassword = password;
        return this;
    }

    public SslContextBuilder withKeyStore(Path privateKeyPemFile, Path certificatesPemFile) {
        this.keyStoreSupplier =
                () ->  {
                    PrivateKey privateKey = KeyUtils.fromPemEncodedPrivateKey(new String(Files.readAllBytes(privateKeyPemFile)));
                    List<X509Certificate> certificates = X509CertificateUtils.certificateListFromPem(new String(Files.readAllBytes(certificatesPemFile)));
                    return KeyStoreBuilder.withType(KeyStoreType.JKS)
                            .withKeyEntry("default", privateKey, certificates)
                            .build();
                };
        this.keyStorePassword = new char[0];
        return this;
    }

    public SslContextBuilder withTrustManagerFactory(TrustManagerFactory trustManagersFactory) {
        this.trustManagerFactory = trustManagersFactory;
        return this;
    }

    public SslContextBuilder withKeyManagerFactory(KeyManagerFactory keyManagerFactory) {
        this.keyManagerFactory = keyManagerFactory;
        return this;
    }

    /**
     * Note: Callee is responsible for configuring the key manager.
     *       Any keystore configured by {@link #withKeyStore(KeyStore, char[])} or the other overloads will be ignored.
     */
    public SslContextBuilder withKeyManager(X509ExtendedKeyManager keyManager) {
        this.keyManager = keyManager;
        return this;
    }

    /**
     * Note: Callee is responsible for configuring the trust manager.
     *       Any truststore configured by {@link #withTrustStore(KeyStore)} or the other overloads will be ignored.
     */
    public SslContextBuilder withTrustManager(X509ExtendedTrustManager trustManager) {
        this.trustManager = trustManager;
        return this;
    }

    public SSLContext build() { return buildContext().context(); }

    /** @return same {@link #build()} but includes the {@link SSLContext} instance's associated trust/key managers */
    public X509SslContext buildContext() {
        try {
            SSLContext sslContext = SSLContext.getInstance(TlsContext.SSL_CONTEXT_VERSION);
            X509ExtendedTrustManager trustManager = this.trustManager != null
                    ? this.trustManager
                    : trustManagerFactory.createTrustManager(trustStoreSupplier.get());
            X509ExtendedKeyManager keyManager = this.keyManager != null
                    ? this.keyManager
                    : keyManagerFactory.createKeyManager(keyStoreSupplier.get(), keyStorePassword);
            sslContext.init(new KeyManager[] {keyManager}, new TrustManager[] {trustManager}, null);
            return new X509SslContext(sslContext, trustManager, keyManager);
        } catch (GeneralSecurityException e) {
            throw new RuntimeException(e);
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
        finally {
            keyStorePassword = null;
        }
    }

    private static KeyStore createTrustStore(List<X509Certificate> caCertificates) {
        return KeyStoreBuilder.withType(KeyStoreType.JKS)
                .withCertificateEntries("cert", caCertificates)
                .build();
    }

    private interface KeyStoreSupplier {
        KeyStore get() throws IOException, GeneralSecurityException;
    }

    /**
     * A factory interface for creating {@link X509ExtendedTrustManager}.
     */
    @FunctionalInterface
    public interface TrustManagerFactory {
        X509ExtendedTrustManager createTrustManager(KeyStore truststore) throws GeneralSecurityException;
    }

    /**
     * A factory interface for creating {@link X509ExtendedKeyManager}.
     */
    @FunctionalInterface
    public interface KeyManagerFactory {
        X509ExtendedKeyManager createKeyManager(KeyStore truststore, char[] password) throws GeneralSecurityException;
    }

}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/vespalib/crypto/private_key.h>
#include <vespa/vespalib/crypto/x509_certificate.h>
#include <vespa/vespalib/data/smart_buffer.h>
#include <vespa/vespalib/net/tls/authorization_mode.h>
#include <vespa/vespalib/net/tls/crypto_codec.h>
#include <vespa/vespalib/net/tls/statistics.h>
#include <vespa/vespalib/net/tls/tls_context.h>
#include <vespa/vespalib/net/tls/transport_security_options.h>
#include <vespa/vespalib/net/tls/impl/openssl_crypto_codec_impl.h>
#include <vespa/vespalib/net/tls/impl/openssl_tls_context_impl.h>
#include <vespa/vespalib/test/make_tls_options_for_testing.h>
#include <vespa/vespalib/test/peer_policy_utils.h>
#include <vespa/vespalib/util/size_literals.h>
#include <stdexcept>
#include <vespa/vespalib/gtest/gtest.h>

using namespace vespalib;
using namespace vespalib::crypto;
using namespace vespalib::net::tls;
using namespace vespalib::net::tls::impl;

const char* decode_state_to_str(DecodeResult::State state) noexcept {
    switch (state) {
    case DecodeResult::State::Failed: return "Broken";
    case DecodeResult::State::OK:     return "OK";
    case DecodeResult::State::NeedsMorePeerData: return "NeedsMorePeerData";
    case DecodeResult::State::Closed: return "Closed";
    }
    abort();
}

const char* hs_state_to_str(HandshakeResult::State state) noexcept {
    switch (state) {
    case HandshakeResult::State::Failed: return "Broken";
    case HandshakeResult::State::Done:   return "Done";
    case HandshakeResult::State::NeedsMorePeerData: return "NeedsMorePeerData";
    case HandshakeResult::State::NeedsWork: return "NeedsWork";
    }
    abort();
}

void print_handshake_result(const char* mode, const HandshakeResult& res) {
    fprintf(stderr, "(handshake) %s consumed %zu peer bytes, wrote %zu peer bytes. State: %s\n",
            mode, res.bytes_consumed, res.bytes_produced,
            hs_state_to_str(res.state));
}

void print_encode_result(const char* mode, const EncodeResult& res) {
    fprintf(stderr, "(encode) %s read %zu plaintext, wrote %zu cipher. State: %s\n",
            mode, res.bytes_consumed, res.bytes_produced,
            res.failed ? "Broken! D:" : "OK");
}

void print_decode_result(const char* mode, const DecodeResult& res) {
    fprintf(stderr, "(decode) %s read %zu cipher, wrote %zu plaintext. State: %s\n",
            mode, res.bytes_consumed, res.bytes_produced,
            decode_state_to_str(res.state));
}

TransportSecurityOptions ts_from_pems(std::string_view ca_certs_pem,
                                      std::string_view cert_chain_pem,
                                      std::string_view private_key_pem)
{
    auto ts_builder = TransportSecurityOptions::Params().
            ca_certs_pem(ca_certs_pem).
            cert_chain_pem(cert_chain_pem).
            private_key_pem(private_key_pem).
            authorized_peers(AuthorizedPeers::allow_all_authenticated());
    return TransportSecurityOptions(std::move(ts_builder));
}

struct Fixture {
    TransportSecurityOptions tls_opts;
    std::shared_ptr<TlsContext> tls_ctx;
    std::unique_ptr<OpenSslCryptoCodecImpl> client;
    std::unique_ptr<OpenSslCryptoCodecImpl> server;
    SmartBuffer client_to_server;
    SmartBuffer server_to_client;

    Fixture()
        : tls_opts(vespalib::test::make_tls_options_for_testing()),
          tls_ctx(TlsContext::create_default_context(tls_opts, AuthorizationMode::Enforce)),
          client(create_openssl_codec(tls_ctx, CryptoCodec::Mode::Client)),
          server(create_openssl_codec(tls_ctx, CryptoCodec::Mode::Server)),
          client_to_server(64_Ki),
          server_to_client(64_Ki)
    {}
    ~Fixture();

    static TransportSecurityOptions create_options_without_own_peer_cert() {
        auto source_opts = vespalib::test::make_tls_options_for_testing();
        return ts_from_pems(source_opts.ca_certs_pem(), "", "");
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec(
            const TransportSecurityOptions& opts, CryptoCodec::Mode mode, const SocketSpec& peer_spec) {
        auto ctx = TlsContext::create_default_context(opts, AuthorizationMode::Enforce);
        return create_openssl_codec(ctx, mode, peer_spec);
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec(
            const TransportSecurityOptions& opts, CryptoCodec::Mode mode) {
        return create_openssl_codec(opts, mode, SocketSpec::invalid);
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec(
            const TransportSecurityOptions& opts,
            std::shared_ptr<CertificateVerificationCallback> cert_verify_callback,
            CryptoCodec::Mode mode) {
        auto ctx = TlsContext::create_default_context(opts, std::move(cert_verify_callback), AuthorizationMode::Enforce);
        return create_openssl_codec(ctx, mode);
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec(
            const std::shared_ptr<TlsContext>& ctx, CryptoCodec::Mode mode, const SocketSpec& peer_spec) {
        auto ctx_impl = std::dynamic_pointer_cast<impl::OpenSslTlsContextImpl>(ctx);
        if (mode == CryptoCodec::Mode::Client) {
            return OpenSslCryptoCodecImpl::make_client_codec(std::move(ctx_impl), peer_spec, SocketAddress());
        } else {
            return OpenSslCryptoCodecImpl::make_server_codec(std::move(ctx_impl), SocketAddress());
        }
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec(
            const std::shared_ptr<TlsContext>& ctx, CryptoCodec::Mode mode) {
        return create_openssl_codec(ctx, mode, SocketSpec::invalid);
    }

    static EncodeResult do_encode(CryptoCodec& codec, Output& buffer, std::string_view plaintext) {
        auto out = buffer.reserve(codec.min_encode_buffer_size());
        auto enc_res = codec.encode(plaintext.data(), plaintext.size(), out.data, out.size);
        buffer.commit(enc_res.bytes_produced);
        return enc_res;
    }

    static DecodeResult do_decode(CryptoCodec& codec, Input& buffer, std::string& out,
                                  size_t max_bytes_produced, size_t max_bytes_consumed) {
        auto in = buffer.obtain();
        out.resize(max_bytes_produced);
        auto to_consume = std::min(in.size, max_bytes_consumed);
        auto enc_res = codec.decode(in.data, to_consume, &out[0], out.size());
        buffer.evict(enc_res.bytes_consumed);
        out.resize(enc_res.bytes_produced);
        return enc_res;
    }

    EncodeResult client_encode(std::string_view plaintext) {
        auto res = do_encode(*client, client_to_server, plaintext);
        print_encode_result("client", res);
        return res;
    }

    EncodeResult server_encode(std::string_view plaintext) {
        auto res = do_encode(*server, server_to_client, plaintext);
        print_encode_result("server", res);
        return res;
    }

    DecodeResult client_decode(std::string& out, size_t max_bytes_produced,
                               size_t max_bytes_consumed = UINT64_MAX) {
        auto res = do_decode(*client, server_to_client, out, max_bytes_produced, max_bytes_consumed);
        print_decode_result("client", res);
        return res;
    }

    DecodeResult server_decode(std::string& out, size_t max_bytes_produced,
                               size_t max_bytes_consumed = UINT64_MAX) {
        auto res = do_decode(*server, client_to_server, out, max_bytes_produced, max_bytes_consumed);
        print_decode_result("server", res);
        return res;
    }

    DecodeResult client_decode_ignore_plaintext_output() {
        std::string dummy_decoded;
        constexpr size_t dummy_max_decoded = 100;
        return client_decode(dummy_decoded, dummy_max_decoded);
    }

    DecodeResult server_decode_ignore_plaintext_output() {
        std::string dummy_decoded;
        constexpr size_t dummy_max_decoded = 100;
        return server_decode(dummy_decoded, dummy_max_decoded);
    }

    EncodeResult do_half_close(CryptoCodec& codec, Output& buffer) {
        auto out = buffer.reserve(codec.min_encode_buffer_size());
        auto enc_res = codec.half_close(out.data, out.size);
        buffer.commit(enc_res.bytes_produced);
        return enc_res;
    }

    EncodeResult client_half_close() {
        auto res = do_half_close(*client, client_to_server);
        print_encode_result("client", res);
        return res;
    }

    EncodeResult server_half_close() {
        auto res = do_half_close(*server, server_to_client);
        print_encode_result("server", res);
        return res;
    }

    HandshakeResult do_handshake(CryptoCodec& codec, Input& input, Output& output) {
        auto in = input.obtain();
        auto out = output.reserve(codec.min_encode_buffer_size());
        auto hs_result = codec.handshake(in.data, in.size, out.data, out.size);
        input.evict(hs_result.bytes_consumed);
        output.commit(hs_result.bytes_produced);
        return hs_result;
    }

    void do_handshake_work(CryptoCodec& codec) {
        codec.do_handshake_work();
    }

    bool handshake() {
        HandshakeResult cli_res;
        HandshakeResult serv_res;
        while (!(cli_res.done() && serv_res.done())) {
            while ((cli_res  = do_handshake(*client, server_to_client, client_to_server)).needs_work()) {
                fprintf(stderr, "doing client handshake work\n");
                do_handshake_work(*client);
            }
            print_handshake_result("client handshake()", cli_res);
            while ((serv_res = do_handshake(*server, client_to_server, server_to_client)).needs_work()) {
                fprintf(stderr, "doing server handshake work\n");
                do_handshake_work(*server);
            }
            print_handshake_result("server handshake()", serv_res);

            if (cli_res.failed() || serv_res.failed()) {
                return false;
            }
        }
        return true;
    }
};

Fixture::~Fixture() = default;

TEST(OpensslImplTest, client_and_server_can_complete_handshake) {
    Fixture f;
    fprintf(stderr, "Compiled with %s\n", OPENSSL_VERSION_TEXT);
    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, client_handshake_initially_returns_NeedsWork_without_producing_anything) {
    Fixture f;
    auto res = f.do_handshake(*f.client, f.server_to_client, f.client_to_server);
    EXPECT_TRUE(res.needs_work());
    EXPECT_EQ(0u, res.bytes_consumed);
    EXPECT_EQ(0u, res.bytes_produced);
}

TEST(OpensslImplTest, server_handshake_returns_NeedsPeerData_with_empty_input) {
    Fixture f;
    auto res = f.do_handshake(*f.server, f.client_to_server, f.server_to_client);
    EXPECT_EQ(static_cast<int>(HandshakeResult::State::NeedsMorePeerData),
                 static_cast<int>(res.state));
    EXPECT_EQ(0u, res.bytes_consumed);
    EXPECT_EQ(0u, res.bytes_produced);
}

TEST(OpensslImplTest, clients_and_servers_can_send_single_data_frame_after_handshake__not_full_duplex) {
    Fixture f;
    ASSERT_TRUE(f.handshake());

    std::string client_plaintext = "Hellooo world! :D";
    std::string server_plaintext = "Goodbye moon~ :3";

    ASSERT_FALSE(f.client_encode(client_plaintext).failed);
    std::string server_plaintext_out;
    ASSERT_TRUE(f.server_decode(server_plaintext_out, 256).frame_decoded_ok());
    EXPECT_EQ(client_plaintext, server_plaintext_out);

    ASSERT_FALSE(f.server_encode(server_plaintext).failed);
    std::string client_plaintext_out;
    ASSERT_TRUE(f.client_decode(client_plaintext_out, 256).frame_decoded_ok());
    EXPECT_EQ(server_plaintext, client_plaintext_out);
}

TEST(OpensslImplTest, clients_and_servers_can_send_single_data_frame_after_handshake__full_duplex) {
    Fixture f;
    ASSERT_TRUE(f.handshake());

    std::string client_plaintext = "Greetings globe! :D";
    std::string server_plaintext = "Sayonara luna~ :3";

    ASSERT_FALSE(f.client_encode(client_plaintext).failed);
    ASSERT_FALSE(f.server_encode(server_plaintext).failed);

    std::string client_plaintext_out;
    std::string server_plaintext_out;
    ASSERT_TRUE(f.server_decode(server_plaintext_out, 256).frame_decoded_ok());
    EXPECT_EQ(client_plaintext, server_plaintext_out);
    ASSERT_TRUE(f.client_decode(client_plaintext_out, 256).frame_decoded_ok());
    EXPECT_EQ(server_plaintext, client_plaintext_out);
}

TEST(OpensslImplTest, short_ciphertext_read_on_decode_returns_NeedsMorePeerData) {
    Fixture f;
    ASSERT_TRUE(f.handshake());

    std::string client_plaintext = "very secret foo";
    ASSERT_FALSE(f.client_encode(client_plaintext).failed);

    std::string server_plaintext_out;
    auto dec_res = f.server_decode(server_plaintext_out, 256, 10);
    EXPECT_FALSE(dec_res.failed()); // Short read is not a failure mode
    EXPECT_TRUE(dec_res.state == DecodeResult::State::NeedsMorePeerData);
}

TEST(OpensslImplTest, encodes_larger_than_max_frame_size_are_split_up) {
    Fixture f;
    ASSERT_TRUE(f.handshake());
    constexpr auto frame_size = impl::OpenSslCryptoCodecImpl::MaximumFramePlaintextSize;
    std::string client_plaintext(frame_size + 50, 'X');

    auto enc_res = f.client_encode(client_plaintext);
    ASSERT_FALSE(enc_res.failed);
    ASSERT_EQ(frame_size, enc_res.bytes_consumed);
    auto remainder = client_plaintext.substr(frame_size);

    enc_res = f.client_encode(remainder);
    ASSERT_FALSE(enc_res.failed);
    ASSERT_EQ(50u, enc_res.bytes_consumed);

    // Over on the server side, we expect to decode 2 matching frames
    std::string server_plaintext_out;
    auto dec_res = f.server_decode(server_plaintext_out, frame_size);
    ASSERT_TRUE(dec_res.frame_decoded_ok());
    EXPECT_EQ(frame_size, dec_res.bytes_produced);

    std::string remainder_out;
    dec_res = f.server_decode(remainder_out, frame_size);
    ASSERT_TRUE(dec_res.frame_decoded_ok());
    EXPECT_EQ(50u, dec_res.bytes_produced);

    EXPECT_EQ(client_plaintext, server_plaintext_out + remainder);
}

TEST(OpensslImplTest, client_without_a_certificate_is_rejected_by_server) {
    Fixture f;
    f.client = f.create_openssl_codec(f.create_options_without_own_peer_cert(), CryptoCodec::Mode::Client);
    EXPECT_FALSE(f.handshake());
}

void check_half_close_encoded_ok(const EncodeResult& close_res) {
    EXPECT_FALSE(close_res.failed);
    EXPECT_GT(close_res.bytes_produced, 0u);
    EXPECT_EQ(close_res.bytes_consumed, 0u);
}

void check_decode_peer_is_reported_closed(const DecodeResult& decoded) {
    EXPECT_TRUE(decoded.closed());
    EXPECT_GT(decoded.bytes_consumed, 0u);
    EXPECT_EQ(decoded.bytes_produced, 0u);
}

TEST(OpensslImplTest, both_peers_can_half_close_their_connections) {
    Fixture f;
    ASSERT_TRUE(f.handshake());
    auto close_res = f.client_half_close();
    check_half_close_encoded_ok(close_res);

    auto decoded = f.server_decode_ignore_plaintext_output();
    check_decode_peer_is_reported_closed(decoded);

    close_res = f.server_half_close();
    check_half_close_encoded_ok(close_res);

    decoded = f.client_decode_ignore_plaintext_output();
    check_decode_peer_is_reported_closed(decoded);
}

// Certificate note: public keys must be of the same type as those
// used by vespalib::test::make_tls_options_for_testing(). In this case,
// it's P-256 EC keys.
// Also note: the Subject of this CA is different from the baseline
// test CA to avoid validation errors. This also means the Issuer is
// different for the below host certificate.
constexpr const char* unknown_ca_pem = R"(-----BEGIN CERTIFICATE-----
MIIBvzCCAWYCCQDEtg8a8Y5bBzAKBggqhkjOPQQDAjBoMQswCQYDVQQGEwJVUzEU
MBIGA1UEBwwLTG9vbmV5VmlsbGUxDjAMBgNVBAoMBUFDTUUyMRcwFQYDVQQLDA5B
Q01FIHRlc3QgQ0EgMjEaMBgGA1UEAwwRYWNtZTIuZXhhbXBsZS5jb20wHhcNMTgw
OTI3MTM0NjA3WhcNNDYwMjEyMTM0NjA3WjBoMQswCQYDVQQGEwJVUzEUMBIGA1UE
BwwLTG9vbmV5VmlsbGUxDjAMBgNVBAoMBUFDTUUyMRcwFQYDVQQLDA5BQ01FIHRl
c3QgQ0EgMjEaMBgGA1UEAwwRYWNtZTIuZXhhbXBsZS5jb20wWTATBgcqhkjOPQIB
BggqhkjOPQMBBwNCAAT88ScwGmpJ4NycxZBaqgSpw+IXfeIFR1oCGpxlLaKyrdpw
Sl9SeuAyJfW4yNinzUeiuX+5hSrzly4yFrOIW/n6MAoGCCqGSM49BAMCA0cAMEQC
IGNCYvQyZm/7GgTCi55y3RWK0tEE73ivEut9V0qvlqarAiBj8IDxv+Dm0ZFlB/8E
EYn91JZORccsNSJkfIWqrGEjBA==
-----END CERTIFICATE-----)";

// Signed by unknown CA above
constexpr const char* untrusted_host_cert_pem = R"(-----BEGIN CERTIFICATE-----
MIIBrzCCAVYCCQDAZrFWZPw7djAKBggqhkjOPQQDAjBoMQswCQYDVQQGEwJVUzEU
MBIGA1UEBwwLTG9vbmV5VmlsbGUxDjAMBgNVBAoMBUFDTUUyMRcwFQYDVQQLDA5B
Q01FIHRlc3QgQ0EgMjEaMBgGA1UEAwwRYWNtZTIuZXhhbXBsZS5jb20wHhcNMTgw
OTI3MTM0NjA3WhcNNDYwMjEyMTM0NjA3WjBYMQswCQYDVQQGEwJVUzEUMBIGA1UE
BwwLTG9vbmV5VmlsbGUxGjAYBgNVBAoMEVJvYWQgUnVubmVyLCBJbmMuMRcwFQYD
VQQDDA5yci5leGFtcGxlLmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMrp
YgaA3CbDCaHa5CC6Vr7TLHEPNMkLNGnr2692a57ExWk1FMzNlZfaS79b67o6zxAu
/HMiEHtseecH96UaGg4wCgYIKoZIzj0EAwIDRwAwRAIgOjiCql8VIe0/Ihyymr0a
IforjEAMmPffLdHnMJzbya8CIBKWeTzSnG7/0PE0K73vqr+OrE5V31FjvzvYpvdT
tSe+
-----END CERTIFICATE-----)";

constexpr const char* untrusted_host_key_pem = R"(-----BEGIN EC PARAMETERS-----
BggqhkjOPQMBBw==
-----END EC PARAMETERS-----
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIHwh0Is5sf4emYv0UBsHSCCMI0XCV2RzhafIQ3j1BTK0oAoGCCqGSM49
AwEHoUQDQgAEyuliBoDcJsMJodrkILpWvtMscQ80yQs0aevbr3ZrnsTFaTUUzM2V
l9pLv1vrujrPEC78cyIQe2x55wf3pRoaDg==
-----END EC PRIVATE KEY-----)";

TEST(OpensslImplTest, client_with_certificate_signed_by_untrusted_CA_is_rejected_by_server) {
    Fixture f;
    auto client_opts = ts_from_pems(unknown_ca_pem, untrusted_host_cert_pem, untrusted_host_key_pem);
    f.client = f.create_openssl_codec(client_opts, CryptoCodec::Mode::Client);
    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, server_with_certificate_signed_by_untrusted_CA_is_rejected_by_client) {
    Fixture f;
    auto server_opts = ts_from_pems(unknown_ca_pem, untrusted_host_cert_pem, untrusted_host_key_pem);
    f.server = f.create_openssl_codec(server_opts, CryptoCodec::Mode::Server);
    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, can_specify_multiple_trusted_CA_certs_in_transport_options) {
    Fixture f;
    auto& base_opts = f.tls_opts;
    auto multi_ca_pem = base_opts.ca_certs_pem() + "\n" + unknown_ca_pem;
    auto multi_ca_using_ca_1 = ts_from_pems(multi_ca_pem, untrusted_host_cert_pem, untrusted_host_key_pem);
    auto multi_ca_using_ca_2 = ts_from_pems(multi_ca_pem, base_opts.cert_chain_pem(), base_opts.private_key_pem());
    // Let client be signed by CA 1, server by CA 2. Both have the two CAs in their trust store
    // so this should allow for a successful handshake.
    f.client = f.create_openssl_codec(multi_ca_using_ca_1, CryptoCodec::Mode::Client);
    f.server = f.create_openssl_codec(multi_ca_using_ca_2, CryptoCodec::Mode::Server);
    EXPECT_TRUE(f.handshake());
}

struct CertFixture : Fixture {
    CertKeyWrapper root_ca;

    static CertKeyWrapper make_root_ca() {
        auto dn = X509Certificate::DistinguishedName()
                .country("US").state("CA").locality("Sunnyvale")
                .organization("ACME, Inc.")
                .organizational_unit("ACME Root CA")
                .add_common_name("acme.example.com");
        auto subject = X509Certificate::SubjectInfo(std::move(dn));
        auto key = PrivateKey::generate_p256_ec_key();
        auto params = X509Certificate::Params::self_signed(std::move(subject), key);
        auto cert = X509Certificate::generate_from(std::move(params));
        return {std::move(cert), std::move(key)};
    }

    CertFixture()
        : Fixture(),
          root_ca(make_root_ca())
    {}
    ~CertFixture();

    static X509Certificate::SubjectInfo make_subject_info(const std::vector<std::string>& common_names,
                                                          const std::vector<std::string>& sans) {
        auto dn = X509Certificate::DistinguishedName()
                .country("US").state("CA").locality("Sunnyvale")
                .organization("Wile E. Coyote, Ltd.")
                .organizational_unit("Personal Rocketry Division");
        for (auto& cn : common_names) {
            dn.add_common_name(cn);
        }
        auto subject = X509Certificate::SubjectInfo(std::move(dn));
        for (auto& san : sans) {
            subject.add_subject_alt_name(san);
        }
        return subject;
    }

    CertKeyWrapper create_ca_issued_peer_cert(const std::vector<std::string>& common_names,
                                              const std::vector<std::string>& sans) const {
        auto subject = make_subject_info(common_names, sans);
        auto key = PrivateKey::generate_p256_ec_key();
        auto params = X509Certificate::Params::issued_by(std::move(subject), key, root_ca.cert, root_ca.key);
        auto cert = X509Certificate::generate_from(std::move(params));
        return {std::move(cert), std::move(key)};
    }

    CertKeyWrapper create_self_signed_peer_cert(const std::vector<std::string>& common_names,
                                                const std::vector<std::string>& sans) const {
        auto subject = make_subject_info(common_names, sans);
        auto key = PrivateKey::generate_p256_ec_key();
        auto params = X509Certificate::Params::self_signed(std::move(subject), key);
        auto cert = X509Certificate::generate_from(std::move(params));
        return {std::move(cert), std::move(key)};
    }

    static std::unique_ptr<OpenSslCryptoCodecImpl> create_openssl_codec_with_authz_mode(
            const TransportSecurityOptions& opts,
            std::shared_ptr<CertificateVerificationCallback> cert_verify_callback,
            CryptoCodec::Mode codec_mode,
            AuthorizationMode authz_mode) {
        auto ctx = TlsContext::create_default_context(opts, std::move(cert_verify_callback), authz_mode);
        return create_openssl_codec(ctx, codec_mode);
    }

    TransportSecurityOptions::Params ts_builder_from(const CertKeyWrapper& ck) const {
        return TransportSecurityOptions::Params().
                ca_certs_pem(root_ca.cert->to_pem()).
                cert_chain_pem(ck.cert->to_pem()).
                private_key_pem(ck.key->private_to_pem());
    }

    void reset_client_with_cert_opts(const CertKeyWrapper& ck, AuthorizedPeers authorized) {
        auto ts_params = ts_builder_from(ck).authorized_peers(std::move(authorized));
        client = create_openssl_codec(TransportSecurityOptions(std::move(ts_params)), CryptoCodec::Mode::Client);
    }

    void reset_client_with_cert_opts(const CertKeyWrapper& ck, std::shared_ptr<CertificateVerificationCallback> cert_cb) {
        auto ts_params = ts_builder_from(ck).authorized_peers(AuthorizedPeers::allow_all_authenticated());
        client = create_openssl_codec(TransportSecurityOptions(std::move(ts_params)),
                                      std::move(cert_cb), CryptoCodec::Mode::Client);
    }

    void reset_server_with_cert_opts(const CertKeyWrapper& ck, AuthorizedPeers authorized) {
        auto ts_params = ts_builder_from(ck).authorized_peers(std::move(authorized));
        server = create_openssl_codec(TransportSecurityOptions(std::move(ts_params)), CryptoCodec::Mode::Server);
    }

    void reset_server_with_cert_opts(const CertKeyWrapper& ck, std::shared_ptr<CertificateVerificationCallback> cert_cb) {
        auto ts_params = ts_builder_from(ck).authorized_peers(AuthorizedPeers::allow_all_authenticated());
        server = create_openssl_codec(TransportSecurityOptions(std::move(ts_params)),
                                      std::move(cert_cb), CryptoCodec::Mode::Server);
    }

    void reset_server_with_cert_opts(const CertKeyWrapper& ck,
                                     std::shared_ptr<CertificateVerificationCallback> cert_cb,
                                     AuthorizationMode authz_mode) {
        auto ts_params = ts_builder_from(ck).authorized_peers(AuthorizedPeers::allow_all_authenticated());
        server = create_openssl_codec_with_authz_mode(TransportSecurityOptions(std::move(ts_params)),
                                                      std::move(cert_cb), CryptoCodec::Mode::Server, authz_mode);
    }

    void reset_client_with_peer_spec(const CertKeyWrapper& ck,
                                     const SocketSpec& peer_spec,
                                     bool disable_hostname_validation = false)
    {
        auto ts_params = ts_builder_from(ck).
                authorized_peers(AuthorizedPeers::allow_all_authenticated()).
                disable_hostname_validation(disable_hostname_validation);
        client = create_openssl_codec(TransportSecurityOptions(std::move(ts_params)),
                                      CryptoCodec::Mode::Client, peer_spec);
    }
};

CertFixture::~CertFixture() = default;

struct PrintingCertificateCallback : CertificateVerificationCallback {
    VerificationResult verify(const PeerCredentials& peer_creds) const override {
        if (!peer_creds.common_name.empty())  {
            fprintf(stderr, "Got a CN: %s\n", peer_creds.common_name.c_str());
        }
        for (auto& dns : peer_creds.dns_sans) {
            fprintf(stderr, "Got a DNS SAN entry: %s\n", dns.c_str());
        }
        return VerificationResult::make_authorized_with_all_capabilities();
    }
};

// Single-use mock verifier
struct MockCertificateCallback : CertificateVerificationCallback {
    mutable PeerCredentials creds; // only used in single thread testing context
    VerificationResult verify(const PeerCredentials& peer_creds) const override {
        creds = peer_creds;
        return VerificationResult::make_authorized_with_all_capabilities();
    }
};

struct AlwaysFailVerifyCallback : CertificateVerificationCallback {
    VerificationResult verify([[maybe_unused]] const PeerCredentials& peer_creds) const override {
        fprintf(stderr, "Rejecting certificate, none shall pass!\n");
        return VerificationResult::make_not_authorized();
    }
};

struct ExceptionThrowingCallback : CertificateVerificationCallback {
    VerificationResult verify([[maybe_unused]] const PeerCredentials& peer_creds) const override {
        throw std::runtime_error("oh no what is going on");
    }
};

TEST(OpensslImplTest, certificate_verification_callback_returning_unauthorized_breaks_handshake) {
    CertFixture f;
    auto ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    f.reset_server_with_cert_opts(ck, std::make_shared<AlwaysFailVerifyCallback>());
    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, exception_during_verification_callback_processing_breaks_handshake) {
    CertFixture f;
    auto ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    f.reset_server_with_cert_opts(ck, std::make_shared<ExceptionThrowingCallback>());
    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, certificate_verification_callback_observes_CN_DNS_SANs_and_URI_SANs) {
    CertFixture f;
    auto ck = f.create_ca_issued_peer_cert(
            {{"rockets.wile.example.com"}},
            {{"DNS:crash.wile.example.com"}, {"DNS:burn.wile.example.com"},
             {"URI:foo://bar.baz/zoid"}});

    fprintf(stderr, "certs:\n%s%s\n", f.root_ca.cert->to_pem().c_str(), ck.cert->to_pem().c_str());

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    auto server_cb = std::make_shared<MockCertificateCallback>();
    f.reset_server_with_cert_opts(ck, server_cb);
    ASSERT_TRUE(f.handshake());

    auto& creds = server_cb->creds;
    EXPECT_EQ("rockets.wile.example.com", creds.common_name);
    ASSERT_EQ(2u, creds.dns_sans.size());
    EXPECT_EQ("crash.wile.example.com", creds.dns_sans[0]);
    EXPECT_EQ("burn.wile.example.com", creds.dns_sans[1]);
    ASSERT_EQ(1u, creds.uri_sans.size());
    EXPECT_EQ("foo://bar.baz/zoid", creds.uri_sans[0]);
}

TEST(OpensslImplTest, peer_credentials_are_propagated_to_CryptoCodec) {
    CertFixture f;
    auto cli_cert = f.create_ca_issued_peer_cert(
            {{"rockets.wile.example.com"}},
            {{"DNS:crash.wile.example.com"}, {"DNS:burn.wile.example.com"},
             {"URI:foo://bar.baz/zoid"}});
    auto serv_cert = f.create_ca_issued_peer_cert(
            {{"birdseed.roadrunner.example.com"}},
            {{"DNS:fake.tunnel.example.com"}});
    f.reset_client_with_cert_opts(cli_cert, std::make_shared<PrintingCertificateCallback>());
    auto server_cb = std::make_shared<MockCertificateCallback>();
    f.reset_server_with_cert_opts(serv_cert, server_cb);
    ASSERT_TRUE(f.handshake());

    auto& client_creds = f.server->peer_credentials();
    auto& server_creds = f.client->peer_credentials();

    fprintf(stderr, "Client credentials (observed by server): %s\n", client_creds.to_string().c_str());
    fprintf(stderr, "Server credentials (observed by client): %s\n", server_creds.to_string().c_str());

    EXPECT_EQ("rockets.wile.example.com", client_creds.common_name);
    ASSERT_EQ(2u, client_creds.dns_sans.size());
    EXPECT_EQ("crash.wile.example.com", client_creds.dns_sans[0]);
    EXPECT_EQ("burn.wile.example.com", client_creds.dns_sans[1]);
    ASSERT_EQ(1u, client_creds.uri_sans.size());
    EXPECT_EQ("foo://bar.baz/zoid", client_creds.uri_sans[0]);

    EXPECT_EQ("birdseed.roadrunner.example.com", server_creds.common_name);
    ASSERT_EQ(1u, server_creds.dns_sans.size());
    EXPECT_EQ("fake.tunnel.example.com", server_creds.dns_sans[0]);
    ASSERT_EQ(0u, server_creds.uri_sans.size());
}

TEST(OpensslImplTest, last_occurring_CN_is_given_to_verification_callback_if_multiple_CNs_are_present) {
    CertFixture f;
    auto ck = f.create_ca_issued_peer_cert(
            {{"foo.wile.example.com"}, {"bar.wile.example.com"}, {"baz.wile.example.com"}}, {});

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    auto server_cb = std::make_shared<MockCertificateCallback>();
    f.reset_server_with_cert_opts(ck, server_cb);
    ASSERT_TRUE(f.handshake());

    auto& creds = server_cb->creds;
    EXPECT_EQ("baz.wile.example.com", creds.common_name);
}

// TODO we are likely to want IPADDR SANs at some point
TEST(OpensslImplTest, only_DNS_and_URI_SANs_are_enumerated) {
    CertFixture f;
    auto ck = f.create_ca_issued_peer_cert({}, {"IP:127.0.0.1"});

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    auto server_cb = std::make_shared<MockCertificateCallback>();
    f.reset_server_with_cert_opts(ck, server_cb);
    ASSERT_TRUE(f.handshake());
    EXPECT_EQ(0u, server_cb->creds.dns_sans.size());
    EXPECT_EQ(0u, server_cb->creds.uri_sans.size());
}

// A server must only trust the actual verified peer certificate, not any other random
// certificate that the client decides to include in its certificate chain. See CVE-2023-2422.
// Note: this is a preemptive test; we are not--and have never been--vulnerable to this issue.
TEST(OpensslImplTest, certificate_credential_extraction_is_not_vulnerable_to_CVE_2023_2422) {
    CertFixture f;
    auto good_ck = f.create_ca_issued_peer_cert({}, {{"DNS:legit.example.com"}});
    auto evil_ck = f.create_self_signed_peer_cert({"rudolf.example.com"}, {{"DNS:blodstrupmoen.example.com"}});

    auto ts_params = TransportSecurityOptions::Params().
            ca_certs_pem(f.root_ca.cert->to_pem()).
            // Concatenate CA-signed good cert with self-signed cert with different credentials.
            // We should only ever look at the good cert.
            cert_chain_pem(good_ck.cert->to_pem() + evil_ck.cert->to_pem()).
            private_key_pem(good_ck.key->private_to_pem() + evil_ck.key->private_to_pem()).
            authorized_peers(AuthorizedPeers::allow_all_authenticated());

    f.client = f.create_openssl_codec(TransportSecurityOptions(std::move(ts_params)),
                                      std::make_shared<PrintingCertificateCallback>(),
                                      CryptoCodec::Mode::Client);
    auto server_cb = std::make_shared<MockCertificateCallback>();
    f.reset_server_with_cert_opts(good_ck, server_cb);
    ASSERT_TRUE(f.handshake());

    auto& creds = server_cb->creds;
    EXPECT_EQ("", creds.common_name);
    ASSERT_EQ(1u, creds.dns_sans.size());
    EXPECT_EQ("legit.example.com", creds.dns_sans[0]);
}

// We don't test too many combinations of peer policies here, only that
// the wiring is set up. Verification logic is tested elsewhere.

TEST(OpensslImplTest, client_rejects_server_with_certificate_that_DOES_NOT_match_peer_policy) {
    CertFixture f;
    auto client_ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});
    auto authorized = authorized_peers({policy_with({required_san_dns("crash.wile.example.com")})});
    f.reset_client_with_cert_opts(client_ck, std::move(authorized));
    // crash.wile.example.com not present in certificate
    auto server_ck = f.create_ca_issued_peer_cert(
            {}, {{"DNS:birdseed.wile.example.com"}, {"DNS:roadrunner.wile.example.com"}});
    f.reset_server_with_cert_opts(server_ck, AuthorizedPeers::allow_all_authenticated());

    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, client_allows_server_with_certificate_that_DOES_match_peer_policy) {
    CertFixture f;
    auto client_ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});
    auto authorized = authorized_peers({policy_with({required_san_dns("crash.wile.example.com")})});
    f.reset_client_with_cert_opts(client_ck, std::move(authorized));
    auto server_ck = f.create_ca_issued_peer_cert(
            {}, {{"DNS:birdseed.wile.example.com"}, {"DNS:crash.wile.example.com"}});
    f.reset_server_with_cert_opts(server_ck, AuthorizedPeers::allow_all_authenticated());

    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, server_rejects_client_with_certificate_that_DOES_NOT_match_peer_policy) {
    CertFixture f;
    auto server_ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});
    auto authorized = authorized_peers({policy_with({required_san_dns("crash.wile.example.com")})});
    f.reset_server_with_cert_opts(server_ck, std::move(authorized));
    // crash.wile.example.com not present in certificate
    auto client_ck = f.create_ca_issued_peer_cert(
            {}, {{"DNS:birdseed.wile.example.com"}, {"DNS:roadrunner.wile.example.com"}});
    f.reset_client_with_cert_opts(client_ck, AuthorizedPeers::allow_all_authenticated());

    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, server_allows_client_with_certificate_that_DOES_match_peer_policy) {
    CertFixture f;
    auto server_ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});
    auto authorized = authorized_peers({policy_with({required_san_dns("crash.wile.example.com")})});
    f.reset_server_with_cert_opts(server_ck, std::move(authorized));
    auto client_ck = f.create_ca_issued_peer_cert(
            {}, {{"DNS:birdseed.wile.example.com"}, {"DNS:crash.wile.example.com"}});
    f.reset_client_with_cert_opts(client_ck, AuthorizedPeers::allow_all_authenticated());

    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, uuthz_policy_derived_peer_capabilities_are_propagated_to_CryptoCodec) {
    CertFixture f;
    auto server_ck = f.create_ca_issued_peer_cert({}, {{"DNS:hello.world.example.com"}});
    auto authorized = authorized_peers({policy_with({required_san_dns("stale.memes.example.com")},
                                                    CapabilitySet::of({Capability::content_search_api(),
                                                                       Capability::content_status_pages()})),
                                        policy_with({required_san_dns("fresh.memes.example.com")},
                                                    CapabilitySet::make_with_all_capabilities())});
    f.reset_server_with_cert_opts(server_ck, std::move(authorized));
    auto client_ck = f.create_ca_issued_peer_cert({}, {{"DNS:stale.memes.example.com"}});
    f.reset_client_with_cert_opts(client_ck, AuthorizedPeers::allow_all_authenticated());

    ASSERT_TRUE(f.handshake());

    // Note: "inversion" of client <-> server is because the capabilities are that of the _peer_.
    auto client_caps = f.server->granted_capabilities();
    auto server_caps = f.client->granted_capabilities();
    // Server (from client's PoV) implicitly has all capabilities since client doesn't specify any policies
    EXPECT_EQ(server_caps, CapabilitySet::make_with_all_capabilities());
    // Client (from server's PoV) only has capabilities for the rule matching its DNS SAN entry
    EXPECT_EQ(client_caps, CapabilitySet::of({Capability::content_search_api(),
                                                 Capability::content_status_pages()}));
}

TEST(OpensslImplTest, handshake_is_allowed_if_at_least_one_policy_matches_even_if_resulting_capability_set_is_empty) {
    CertFixture f;
    auto server_ck = f.create_ca_issued_peer_cert({}, {{"DNS:hello.world.example.com"}});
    auto authorized = authorized_peers({policy_with({required_san_dns("stale.memes.example.com")},
                                                    CapabilitySet::make_empty()),
                                        policy_with({required_san_dns("fresh.memes.example.com")},
                                                    CapabilitySet::make_with_all_capabilities())});
    f.reset_server_with_cert_opts(server_ck, std::move(authorized));
    auto client_ck = f.create_ca_issued_peer_cert({}, {{"DNS:stale.memes.example.com"}});
    f.reset_client_with_cert_opts(client_ck, AuthorizedPeers::allow_all_authenticated());

    ASSERT_TRUE(f.handshake());

    // Note: "inversion" of client <-> server is because the capabilities are that of the _peer_.
    auto client_caps = f.server->granted_capabilities();
    auto server_caps = f.client->granted_capabilities();
    // Server (from client's PoV) implicitly has all capabilities since client doesn't specify any policies
    EXPECT_EQ(server_caps, CapabilitySet::make_with_all_capabilities());
    // Client (from server's PoV) only has capabilities for the rule matching its DNS SAN entry.
    // In this case, it is the empty set.
    EXPECT_EQ(client_caps, CapabilitySet::make_empty());
}

void reset_peers_with_server_authz_mode(CertFixture& f, AuthorizationMode authz_mode) {
    auto ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});

    f.reset_client_with_cert_opts(ck, std::make_shared<PrintingCertificateCallback>());
    f.reset_server_with_cert_opts(ck, std::make_shared<AlwaysFailVerifyCallback>(), authz_mode);
}

TEST(OpensslImplTest, log_only_insecure_authorization_mode_ignores_verification_result) {
    CertFixture f;
    reset_peers_with_server_authz_mode(f, AuthorizationMode::LogOnly);
    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, disabled_insecure_authorization_mode_ignores_verification_result) {
    CertFixture f;
    reset_peers_with_server_authz_mode(f, AuthorizationMode::Disable);
    EXPECT_TRUE(f.handshake());
}

void reset_peers_with_client_peer_spec(CertFixture& f,
                                       const SocketSpec& peer_spec,
                                       bool disable_hostname_validation = false)
{
    auto client_ck = f.create_ca_issued_peer_cert({"hello.world.example.com"}, {});
    f.reset_client_with_peer_spec(client_ck, peer_spec, disable_hostname_validation);
    // Since hostname validation is enabled by default, providing a peer spec also
    // means that we must have a valid server name to present back (or the handshake fails).
    auto server_ck = f.create_ca_issued_peer_cert({}, {{"DNS:*.example.com"}});
    f.reset_server_with_cert_opts(server_ck, AuthorizedPeers::allow_all_authenticated());
}

TEST(OpensslImplTest, client_does_not_send_SNI_extension_if_hostname_not_provided_in_spec) {
    CertFixture f;
    reset_peers_with_client_peer_spec(f, SocketSpec::invalid);

    ASSERT_TRUE(f.handshake());
    auto maybe_sni = f.server->client_provided_sni_extension();
    EXPECT_FALSE(maybe_sni.has_value());
}

TEST(OpensslImplTest, client_sends_SNI_extension_with_hostname_provided_in_spec) {
    CertFixture f;
    reset_peers_with_client_peer_spec(f, SocketSpec::from_host_port("sni-test.example.com", 12345));

    ASSERT_TRUE(f.handshake());
    auto maybe_sni = f.server->client_provided_sni_extension();
    ASSERT_TRUE(maybe_sni.has_value());
    EXPECT_EQ("sni-test.example.com", *maybe_sni);
}

TEST(OpensslImplTest, client_hostname_validation_passes_handshake_if_server_hostname_matches_certificate) {
    CertFixture f;
    reset_peers_with_client_peer_spec(f, SocketSpec::from_host_port("server-must-be-under.example.com", 12345), false);
    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, client_hostname_validation_fails_handshake_if_server_hostname_mismatches_certificate) {
    CertFixture f;
    // Wildcards only apply to a single level, so this should fail as the server only has a cert for *.example.com
    reset_peers_with_client_peer_spec(f, SocketSpec::from_host_port("nested.name.example.com", 12345), false);
    EXPECT_FALSE(f.handshake());
}

TEST(OpensslImplTest, mismatching_server_cert_vs_hostname_does_not_fail_if_hostname_validation_is_disabled) {
    CertFixture f;
    reset_peers_with_client_peer_spec(f, SocketSpec::from_host_port("a.very.nested.name.example.com", 12345), true);
    EXPECT_TRUE(f.handshake());
}

TEST(OpensslImplTest, failure_statistics_are_incremented_on_authorization_failures) {
    CertFixture f;
    reset_peers_with_server_authz_mode(f, AuthorizationMode::Enforce);
    auto server_before = ConnectionStatistics::get(true).snapshot();
    auto client_before = ConnectionStatistics::get(false).snapshot();
    EXPECT_FALSE(f.handshake());
    auto server_stats = ConnectionStatistics::get(true).snapshot().subtract(server_before);
    auto client_stats = ConnectionStatistics::get(false).snapshot().subtract(client_before);

    EXPECT_EQ(1u, server_stats.invalid_peer_credentials);
    EXPECT_EQ(0u, client_stats.invalid_peer_credentials);
    EXPECT_EQ(1u, server_stats.failed_tls_handshakes);
    EXPECT_EQ(0u, server_stats.tls_connections);
    // Client TLS connection count may be 0 (<= v1.2) or 1 (v1.3), since v1.3
    // completes its handshake earlier.
}

TEST(OpensslImplTest, success_statistics_are_incremented_on_OK_authorization) {
    CertFixture f;
    reset_peers_with_server_authz_mode(f, AuthorizationMode::Disable);
    auto server_before = ConnectionStatistics::get(true).snapshot();
    auto client_before = ConnectionStatistics::get(false).snapshot();
    EXPECT_TRUE(f.handshake());
    auto server_stats = ConnectionStatistics::get(true).snapshot().subtract(server_before);
    auto client_stats = ConnectionStatistics::get(false).snapshot().subtract(client_before);

    EXPECT_EQ(0u, server_stats.invalid_peer_credentials);
    EXPECT_EQ(0u, client_stats.invalid_peer_credentials);
    EXPECT_EQ(0u, server_stats.failed_tls_handshakes);
    EXPECT_EQ(0u, client_stats.failed_tls_handshakes);
    EXPECT_EQ(1u, server_stats.tls_connections);
    EXPECT_EQ(1u, client_stats.tls_connections);
}

// TODO we can't test embedded nulls since the OpenSSL v3 extension APIs
// take in null terminated strings as arguments... :I

/*
 * TODO tests:
 *  - handshakes with multi frame writes
 *  - completed handshake with pipelined data frame
 *  - short plaintext writes on decode (.. if we even want to support this..)
 *  - short ciphertext write on encode (.. if we even want to support this..)
 *  - detection of peer shutdown session
 */

GTEST_MAIN_RUN_ALL_TESTS()

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/test/nexus.h>
#include <vbench/test/all.h>
#include <vespa/vespalib/net/crypto_engine.h>
#include <vespa/vespalib/net/tls/tls_crypto_engine.h>
#include <vespa/vespalib/test/make_tls_options_for_testing.h>

using namespace vbench;

auto null_crypto = std::make_shared<vespalib::NullCryptoEngine>();
auto tls_crypto = std::make_shared<vespalib::TlsCryptoEngine>(vespalib::test::make_tls_options_for_testing());

using OutputWriter = vespalib::OutputWriter;
using vespalib::CryptoEngine;
using vespalib::test::Nexus;

const size_t numLines = 100;

struct Agent {
    Stream::UP socket;
    Agent(Stream::UP s) : socket(std::move(s)) {}
    void write(const char *prefix) {
        OutputWriter out(*socket, 32);
        for (size_t i = 0; i < numLines; ++i) {
            out.printf("%s%zu\n", prefix, i);
        }
        out.write("\n");
    }
    void read(const char *prefix) {
        LineReader reader(*socket);
        for (size_t lines = 0; true; ++lines) {
            string line;
            reader.readLine(line);
            if (line.empty()) {
                EXPECT_EQ(numLines, lines);
                break;
            }
            EXPECT_EQ(strfmt("%s%zu", prefix, lines), line);
        }
    }
};

void verify_socket(CryptoEngine &crypto, ServerSocket &server_socket, Nexus &ctx) {
    if (ctx.thread_id() == 0) { // client
        Agent client(std::make_unique<Socket>(crypto, "localhost", server_socket.port()));
        client.write("client-");
        client.read("server-");
        ctx.barrier();   // #1
        LineReader reader(*client.socket);
        string line;
        EXPECT_FALSE(reader.readLine(line));
        EXPECT_TRUE(line.empty());
        EXPECT_TRUE(client.socket->eof());
        EXPECT_FALSE(client.socket->tainted());
    } else {              // server
        Agent server(server_socket.accept(crypto));
        server.read("client-");
        server.write("server-");
        ctx.barrier();   // #1
    }
}

TEST(SocketTest, socket) {
    size_t num_threads = 2;
    ServerSocket f1;
    auto task = [&](Nexus &ctx){
                    SCOPED_TRACE("null crypto");
                    verify_socket(*null_crypto, f1, ctx);
                };
    Nexus::run(num_threads, task);
}

TEST(SocketTest, secure_socket) {
    size_t num_threads = 2;
    ServerSocket f1;
    auto task = [&](Nexus &ctx){
                    SCOPED_TRACE("tls crypto");
                    verify_socket(*tls_crypto, f1, ctx);
                };
    Nexus::run(num_threads, task);
}

GTEST_MAIN_RUN_ALL_TESTS()

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#include <vespa/vespalib/net/socket_spec.h>
#include <vespa/vespalib/util/host_name.h>
#include <thread>
#include <functional>

using namespace vespalib;

std::string read_msg(SocketHandle &socket) {
    std::string msg;
    for (;;) {
        char c;
        ssize_t ret = socket.read(&c, 1);
        if (ret != 1) {
            fprintf(stderr, "error during read message\n");
            return msg;
        }
        if (c == '\n') {
            return msg;
        }
        msg += c;
    }
}

void
write_msg(SocketHandle &socket, const std::string &msg) {
    for (const char & c : msg) {
        ssize_t ret = socket.write(&c, 1);
        if (ret != 1) {
            fprintf(stderr, "error during write message\n");
            return;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        return 1;
    }
    std::string host(argv[1]);
    int port = atoi(argv[2]);
    fprintf(stderr, "running socket test client at host %s\n", HostName::get().c_str());
    fprintf(stderr, "trying to connect to host %s at port %d\n", host.c_str(), port);
    auto list = SocketAddress::resolve(port, host.c_str());
    if (!list.empty()) {
        fprintf(stderr, "all remote addresses:\n");
        for (const auto &addr: list) {
            fprintf(stderr, "  %s\n", addr.spec().c_str());
        }
    }
    SocketHandle socket = SocketSpec::from_host_port(host, port).client_address().connect();
    if (!socket.valid()) {
        fprintf(stderr, "connect failed\n");
        return 1;
    }
    fprintf(stderr, "connected to: %s (local address: %s)\n",
            SocketAddress::peer_address(socket.get()).spec().c_str(),
            SocketAddress::address_of(socket.get()).spec().c_str());
    write_msg(socket, "hello from client\n");
    fprintf(stderr, "message from server: '%s'\n", read_msg(socket).c_str());
    return 0;
}

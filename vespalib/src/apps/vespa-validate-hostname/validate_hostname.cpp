// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <cstdio>
#include <vespa/vespalib/net/socket_address.h>
#include <set>
#include <string>

using vespalib::SocketAddress;

std::set<std::string> make_ip_set() {
    std::set<std::string> result;
    for (const auto &addr: SocketAddress::get_interfaces()) {
        result.insert(addr.ip_address());
    }
    return result;
}

std::string normalize(const std::string &hostname) {
    std::string canon_name = SocketAddress::normalize(hostname);
    if (canon_name != hostname) {
        fprintf(stderr, "warning: hostname validation: '%s' is not same as canonical hostname '%s'\n",
                hostname.c_str(), canon_name.c_str());
    }
    return canon_name;
}

int usage(const char *self) {
    fprintf(stderr, "usage: %s <hostname>\n", self);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return usage(argv[0]);
    }
    bool valid = true;
    auto my_ip_set = make_ip_set();
    std::string hostname = normalize(argv[1]);
    auto addr_list = SocketAddress::resolve(80, hostname.c_str());
    if (addr_list.empty()) {
        valid = false;
        fprintf(stderr, "FATAL: hostname validation failed: '%s' could not be resolved\n",
                hostname.c_str());
    }
    for (const SocketAddress &addr: addr_list) {
        std::string ip_addr = addr.ip_address();
        if (my_ip_set.count(ip_addr) == 0) {
            valid = false;
            fprintf(stderr, "FATAL: hostname validation failed: '%s' resolves to ip address not owned by this host (%s)\n",
                    hostname.c_str(), ip_addr.c_str());
        }
    }
    return valid ? 0 : 1;
}

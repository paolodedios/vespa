// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <string>
#include <vector>
#include <iosfwd>

namespace vespalib::net::tls {

// Simple wrapper of the information most useful to certificate verification code.
struct PeerCredentials {
    // The last occurring (i.e. "most specific") CN present in the certificate,
    // or the empty string if no CN is given (or if the CN is curiously empty).
    std::string common_name;
    // 0-n DNS SAN entries. Note: "DNS:" prefix is not present in strings.
    std::vector<std::string> dns_sans;
    // 0-n DNS URI entries. Note: "URI:" prefix is not present in strings.
    std::vector<std::string> uri_sans;

    PeerCredentials();
    PeerCredentials(const PeerCredentials&);
    PeerCredentials& operator=(const PeerCredentials&);
    PeerCredentials(PeerCredentials&&) noexcept;
    PeerCredentials& operator=(PeerCredentials&&) noexcept;
    ~PeerCredentials();

    std::string to_string() const;
};

std::ostream& operator<<(std::ostream&, const PeerCredentials&);

}

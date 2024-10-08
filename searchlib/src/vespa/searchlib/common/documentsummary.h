// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <cstdint>
#include <string>

namespace search::docsummary {

class DocumentSummary
{
public:
    static bool readDocIdLimit(const std::string &dir, uint32_t &docIdLimit);
    static bool writeDocIdLimit(const std::string &dir, uint32_t docIdLimit);
};

}


// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <memory>
#include <string>

namespace proton {

/**
 * Interface used to inspect which fields are present in a document type.
 */
struct IDocumentTypeInspector
{
    using SP = std::shared_ptr<IDocumentTypeInspector>;

    virtual ~IDocumentTypeInspector() =default;

    virtual bool hasUnchangedField(const std::string &name) const = 0;
};

} // namespace proton


// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <vespa/document/select/node.h>

namespace document {

namespace select { class VariableMap; }

class IDocumentTypeRepo;

class DocumentCalculator {
public:
    DocumentCalculator(const IDocumentTypeRepo& repo, const std::string& expression);
    ~DocumentCalculator();
    double evaluate(const Document& doc, std::unique_ptr<select::VariableMap> variables);

private:
    std::unique_ptr<select::Node> _selectionNode;
};

}


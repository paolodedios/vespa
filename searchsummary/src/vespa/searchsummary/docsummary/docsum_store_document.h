// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "i_docsum_store_document.h"

namespace document { class Document; }

namespace search::docsummary {

/**
 * Class providing access to a document retrieved from an IDocsumStore.
 **/
class DocsumStoreDocument : public IDocsumStoreDocument
{
    std::unique_ptr<document::Document> _document;
public:
    explicit DocsumStoreDocument(std::unique_ptr<document::Document> document);
    ~DocsumStoreDocument() override;
    DocsumStoreFieldValue get_field_value(const std::string& field_name) const override;
    void insert_summary_field(const std::string& field_name, ElementIds selected_elements, vespalib::slime::Inserter& inserter, IStringFieldConverter* converter) const override;
    void insert_juniper_field(const std::string& field_name, ElementIds selected_elements, vespalib::slime::Inserter& inserter, IJuniperConverter& converter) const override;
    void insert_document_id(vespalib::slime::Inserter& inserter) const override;
};

}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "docsum_field_writer_factory.h"
#include "fieldsearchspec.h"
#include "tokens_dfw.h"
#include <vespa/searchlib/common/matching_elements_fields.h>
#include <vespa/searchsummary/docsummary/copy_dfw.h>
#include <vespa/searchsummary/docsummary/docsum_field_writer.h>
#include <vespa/searchsummary/docsummary/docsum_field_writer_commands.h>
#include <vespa/searchsummary/docsummary/empty_dfw.h>
#include <vespa/searchsummary/docsummary/summary_elements_selector.h>
#include <vespa/vsm/config/config-vsmfields.h>
#include <algorithm>

using search::MatchingElementsFields;
using search::Normalizing;
using search::docsummary::CopyDFW;
using search::docsummary::DocsumFieldWriter;
using search::docsummary::EmptyDFW;
using search::docsummary::IDocsumEnvironment;
using search::docsummary::IQueryTermFilterFactory;
using search::docsummary::SummaryElementsSelector;
using vespa::config::search::vsm::VsmfieldsConfig;

namespace vsm {

namespace {

bool is_exact_match(std::string_view arg1) {
    return ((arg1 == "exact") || (arg1 == "word"));
}

std::unique_ptr<DocsumFieldWriter>
make_tokens_dfw(const std::string& source, VsmfieldsConfig& fields_config)
{
    bool exact_match = false;
    Normalizing normalize_mode = Normalizing::LOWERCASE;
    auto it = std::find_if(fields_config.fieldspec.begin(), fields_config.fieldspec.end(), [&source](auto& fs) { return source == fs.name; });
    if (it != fields_config.fieldspec.end()) {
        exact_match = is_exact_match(it->arg1);
        normalize_mode = FieldSearchSpecMap::convert_normalize_mode(it->normalize);
    }
    return std::make_unique<TokensDFW>(source, exact_match, normalize_mode);
}

}

DocsumFieldWriterFactory::DocsumFieldWriterFactory(bool use_v8_geo_positions, const IDocsumEnvironment& env, const IQueryTermFilterFactory& query_term_filter_factory, const vespa::config::search::vsm::VsmfieldsConfig& vsm_fields_config)
    : search::docsummary::DocsumFieldWriterFactory(use_v8_geo_positions, env, query_term_filter_factory),
      _vsm_fields_config(vsm_fields_config)
{
}

DocsumFieldWriterFactory::~DocsumFieldWriterFactory() = default;

std::unique_ptr<DocsumFieldWriter>
DocsumFieldWriterFactory::create_docsum_field_writer(const std::string& field_name,
                                                     const std::string& command,
                                                     const std::string& source)
{
    std::unique_ptr<DocsumFieldWriter> fieldWriter;
    using namespace search::docsummary;
    if ((command == command::positions) ||
        (command == command::abs_distance))
    {
        fieldWriter = std::make_unique<EmptyDFW>();
    } else if ((command == command::attribute) ||
               (command == command::attribute_combiner)) {
        if (!source.empty() && source != field_name) {
            fieldWriter = std::make_unique<CopyDFW>(source);
        }
    } else if (command == command::geo_position) {
    } else if ((command == command::tokens) ||
               (command == command::attribute_tokens)) {
        if (!source.empty()) {
            fieldWriter = make_tokens_dfw(source, _vsm_fields_config);
        } else {
            throw_missing_source(command);
        }
    } else {
        return search::docsummary::DocsumFieldWriterFactory::create_docsum_field_writer(field_name, command, source);
    }
    return fieldWriter;
}

}

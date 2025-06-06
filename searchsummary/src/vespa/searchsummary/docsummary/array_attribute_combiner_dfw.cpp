// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "array_attribute_combiner_dfw.h"
#include "attribute_field_writer.h"
#include "docsum_field_writer_state.h"
#include "docsumstate.h"
#include "struct_fields_resolver.h"
#include "summary_elements_selector.h"
#include <vespa/searchcommon/attribute/iattributecontext.h>
#include <vespa/vespalib/data/slime/cursor.h>
#include <vespa/vespalib/data/slime/inserter.h>
#include <vespa/vespalib/util/stash.h>
#include <algorithm>
#include <cassert>

using search::attribute::IAttributeContext;
using search::attribute::IAttributeVector;
using vespalib::slime::Cursor;

namespace search::docsummary {

namespace {

class ArrayAttributeFieldWriterState : public DocsumFieldWriterState
{
    // AttributeFieldWriter instances are owned by stash passed to constructor
    std::vector<AttributeFieldWriter*>                 _writers;

public:
    ArrayAttributeFieldWriterState(const std::vector<std::string> &fieldNames,
                                   const std::vector<std::string> &attributeNames,
                                   IAttributeContext &context,
                                   vespalib::Stash& stash,
                                   bool is_map_of_scalar);
    ~ArrayAttributeFieldWriterState() override;
    void insert_element(uint32_t element_index, Cursor &array);
    void insertField(uint32_t docId, ElementIds selected_elements, vespalib::slime::Inserter &target) override;
};

ArrayAttributeFieldWriterState::ArrayAttributeFieldWriterState(const std::vector<std::string> &fieldNames,
                                                               const std::vector<std::string> &attributeNames,
                                                               IAttributeContext &context,
                                                               vespalib::Stash& stash,
                                                               bool is_map_of_scalar)
    : DocsumFieldWriterState(),
      _writers()
{
    size_t fields = fieldNames.size();
    _writers.reserve(fields);
    for (uint32_t field = 0; field < fields; ++field) {
        const IAttributeVector *attr = context.getAttribute(attributeNames[field]);
        if (attr != nullptr) {
            _writers.emplace_back(&AttributeFieldWriter::create(fieldNames[field], *attr, stash, is_map_of_scalar));
        }
    }
}

ArrayAttributeFieldWriterState::~ArrayAttributeFieldWriterState() = default;

void
ArrayAttributeFieldWriterState::insert_element(uint32_t element_index, Cursor &array)
{
    Cursor &obj = array.addObject();
    for (auto &writer : _writers) {
        writer->print(element_index, obj);
    }
}

void
ArrayAttributeFieldWriterState::insertField(uint32_t docId, ElementIds selected_elements, vespalib::slime::Inserter &target)
{
    uint32_t elems = 0;
    for (auto &writer : _writers) {
        elems = std::max(elems, writer->fetch(docId));
    }
    if (elems == 0) {
        return;
    }

    if (!selected_elements.all_elements()) {
        if (selected_elements.empty() || selected_elements.back() >= elems) {
            return;
        }
        Cursor &arr = target.insertArray();
        for (auto idx : selected_elements) {
            if (idx < elems) {
                insert_element(idx, arr);
            }
        }
    } else {
        Cursor &arr = target.insertArray();
        for (uint32_t idx = 0; idx < elems; ++idx) {
            insert_element(idx, arr);
        }
    }
}

}

ArrayAttributeCombinerDFW::ArrayAttributeCombinerDFW(const StructFieldsResolver& fields_resolver)
    : AttributeCombinerDFW(),
      _fields(fields_resolver.get_array_fields()),
      _attributeNames(fields_resolver.get_array_attributes()),
      _is_map_of_scalar(fields_resolver.is_map_of_scalar())
{
}

ArrayAttributeCombinerDFW::~ArrayAttributeCombinerDFW() = default;

DocsumFieldWriterState*
ArrayAttributeCombinerDFW::allocFieldWriterState(IAttributeContext &context, vespalib::Stash& stash) const
{
    return &stash.create<ArrayAttributeFieldWriterState>(_fields, _attributeNames, context, stash, _is_map_of_scalar);
}

}

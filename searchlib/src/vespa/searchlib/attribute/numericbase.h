// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "attributevector.h"
#include "i_enum_store.h"
#include "loadedenumvalue.h"

namespace search {

class ReaderBase;

class NumericAttribute : public AttributeVector
{
protected:
    using EnumIndex = IEnumStore::Index;
    using EnumVector = IEnumStore::EnumVector;

    NumericAttribute(const std::string & name, const AttributeVector::Config & cfg)
        : AttributeVector(name, cfg)
    { }

    virtual void load_enumerated_data(ReaderBase& attrReader, enumstore::EnumeratedPostingsLoader& loader, size_t num_values);
    virtual void load_enumerated_data(ReaderBase& attrReader, enumstore::EnumeratedLoader& loader);
    virtual void load_posting_lists_and_update_enum_store(enumstore::EnumeratedPostingsLoader& loader);
    bool onAddDoc(DocId) override { return true; }
};

} // namespace search

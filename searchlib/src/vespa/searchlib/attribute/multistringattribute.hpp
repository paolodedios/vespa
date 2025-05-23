// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "multistringattribute.h"
#include "enumattribute.hpp"
#include "enumerated_multi_value_read_view.h"
#include "multi_string_enum_hint_search_context.h"
#include "string_sort_blob_writer.h"
#include <vespa/searchcommon/attribute/config.h>
#include <vespa/searchcommon/attribute/i_sort_blob_writer.h>
#include <vespa/searchlib/query/query_term_ucs4.h>
#include <vespa/searchlib/util/bufferwriter.h>
#include <vespa/vespalib/text/lowercase.h>
#include <vespa/vespalib/text/utf8.h>
#include <vespa/vespalib/util/regexp.h>
#include <vespa/vespalib/util/stash.h>

namespace search {

//-----------------------------------------------------------------------------
// MultiValueStringAttributeT public
//-----------------------------------------------------------------------------
template <typename B, typename M>
MultiValueStringAttributeT<B, M>::
MultiValueStringAttributeT(const std::string &name,
                           const AttributeVector::Config &c)
    : MultiValueEnumAttribute<B, M>(name, c)
{ }

template <typename B, typename M>
MultiValueStringAttributeT<B, M>::MultiValueStringAttributeT(const std::string &name)
    : MultiValueStringAttributeT<B, M>(name, AttributeVector::Config(AttributeVector::BasicType::STRING,  attribute::CollectionType::ARRAY))
{ }

template <typename B, typename M>
MultiValueStringAttributeT<B, M>::~MultiValueStringAttributeT() = default;


template <typename B, typename M>
void
MultiValueStringAttributeT<B, M>::freezeEnumDictionary()
{
    this->getEnumStore().freeze_dictionary();
}

template <typename B, typename M>
std::unique_ptr<attribute::SearchContext>
MultiValueStringAttributeT<B, M>::getSearch(QueryTermSimpleUP qTerm,
                                            const attribute::SearchContextParams &params) const
{
    bool cased = this->get_match_is_cased();
    auto doc_id_limit = this->getCommittedDocIdLimit();
    return std::make_unique<attribute::MultiStringEnumHintSearchContext<M>>(std::move(qTerm), cased, params.fuzzy_matching_algorithm(),
            *this, this->_mvMapping.make_read_view(doc_id_limit), this->_enumStore, doc_id_limit, this->getStatus().getNumValues());
}

template <typename B, typename M>
const attribute::IArrayReadView<const char*>*
MultiValueStringAttributeT<B, M>::make_read_view(attribute::IMultiValueAttribute::ArrayTag<const char*>, vespalib::Stash& stash) const
{
    return &stash.create<attribute::EnumeratedMultiValueReadView<const char*, M>>(this->_mvMapping.make_read_view(this->getCommittedDocIdLimit()), this->_enumStore);
}

template <typename B, typename M>
const attribute::IWeightedSetReadView<const char*>*
MultiValueStringAttributeT<B, M>::make_read_view(attribute::IMultiValueAttribute::WeightedSetTag<const char*>, vespalib::Stash& stash) const
{
    return &stash.create<attribute::EnumeratedMultiValueReadView<multivalue::WeightedValue<const char*>, M>>(this->_mvMapping.make_read_view(this->getCommittedDocIdLimit()), this->_enumStore);
}

template <typename MultiValueMappingT, typename EnumStoreT, bool asc>
class MultiStringSortBlobWriter : public attribute::ISortBlobWriter {
private:
    const MultiValueMappingT& _mv_mapping;
    const EnumStoreT& _enum_store;
    attribute::StringSortBlobWriter<asc> _writer;
public:
    MultiStringSortBlobWriter(const MultiValueMappingT &mv_mapping, const EnumStoreT &enum_store,
                              const common::BlobConverter *converter, search::common::sortspec::MissingPolicy policy,
                              std::string_view missing_value)
        : _mv_mapping(mv_mapping), _enum_store(enum_store), _writer(converter, policy, missing_value, true)
    {}
    long write(uint32_t docid, void* buf, long available) override {
        _writer.reset(buf, available);
        auto indices = _mv_mapping.get(docid);
        for (auto& v : indices) {
            if (!_writer.candidate(_enum_store.get_value(multivalue::get_value_ref(v).load_acquire()))) {
                return -1;
            }
        }
        return _writer.write();
    }
};

template <typename B, typename M>
std::unique_ptr<attribute::ISortBlobWriter>
MultiValueStringAttributeT<B, M>::make_sort_blob_writer(bool ascending, const common::BlobConverter* converter,
                                                        search::common::sortspec::MissingPolicy policy,
                                                        std::string_view missing_value) const
{
    if (ascending) {
        using SBW = MultiStringSortBlobWriter<MultiValueMapping, EnumStore, true>;
        return std::make_unique<SBW>(this->_mvMapping, this->_enumStore, converter, policy, missing_value);
    } else {
        using SBW = MultiStringSortBlobWriter<MultiValueMapping, EnumStore, false>;
        return std::make_unique<SBW>(this->_mvMapping, this->_enumStore, converter, policy, missing_value);
    }
}

} // namespace search


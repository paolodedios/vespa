// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include <vespa/searchcorespi/index/idiskindex.h>
#include <vespa/searchlib/common/tunefileinfo.h>
#include <vespa/searchlib/diskindex/diskindex.h>
#include <vespa/searchlib/queryeval/blueprint.h>

namespace proton {

class DiskIndexWrapper : public searchcorespi::index::IDiskIndex {
private:
    search::diskindex::DiskIndex _index;
    search::SerialNum _serialNum;

public:
    DiskIndexWrapper(const std::string &indexDir,
                     const search::TuneFileSearch &tuneFileSearch,
                     std::shared_ptr<search::diskindex::IPostingListCache> posting_list_cache);

    DiskIndexWrapper(const DiskIndexWrapper &oldIndex,
                     const search::TuneFileSearch &tuneFileSearch);

    std::unique_ptr<search::queryeval::Blueprint>
    createBlueprint(const IRequestContext & requestContext, const FieldSpec &field, const Node &term) override {
        return _index.createBlueprint(requestContext, field, term);
    }
    std::unique_ptr<search::queryeval::Blueprint>
    createBlueprint(const IRequestContext & requestContext, const FieldSpecList &fields, const Node &term) override {
        return _index.createBlueprint(requestContext, fields, term);
    }
    search::IndexStats get_index_stats(bool clear_disk_io_stats) const override {
        return _index.get_stats(clear_disk_io_stats);
    }

    search::SerialNum getSerialNum() const override;

    void accept(searchcorespi::IndexSearchableVisitor &visitor) const override;
    search::index::FieldLengthInfo get_field_length_info(const std::string& field_name) const override;
    const std::string &getIndexDir() const override { return _index.getIndexDir(); }
    const search::index::Schema &getSchema() const override { return _index.getSchema(); }
};

}  // namespace proton


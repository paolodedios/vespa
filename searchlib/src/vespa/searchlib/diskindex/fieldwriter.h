// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
#pragma once

#include "bitvectorfile.h"
#include <vespa/searchlib/index/dictionaryfile.h>
#include <vespa/searchlib/index/postinglistfile.h>
#include <vespa/searchlib/bitcompression/posocccompression.h>
#include <vespa/searchlib/bitcompression/countcompression.h>
#include <cassert>

namespace search::index { class Schema; }

namespace search::diskindex {

/**
 * FieldWriter is used to write a dictionary and posting list file together.
 *
 * It is used by the fusion code to write the merged output for a field,
 * and by the memory index dump code to write a field to disk.
 */
class FieldWriter {
public:
    using DocIdAndFeatures = index::DocIdAndFeatures;
    using Schema = index::Schema;
    using PostingListParams = index::PostingListParams;

    FieldWriter(const FieldWriter &rhs) = delete;
    FieldWriter(const FieldWriter &&rhs) = delete;
    FieldWriter &operator=(const FieldWriter &rhs) = delete;
    FieldWriter &operator=(const FieldWriter &&rhs) = delete;
    FieldWriter(uint32_t docIdLimit, uint64_t numWordIds, std::string_view prefix);
    ~FieldWriter();

    void newWord(uint64_t wordNum, std::string_view word);
    void newWord(std::string_view word);

    void add(const DocIdAndFeatures &features) {
        assert(features.doc_id() < _docIdLimit);
        assert(features.doc_id() > _prevDocId);
        _posoccfile->writeDocIdAndFeatures(features);
        _bvc.add(features.doc_id());
        _prevDocId = features.doc_id();
    }

    uint64_t getSparseWordNum() const { return _wordNum; }

    bool open(uint32_t minSkipDocs, uint32_t minChunkDocs, uint64_t features_size_flush_bits,
              bool dynamicKPosOccFormat,
              bool encode_interleaved_features,
              const Schema &schema, uint32_t indexId,
              const index::FieldLengthInfo &field_length_info,
              const TuneFileSeqWrite &tuneFileWrite,
              const search::common::FileHeaderContext &fileHeaderContext);

    bool close();

    void getFeatureParams(PostingListParams &params);
    static void remove(const std::string &prefix);
private:
    using DictionaryFileSeqWrite = index::DictionaryFileSeqWrite;
    using PostingListFileSeqWrite = index::PostingListFileSeqWrite;
    using PostingListCounts = index::PostingListCounts;
    std::unique_ptr<DictionaryFileSeqWrite>  _dictFile;
    std::unique_ptr<PostingListFileSeqWrite> _posoccfile;
    BitVectorCandidate      _bvc;
    BitVectorFileWrite      _bmapfile;
    const std::string  _prefix;
    std::string        _word;
    const uint64_t          _numWordIds;
    uint64_t                _compactWordNum;
    uint64_t                _wordNum;
    uint32_t                _prevDocId;
    const uint32_t          _docIdLimit;
    void flush();
    static uint64_t noWordNum() { return 0u; }
};

}

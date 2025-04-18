// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "countcompression.h"
#include <cassert>
#include <string>

namespace search::bitcompression {

class PageDict4StartOffset
{
public:
    uint64_t _fileOffset;
    uint64_t _accNumDocs;

    PageDict4StartOffset()
        : _fileOffset(0u),
          _accNumDocs(0u)
    {
    }

    PageDict4StartOffset(uint64_t fileOffset, uint64_t accNumDocs)
        : _fileOffset(fileOffset),
          _accNumDocs(accNumDocs)
    {
    }

    bool
    operator>=(const PageDict4StartOffset &rhs) const
    {
        if (_fileOffset >= rhs._fileOffset) {
            assert(_accNumDocs >= rhs._accNumDocs);
            return true;
        }
        assert(_accNumDocs < rhs._accNumDocs);
        return false;
    }

    bool
    operator>(const PageDict4StartOffset &rhs) const
    {
        if (_fileOffset > rhs._fileOffset) {
            assert(_accNumDocs > rhs._accNumDocs);
            return true;
        }
        assert(_accNumDocs <= rhs._accNumDocs);
        return false;
    }

    bool
    operator==(const PageDict4StartOffset &rhs) const
    {
        if (_fileOffset == rhs._fileOffset) {
            assert(_accNumDocs == rhs._accNumDocs);
            return true;
        }
        assert(_accNumDocs != rhs._accNumDocs);
        if (_fileOffset < rhs._fileOffset) {
            assert(_accNumDocs < rhs._accNumDocs);
        } else {
            assert(_accNumDocs > rhs._accNumDocs);
        }
        return false;
    }

    void
    adjust(const index::PostingListCounts &counts)
    {
        _fileOffset += counts._bitLength;
        _accNumDocs += counts._numDocs;
    }
};

std::ostream &
operator<<(std::ostream &stream, const index::PostingListCounts &counts);

class PageDict4PageParams
{
public:
    using Counts = index::PostingListCounts;
    using StartOffset = PageDict4StartOffset;

    static uint32_t getPageByteSize()      { return 4096; }
    static uint32_t getPageBitSize()       { return getPageByteSize() * 8; }
    static uint32_t getPageHeaderBitSize() { return 15u + 15u + 15u + 12u; }
    static uint32_t getMaxFileHeaderPad()  { return 999u; }
    static uint32_t getFileHeaderPad(uint32_t offset);
    static uint32_t getL1SkipStride()      { return 16; }
    static uint32_t getL2SkipStride()      { return 8; }
    static uint32_t getL4SkipStride()      { return 16; }
    static uint32_t getL5SkipStride()      { return 8; }
    static uint32_t getL7SkipStride()      { return 8; }
    static uint32_t noL7Ref() { return std::numeric_limits<uint32_t>::max(); }
    static uint32_t getL1Entries(uint32_t countsEntries) { return (countsEntries - 1) / getL1SkipStride(); }
    static uint32_t getL2Entries(uint32_t l1Entries) { return l1Entries / getL2SkipStride(); }
    static uint32_t getL4Entries(uint32_t l3Entries) { return (l3Entries - 1) / getL4SkipStride(); }
    static uint32_t getL5Entries(uint32_t l4Entries) { return l4Entries / getL5SkipStride();
    }
};
/*
 * Sparse sparse layout for random access word counts:
 *
 * selector bit
 * 0 => L6 entry, with word, data file deltas
 * 1 => overflow entry, with word, data file deltas, sparse counts
 *
 * Read from file to memory (compressed mix of L6 entries and overflow entries)
 *
 * Uncompressed L7 array in memory, usable for binary search.
 *
 * File header should contain number of entries
 */

class PageDict4SSWriter : public PageDict4PageParams
{
    using EC = PostingListCountFileEncodeContext;
    using SSEC = EC;

private:
    EC &_eL6;           // L6 stream
    std::string _l6Word;   // L6 word
    StartOffset _l6StartOffset; // file offsets + accnum
    uint64_t _l6PageNum;    // Page number for last L6 entry
    uint32_t _l6SparsePageNum;  // Sparse page number for last L6 entry
    uint64_t _l6WordNum;

public:
    PageDict4SSWriter(SSEC &sse);

    ~PageDict4SSWriter();

    /*
     * Add L6 skip entry.
     *
     * startOffset represents file position / accNumDocs after word.
     */
    void
    addL6Skip(std::string_view word,
              const StartOffset &startOffset,
              uint64_t wordNum,
              uint64_t pageNum,
              uint32_t sparsePageNum);

    /*
     * Add overflow counts entry.
     *
     * startOffset represents file position / accNumDocs at start of entry.
     */
    void
    addOverflowCounts(std::string_view word,
                      const Counts &counts,
                      const StartOffset &startOffset,
                      uint64_t wordNum);

    void flush();
};


/*
 * Sparse page layout for random access word counts:
 *
 * 15 bits L5 size
 * 15 bits L4 size
 * 15 bits number of L3 entries in page
 *     this can be used to derive number of L4 and L5 entries, using
 *     skip stride info.
 * 12 bits word string size
 * L5 data (word ref delta, offset to L4 and L3 data, data file delta)
 * L4 data (word ref delta, offset to L3 data, data file delta)
 * L3 data (word ref delta, offset to full page file is implicit, data file delta)
 * padding
 * word strings (LCP + suffix + NUL)
 *
 * File header should be defined
 */

class PageDict4SPWriter : public PageDict4PageParams
{
    using EC = PostingListCountFileEncodeContext;
    using SSWriter = PageDict4SSWriter;

private:
    EC _eL3;            // L3 stream
    ComprFileWriteContext _wcL3;// L3 buffer
    EC _eL4;            // L4 stream
    ComprFileWriteContext _wcL4;// L4 buffer
    EC _eL5;            // L5 stream
    ComprFileWriteContext _wcL5;// L5 buffer
    std::string _l3Word;   // last L3 word written
    std::string _l4Word;   // last L4 word written
    std::string _l5Word;   // last L5 word written
    std::string _l6Word;   // word before this sparse page
    uint32_t _l3WordOffset; // Offset for next L3 word to write
    uint32_t _l4WordOffset; // Offset for last L4 word written
    uint32_t _l5WordOffset; // Offset for last L5 word written

    // Offsets in data files for last L3 entry
    StartOffset _l3StartOffset;

    // Offsets in data files for last L4 entry
    StartOffset _l4StartOffset;

    // Offsets in data files for last L5 entry
    StartOffset _l5StartOffset;

    // Offsets in data files for last L6 entry
    StartOffset _l6StartOffset;

    uint64_t _l3WordNum;    // word number last L3 entry
    uint64_t _l4WordNum;    // word number last L4 entry
    uint64_t _l5WordNum;    // word number last L5 entry
    uint64_t _l6WordNum;    // word number last L6 entry

    uint32_t _curL3OffsetL4;    // Offset in L3 for last L4 entry
    uint32_t _curL3OffsetL5;    // Offset in L3 for last L5 entry
    uint32_t _curL4OffsetL5;    // Offset in L4 for last L5 entry

    uint32_t _headerSize;   // Size of page header

    uint32_t _l3Entries;    // Number of L3 entries on page
    uint32_t _l4StrideCheck;    // L3 entries since last L4 entry
    uint32_t _l5StrideCheck;    // L4 entries since last L5 entry

    uint32_t _l3Size;       // Size of L3 entries
    uint32_t _l4Size;       // Size of L4 entries
    uint32_t _l5Size;       // Size of L5 entries
    uint32_t _prevL3Size;   // Previous size of L3 entries
    uint32_t _prevL4Size;   // Previous size of L4 entries
    uint32_t _prevL5Size;   // Previous size of L5 entries
    uint32_t _prevWordsSize;    // previous size of words
    uint32_t _sparsePageNum;
    uint32_t _l3PageNum;    // Page number for last L3 entry
    std::vector<char> _words;   // Word buffer

    // Sparse sparse entries and counts that don't fit in a page
    SSWriter &_ssWriter;
    // Encode context where paged sparse counts go
    EC &_spe;

public:
    PageDict4SPWriter(SSWriter &sparseSparsewriter, EC &spe);

    ~PageDict4SPWriter();
    void setup();
    void flushPage();
    void flush();
    void resetPage();
    void addL3Skip(std::string_view word, const StartOffset &startOffset, uint64_t wordNum, uint64_t pageNum);
    void addL4Skip(size_t &lcp);
    void addL5Skip(size_t &lcp);

    bool empty() const { return _l3Entries == 0; }
    uint32_t getSparsePageNum() const { return _sparsePageNum; }

    /*
     * Add overflow counts entry.
     *
     * startOffset represents file position / accNumDocs at start of entry.
     */
    void addOverflowCounts(std::string_view word, const Counts &counts, const StartOffset &startOffset, uint64_t wordNum)
    {
        _ssWriter.addOverflowCounts(word, counts, startOffset, wordNum);
    }
};

/*
 * Page layout for random access word counts:
 *
 * 15 bits L2 size
 * 15 bits L1 size
 * 15 bits number of words in page
 *     this can be used to derive number of L1 and L2 entries, using
 *     skip stride info.
 * 12 bits word string size
 * L2 data (word ref delta, offset to L1 and counts data, data file delta)
 * L1 data (word ref delta, offset to counts, data file delta)
 * counts (sparse count)
 * padding
 * word strings (LCP + suffix + NUL)
 *
 * Alternate layout for overflow page:
 *
 * 15 bits L2 size hardcoded to 0
 * 15 bits L1 size hardcoded to 0
 * 15 bits number of words in page, hardcoded to 0
 * 12 bits word string size, hardcoded to 0
 * More info in sparse sparse file.
 *
 * File header should be defined
 */

class PageDict4PWriter : public PageDict4PageParams
{
public:
    using SPWriter = PageDict4SPWriter;
    using EC = PostingListCountFileEncodeContext;

private:
    EC _eCounts;    // counts stream (sparse counts)
    ComprFileWriteContext _wcCounts;// counts buffer
    EC _eL1;            // L1 stream
    ComprFileWriteContext _wcL1;// L1 buffer
    EC _eL2;            // L2 stream
    ComprFileWriteContext _wcL2;// L2 buffer
    std::string _countsWord;   // last counts on page
    std::string _l1Word;   // Last L1 word written
    std::string _l2Word;   // Last L2 word written
    std::string _l3Word;   // word before this page
    std::string _pendingCountsWord; // pending counts word (counts written)
    uint32_t _countsWordOffset; // Offset for next counts word to write
    uint32_t _l1WordOffset; // Offset of last L1 word written
    uint32_t _l2WordOffset; // Offset of last L2 word written

    // file offsets
    StartOffset _countsStartOffset;

    // Offsets in data files for last L1 entry
    StartOffset _l1StartOffset;

    // Offsets in data files for last L2 entry
    StartOffset _l2StartOffset;

    // Offsets in data files for last L3 entry
    StartOffset _l3StartOffset;

    uint32_t _curCountOffsetL1; // Offset in eCounts for last L1 entry
    uint32_t _curCountOffsetL2; // Offset in eCounts for last L2 entry
    uint32_t _curL1OffsetL2;    // Offset in eL1 for last L2 entry

    uint32_t _headerSize;   // Size of page header

    uint32_t _countsEntries;    // Number of count entries on page
    uint32_t _l1StrideCheck;    // Count entries since last L1 entry
    uint32_t _l2StrideCheck;    // L1 entries since last L2 entry

    uint32_t _countsSize;   // Size of counts
    uint32_t _l1Size;       // Size of L1 entries
    uint32_t _l2Size;       // Size of L2 entries
    uint32_t _prevL1Size;   // Previous size of L1 entries
    uint32_t _prevL2Size;   // Previous size of L2 entries
    uint64_t _pageNum;      // Page number.
    uint64_t _l3WordNum;    // last L3 word num written
    uint64_t _wordNum;      // current word number
    std::vector<char> _words;   // Word buffer
    SPWriter &_spWriter;
    // Encode context where paged counts go
    EC &_pe;

    void
    addOverflowCounts(std::string_view word,
                      const Counts &counts);

public:
    PageDict4PWriter(SPWriter &spWriter, EC &pe);
    ~PageDict4PWriter();

    void setup();
    void flushPage();
    void flush();
    void resetPage();
    void addCounts(std::string_view word, const Counts &counts);
    void addL1Skip(size_t &lcp);
    void addL2Skip(size_t &lcp);
    bool empty() const { return _countsEntries == 0;}
    uint64_t getPageNum() const { return _pageNum; }
    uint64_t getWordNum() const { return _wordNum - 1; }
};


class PageDict4SSLookupRes
{
public:
    using Counts = index::PostingListCounts;
    using StartOffset = PageDict4StartOffset;

    std::string _l6Word;   // last L6 word before key
    std::string _lastWord; // L6 or overflow word >= key
    StartOffset      _l6StartOffset;    // File offsets
    Counts      _counts;    // Counts valid if overflow
    uint64_t _pageNum;
    uint64_t _sparsePageNum;
    uint64_t _l6WordNum;            // wordnum if overflow
    StartOffset      _startOffset;      // valid if overflow
    bool _res;
    bool _overflow;

    PageDict4SSLookupRes();
    ~PageDict4SSLookupRes();
};

/* Reader for sparse sparse file.
 *
 * Read from file to memory (compressed mix of L6 entries and overflow entries)
 *
 * Uncompressed L7 array in memory, usable for binary search.
 */

class PageDict4SSReader : public PageDict4PageParams
{
    using EC = PostingListCountFileEncodeContext;
    using DC = PostingListCountFileDecodeContext;
public:
    class L7Entry
    {
    public:
        std::string _l7Word;
        StartOffset      _l7StartOffset;    // Offsets in data files
        uint64_t _l7WordNum;
        uint64_t _l6Offset; // Offset in L6+overflow stream
        uint32_t _sparsePageNum;// page number for sparse file
        uint64_t _pageNum;  // page number in full file
        uint32_t _l7Ref;    // L7 entry before overflow, or self-ref if L6

        L7Entry()
            : _l7Word(),
              _l7StartOffset(),
              _l7WordNum(0),
              _l6Offset(0),
              _sparsePageNum(0),
              _pageNum(0),
              _l7Ref(0)
        {
        }

        L7Entry(std::string_view l7Word,
                const StartOffset         &l7StartOffset,
                uint64_t l7WordNum,
                uint64_t l6Offset,
                uint32_t sparsePageNum,
                uint64_t pageNum,
                uint32_t l7Ref)
            : _l7Word(l7Word),
              _l7StartOffset(l7StartOffset),
              _l7WordNum(l7WordNum),
              _l6Offset(l6Offset),
              _sparsePageNum(sparsePageNum),
              _pageNum(pageNum),
              _l7Ref(l7Ref)
        {
        }

        bool operator<(std::string_view word) const {
            return _l7Word < word;
        }
    };

    class OverflowRef
    {
    public:
        uint64_t _wordNum;
        uint32_t _l7Ref;    // overflow entry in L7 table

        OverflowRef()
            : _wordNum(0),
              _l7Ref(0)
        {
        }

        OverflowRef(uint64_t wordNum, uint32_t l7Ref)
            : _wordNum(wordNum),
              _l7Ref(l7Ref)
        {
        }

        bool operator<(uint64_t wordNum) const {
            return _wordNum < wordNum;
        }
    };

    ComprBuffer _cb;
    uint64_t _ssFileBitLen; // File size in bits
    uint32_t _ssStartOffset;    // Header size in bits

    using L7Vector = std::vector<L7Entry>;
    L7Vector _l7;// Uncompressed skip list for sparse sparse file

    DC _ssd;    // used to store compression parameters
    uint64_t _spFileBitLen;
    uint64_t _pFileBitLen;
    uint32_t _spStartOffset;
    uint32_t _pStartOffset;
    uint32_t _spFirstPageNum;
    uint32_t _spFirstPageOffset;
    uint32_t _pFirstPageNum;
    uint32_t _pFirstPageOffset;

    using OverflowVector = std::vector<OverflowRef>;
    OverflowVector _overflows;

    PageDict4SSReader(const ComprBuffer &cb,
                      uint32_t ssFileHeaderSize,
                      uint64_t ssFileBitLen,
                      uint32_t spFileHeaderSize,
                      uint64_t spFileBitLen,
                      uint32_t pFileHeaderSize,
                      uint64_t pFileBitLen);
    ~PageDict4SSReader();

    void setup(DC &ssd);
    PageDict4SSLookupRes lookup(std::string_view key);
    PageDict4SSLookupRes lookupOverflow(uint64_t wordNum) const;
    const DC & getSSD() const { return _ssd; }
};


class PageDict4SPLookupRes : public PageDict4PageParams
{
    using EC = PostingListCountFileEncodeContext;
    using DC = PostingListCountFileDecodeContext;
    using SSReader = PageDict4SSReader;

public:
    std::string _l3Word;
    std::string _lastWord; // L3 word >= key
    StartOffset      _l3StartOffset;
    uint64_t _pageNum;
    uint64_t _l3WordNum;

public:
    PageDict4SPLookupRes();

    ~PageDict4SPLookupRes();

    void
    lookup(const SSReader &ssReader,
           const void *sparsePage,
           std::string_view key,
           std::string_view l6Word,
           std::string_view lastSPWord,
           const StartOffset         &l6StartOffset,
           uint64_t l6WordNum,
           uint64_t lowestPageNum);
};


class PageDict4PLookupRes : public PageDict4PageParams
{
public:
    using EC = PostingListCountFileEncodeContext;
    using DC = PostingListCountFileDecodeContext;
    using SSReader = PageDict4SSReader;

public:
    Counts _counts;
    StartOffset _startOffset;
    uint64_t _wordNum;
    bool _res;
    std::string *_nextWord;

public:
    PageDict4PLookupRes();
    ~PageDict4PLookupRes();

    bool
    lookup(const SSReader &ssReader,
           const void *page,
           std::string_view key,
           std::string_view l3Word,
           std::string_view lastPWord,
           const StartOffset &l3StartOffset,
           uint64_t l3WordNum);
};


class PageDict4Reader : public PageDict4PageParams
{
public:
    using DC = PostingListCountFileDecodeContext;
    using EC = PostingListCountFileEncodeContext;
    using SSReader = PageDict4SSReader;

    DC &_pd;
    uint32_t _countsResidue;
    const SSReader &_ssReader;
    uint64_t _pFileBitLen;
    StartOffset _startOffset;
    bool _overflowPage;
    using PCV = std::vector<Counts>;
    struct L1SkipCheck
    {
        uint32_t    wordOffset;
        StartOffset startOffset;
        uint32_t    countOffset;
        L1SkipCheck(const StartOffset &startOffset_)
            : wordOffset(0),
              startOffset(startOffset_),
              countOffset(0)
        {
        }
    };
    struct L2SkipCheck : public L1SkipCheck
    {
        uint32_t    l1Offset;
        L2SkipCheck(const StartOffset &startOffset_)
            : L1SkipCheck(startOffset_),
              l1Offset(0)
        {
        }

        bool check(const L1SkipCheck &rhs) const {
            return startOffset == rhs.startOffset &&
                countOffset == rhs.countOffset;
        }
    };
    struct L3SkipCheck
    {
        StartOffset startOffset;
        uint64_t    wordNum;
        L3SkipCheck(const StartOffset &startOffset_, uint64_t wordNum_)
            : startOffset(startOffset_),
              wordNum(wordNum_)
        {
        }
    };
    struct L4SkipCheck : public L3SkipCheck
    {
        uint32_t    wordOffset;
        uint32_t    l3Offset;
        L4SkipCheck(const StartOffset &startOffset_, uint64_t wordNum_)
            : L3SkipCheck(startOffset_, wordNum_),
              wordOffset(0),
              l3Offset(0)
        {
        }
        bool check(const L3SkipCheck &rhs) const {
            return startOffset == rhs.startOffset &&
                wordNum == rhs.wordNum;
        }
    };
    struct L5SkipCheck : public L4SkipCheck
    {
        uint32_t    l4Offset;
        L5SkipCheck(const StartOffset &startOffset_, uint64_t wordNum_)
            : L4SkipCheck(startOffset_, wordNum_),
              l4Offset(0)
        {
        }
        bool check(const L4SkipCheck &rhs) const {
            return startOffset == rhs.startOffset &&
                wordNum == rhs.wordNum &&
                l3Offset == rhs.l3Offset;
        }
    };
    template <typename Element>
    class CheckVector
    {
        using Vector = std::vector<Element>;
        Vector _vector;
        typename Vector::const_iterator _cur;
        typename Vector::const_iterator _end;
    public:
        CheckVector()
            : _vector(),
              _cur(),
              _end()
        {}
        void clear() { _vector.clear(); }
        void setup() {
            _cur = _vector.cbegin();
            _end = _vector.cend();
        }
        bool valid() const { return _cur != _end; }
        const Element *operator->() const { return _cur.operator->(); }
        void step() { ++_cur; }
        void push_back(const Element &element) {
            _vector.push_back(element);
        }
    };
    template <typename Entry1, typename Entry2>
    void
    checkWordOffsets(const std::vector<char> &words,
                     CheckVector<Entry1> &skip1,
                     CheckVector<Entry2> &skip2);
    PCV _counts;
    PCV::const_iterator _cc;
    PCV::const_iterator _ce;
    using WV = std::vector<char>;
    WV _words;
    WV::const_iterator _wc;
    WV::const_iterator _we;
    std::string _lastWord;
    std::string _lastSSWord;

    DC &_spd;
    uint32_t _l3Residue;
    WV _spwords;
    WV::const_iterator _spwc;
    WV::const_iterator _spwe;

    DC _ssd;
    uint64_t _wordNum;
    CheckVector<L1SkipCheck> _l1SkipChecks;
    CheckVector<L2SkipCheck> _l2SkipChecks;
    CheckVector<L3SkipCheck> _l3SkipChecks;
    CheckVector<L4SkipCheck> _l4SkipChecks;
    CheckVector<L5SkipCheck> _l5SkipChecks;

    PageDict4Reader(const SSReader &ssReader, DC &spd,DC &pd);
    ~PageDict4Reader();

    void setup();
    void setupPage();
    void setupSPage();
    void decodePWord(std::string &word);
    void decodeSPWord(std::string &word);
    void decodeSSWord(std::string &word);
    void readCounts(std::string &word, uint64_t &wordNum, Counts &counts);
    void readOverflowCounts(std::string &word, Counts &counts);
};

}

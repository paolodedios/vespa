// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/storageapi/messageapi/bucketcommand.h>
#include <vespa/storageapi/messageapi/bucketreply.h>

namespace storage::api {

/**
 * \class StatBucketCommand
 * \ingroup stat
 *
 * \brief Command used to get information about a given bucket..
 *
 * Command used by stat to get detailed information about a bucket.
 */
class StatBucketCommand : public BucketCommand {
private:
    std::string _docSelection;
public:
    StatBucketCommand(const document::Bucket &bucket,
                      std::string_view documentSelection);
    ~StatBucketCommand() override;

    const std::string& getDocumentSelection() const { return _docSelection; }
    void print(std::ostream& out, bool verbose, const std::string& indent) const override;
    DECLARE_STORAGECOMMAND(StatBucketCommand, onStatBucket);
};

class StatBucketReply : public BucketReply {
    std::string _results;
public:
    explicit StatBucketReply(const StatBucketCommand&, std::string_view results = "");
    const std::string& getResults() const noexcept { return _results; }
    void print(std::ostream& out, bool verbose, const std::string& indent) const override;
    DECLARE_STORAGEREPLY(StatBucketReply, onStatBucketReply)
};

/**
 * \class GetBucketListCommand
 * \ingroup stat
 *
 * \brief Command used to find actual buckets related to a given one.
 *
 * Command used by stat to query distributor to find actual buckets contained
 * by the given bucket, or buckets that contain the given bucket. (getAll() call
 * on bucket database)
 */
class GetBucketListCommand : public BucketCommand {
public:
    explicit GetBucketListCommand(const document::Bucket &bucket);
    void print(std::ostream& out, bool verbose, const std::string& indent) const override;
    DECLARE_STORAGECOMMAND(GetBucketListCommand, onGetBucketList);
};

class GetBucketListReply : public BucketReply {
public:
    struct BucketInfo {
        document::BucketId _bucket;
        std::string   _bucketInformation;

        BucketInfo(const document::BucketId& id,
                   std::string_view bucketInformation) noexcept
            : _bucket(id),
              _bucketInformation(bucketInformation)
        {}

        bool operator==(const BucketInfo& other) const noexcept {
            return (_bucket == other._bucket
                    && _bucketInformation == other._bucketInformation);
        }
    };

private:
    std::vector<BucketInfo> _buckets;

public:
    explicit GetBucketListReply(const GetBucketListCommand&);
    ~GetBucketListReply() override;
    std::vector<BucketInfo>& getBuckets() { return _buckets; }
    const std::vector<BucketInfo>& getBuckets() const { return _buckets; }
    void print(std::ostream& out, bool verbose, const std::string& indent) const override;
    DECLARE_STORAGEREPLY(GetBucketListReply, onGetBucketListReply)

};

std::ostream& operator<<(std::ostream& out, const GetBucketListReply::BucketInfo& instance);

}

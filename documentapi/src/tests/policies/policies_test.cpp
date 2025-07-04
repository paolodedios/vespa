// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "testframe.h"
#include <vespa/documentapi/documentapi.h>
#include <vespa/documentapi/messagebus/policies/andpolicy.h>
#include <vespa/documentapi/messagebus/policies/contentpolicy.h>
#include <vespa/documentapi/messagebus/policies/documentrouteselectorpolicy.h>
#include <vespa/documentapi/messagebus/policies/errorpolicy.h>
#include <vespa/documentapi/messagebus/policies/externpolicy.h>
#include <vespa/documentapi/messagebus/policies/loadbalancerpolicy.h>
#include <vespa/documentapi/messagebus/policies/localservicepolicy.h>
#include <vespa/documentapi/messagebus/policies/roundrobinpolicy.h>
#include <vespa/documentapi/messagebus/policies/subsetservicepolicy.h>
#include <vespa/messagebus/emptyreply.h>
#include <vespa/messagebus/routing/routingnode.h>
#include <vespa/messagebus/routing/routingtable.h>
#include <vespa/messagebus/routing/policydirective.h>
#include <vespa/messagebus/testlib/testserver.h>
#include <vespa/vdslib/state/clusterstate.h>
#include <vespa/document/base/testdocrepo.h>
#include <vespa/document/fieldvalue/longfieldvalue.h>
#include <vespa/document/datatype/documenttype.h>
#include <vespa/document/repo/documenttyperepo.h>
#include <vespa/document/update/documentupdate.h>
#include <vespa/document/fieldvalue/document.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/test/test_path.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <functional>
#include <optional>
#include <thread>

#include <vespa/log/log.h>
LOG_SETUP("policies_test");

using document::DataType;
using document::Document;
using document::DocumentId;
using document::DocumentTypeRepo;
using document::DocumentUpdate;
using document::readDocumenttypesConfig;
using slobrok::api::IMirrorAPI;
using namespace documentapi;
using vespalib::make_string;
using std::make_unique;
using std::make_shared;
using namespace std::chrono_literals;


class PoliciesTest : public testing::Test {
protected:
    static std::shared_ptr<const DocumentTypeRepo> _repo;
    static const DataType*                         _docType;
    PoliciesTest();
    ~PoliciesTest() override;
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    using WrappedPolicy = std::optional<std::reference_wrapper<ContentPolicy>>;

    static bool trySelect(TestFrame &frame, uint32_t numSelects, const std::vector<string> &expected);
    static void setupExternPolicy(TestFrame &frame, mbus::Slobrok &slobrok, const string &pattern, int32_t numEntries = -1);
    static void setupContentPolicy(TestFrame &frame, const string &param,
                                   const string &pattern, int32_t numEntries, WrappedPolicy& wrapped_policy);
    bool isErrorPolicy(const string &name, const string &param);
    static void assertMirrorReady(const IMirrorAPI &mirror);
    static void assertMirrorContains(const IMirrorAPI &mirror, const string &pattern, uint32_t numEntries);
    mbus::Message::UP newPutDocumentMessage(const string &documentId);
    std::shared_ptr<Document> make_doc(DocumentId docid) {
        return std::make_shared<Document>(*_repo, *_docType, docid);
    }
};

PoliciesTest::PoliciesTest() = default;
PoliciesTest::~PoliciesTest() = default;

std::shared_ptr<const DocumentTypeRepo> PoliciesTest::_repo;
const DataType* PoliciesTest::_docType = nullptr;

void
PoliciesTest::SetUpTestSuite()
{
    auto path = TEST_PATH("../../../test/cfg/testdoctypes.cfg");
    _repo = std::make_shared<DocumentTypeRepo>(readDocumenttypesConfig(path));
    _docType = _repo->getDocumentType("testdoc");
}

void
PoliciesTest::TearDownTestSuite()
{
    _repo.reset();
    _docType = nullptr;
}

const vespalib::duration TIMEOUT = 600s;

TEST_F(PoliciesTest, test_protocol)
{
    auto protocol = std::make_shared<DocumentProtocol>(_repo);

    mbus::IRoutingPolicy::UP policy = protocol->createPolicy("AND", "");
    ASSERT_TRUE(dynamic_cast<ANDPolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("DocumentRouteSelector", "raw:route[0]\n");
    ASSERT_TRUE(dynamic_cast<DocumentRouteSelectorPolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("Extern", "foo;bar/baz");
    ASSERT_TRUE(dynamic_cast<ExternPolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("LoadBalancer",
                                    "cluster=docproc/cluster.default;"
                                    "session=chain.default;syncinit");
    ASSERT_TRUE(dynamic_cast<LoadBalancerPolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("LocalService", "");
    ASSERT_TRUE(dynamic_cast<LocalServicePolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("RoundRobin", "");
    ASSERT_TRUE(dynamic_cast<RoundRobinPolicy*>(policy.get()) != nullptr);

    policy = protocol->createPolicy("SubsetService", "");
    ASSERT_TRUE(dynamic_cast<SubsetServicePolicy*>(policy.get()) != nullptr);
}

TEST_F(PoliciesTest, test_and)
{
    TestFrame frame(_repo);
    frame.setMessage(make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::"))));
    frame.setHop(mbus::HopSpec("test", "[AND]")
                 .addRecipient("foo")
                 .addRecipient("bar"));
    EXPECT_TRUE(frame.testSelect(StringList().add("foo").add("bar")));

    frame.setHop(mbus::HopSpec("test", "[AND:baz]")
                 .addRecipient("foo").addRecipient("bar"));
    EXPECT_TRUE(frame.testSelect(StringList().add("baz"))); // param precedes recipients

    frame.setHop(mbus::HopSpec("test", "[AND:foo]"));
    EXPECT_TRUE(frame.testMergeOneReply("foo"));

    frame.setHop(mbus::HopSpec("test", "[AND:foo bar]"));
    EXPECT_TRUE(frame.testMergeTwoReplies("foo", "bar"));
}

TEST_F(PoliciesTest, require_that_extern_policy_with_illegal_param_is_an_error_policy)
{
    mbus::Slobrok slobrok;
    string spec = vespalib::make_string("tcp/localhost:%d", slobrok.port());

    EXPECT_TRUE(isErrorPolicy("Extern", ""));
    EXPECT_TRUE(isErrorPolicy("Extern", spec));
    EXPECT_TRUE(isErrorPolicy("Extern", spec + ";"));
    EXPECT_TRUE(isErrorPolicy("Extern", spec + ";bar"));
}

TEST_F(PoliciesTest, require_that_extern_policy_with_unknown_pattern_selects_none)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));

    mbus::Slobrok slobrok;
    ASSERT_NO_FATAL_FAILURE(setupExternPolicy(frame, slobrok, "foo/bar"));
    EXPECT_TRUE(frame.testSelect(StringList()));
}

TEST_F(PoliciesTest, require_that_extern_policy_selects_from_extern_slobrok)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));
    mbus::Slobrok slobrok;
    std::vector<mbus::TestServer*> servers;
    for (uint32_t i = 0; i < 10; ++i) {
        auto *server = new mbus::TestServer(
                mbus::Identity(make_string("docproc/cluster.default/%d", i)), mbus::RoutingSpec(), slobrok,
                std::make_shared<DocumentProtocol>(_repo));
        servers.push_back(server);
        server->net.registerSession("chain.default");
    }
    ASSERT_NO_FATAL_FAILURE(setupExternPolicy(frame, slobrok, "docproc/cluster.default/*/chain.default", 10));
    std::set<string> lst;
    for (uint32_t i = 0; i < servers.size(); ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());

        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }
    EXPECT_EQ(servers.size(), lst.size());
    for (auto & server : servers) {
        delete server;
    }
}

TEST_F(PoliciesTest, require_that_extern_policy_merges_one_reply_as_protocol)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));
    mbus::Slobrok slobrok;
    mbus::TestServer server(mbus::Identity("docproc/cluster.default/0"), mbus::RoutingSpec(), slobrok,
                            std::make_shared<DocumentProtocol>(_repo));
    server.net.registerSession("chain.default");
    ASSERT_NO_FATAL_FAILURE(setupExternPolicy(frame, slobrok, "docproc/cluster.default/0/chain.default", 1));
    EXPECT_TRUE(frame.testMergeOneReply(server.net.getConnectionSpec() + "/chain.default"));
}

mbus::Message::UP
PoliciesTest::newPutDocumentMessage(const string &documentId)
{
    return make_unique<PutDocumentMessage>(make_doc(DocumentId(documentId)));
}

void
PoliciesTest::setupExternPolicy(TestFrame &frame, mbus::Slobrok &slobrok, const string &pattern, int32_t numEntries)
{
    string param = vespalib::make_string("tcp/localhost:%d;%s", slobrok.port(), pattern.c_str());
    frame.setHop(mbus::HopSpec("test", vespalib::make_string("[Extern:%s]", param.c_str())));
    mbus::MessageBus &mbus = frame.getMessageBus();
    const mbus::HopBlueprint *hop = mbus.getRoutingTable(DocumentProtocol::NAME)->getHop("test");
    const mbus::PolicyDirective &dir = dynamic_cast<const mbus::PolicyDirective&>(*hop->getDirective(0));
    ExternPolicy &policy = dynamic_cast<ExternPolicy&>(*mbus.getRoutingPolicy(DocumentProtocol::NAME, dir.getName(), dir.getParam()));
    ASSERT_NO_FATAL_FAILURE(assertMirrorReady(*policy.getMirror()));
    if (numEntries >= 0) {
        ASSERT_NO_FATAL_FAILURE(assertMirrorContains(*policy.getMirror(), pattern, numEntries));
    }
}

void
PoliciesTest::assertMirrorReady(const slobrok::api::IMirrorAPI &mirror)
{
    for (uint32_t i = 0; i < 6000; ++i) {
        if (mirror.ready()) {
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    FAIL() << "Mirror not ready";
}

void
PoliciesTest::assertMirrorContains(const slobrok::api::IMirrorAPI &mirror, const string &pattern, uint32_t numEntries)
{
    for (uint32_t i = 0; i < 6000; ++i) {
        if (mirror.lookup(pattern).size() == numEntries) {
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    FAIL() << "Mirror does not contain pattern '" << pattern << "'";
}

TEST_F(PoliciesTest, test_extern_send)
{
    // Setup local source node.
    mbus::Slobrok local;
    mbus::TestServer src(mbus::Identity("src"), mbus::RoutingSpec(), local,
                         std::make_shared<DocumentProtocol>(_repo));
    mbus::Receptor sr;
    mbus::SourceSession::UP ss = src.mb.createSourceSession(sr, mbus::SourceSessionParams().setTimeout(60s));

    mbus::Slobrok slobrok;
    mbus::TestServer itr(mbus::Identity("itr"), mbus::RoutingSpec()
                         .addTable(mbus::RoutingTableSpec(DocumentProtocol::NAME)
                                   .addRoute(std::move(mbus::RouteSpec("default").addHop("dst")))
                                   .addHop(mbus::HopSpec("dst", "dst/session"))),
                         slobrok, std::make_shared<DocumentProtocol>(_repo));
    mbus::Receptor ir;
    mbus::IntermediateSession::UP is = itr.mb.createIntermediateSession("session", true, ir, ir);

    mbus::TestServer dst(mbus::Identity("dst"), mbus::RoutingSpec(), slobrok,
                         std::make_shared<DocumentProtocol>(_repo));
    mbus::Receptor dr;
    mbus::DestinationSession::UP ds = dst.mb.createDestinationSession("session", true, dr);

    // Send message from local node to remote cluster and resolve route there.
    mbus::Message::UP msg = std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::"));
    msg->getTrace().setLevel(9);
    msg->setRoute(mbus::Route::parse(vespalib::make_string("[Extern:tcp/localhost:%d;itr/session] default", slobrok.port())));

    ASSERT_TRUE(ss->send(std::move(msg)).isAccepted());
    ASSERT_TRUE((msg = ir.getMessage(TIMEOUT)));
    is->forward(std::move(msg));
    ASSERT_TRUE((msg = dr.getMessage(TIMEOUT)));
    ds->acknowledge(std::move(msg));
    mbus::Reply::UP reply = ir.getReply(TIMEOUT);
    ASSERT_TRUE(reply);
    is->forward(std::move(reply));
    ASSERT_TRUE((reply = sr.getReply(TIMEOUT)));

    fprintf(stderr, "%s", reply->getTrace().toString().c_str());
}

TEST_F(PoliciesTest, test_extern_multiple_slobroks)
{
    mbus::Slobrok local;
    mbus::TestServer src(mbus::Identity("src"), mbus::RoutingSpec(), local,
                         std::make_shared<DocumentProtocol>(_repo));
    mbus::Receptor sr;
    mbus::SourceSession::UP ss = src.mb.createSourceSession(sr, mbus::SourceSessionParams().setTimeout(60s));

    string spec;
    mbus::Receptor dr;
    {
        mbus::Slobrok ext;
        spec.append(vespalib::make_string("tcp/localhost:%d", ext.port()));

        mbus::TestServer dst(mbus::Identity("dst"), mbus::RoutingSpec(), ext,
                             std::make_shared<DocumentProtocol>(_repo));
        mbus::DestinationSession::UP ds = dst.mb.createDestinationSession("session", true, dr);

        mbus::Message::UP msg = std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::"));
        msg->setRoute(mbus::Route::parse(vespalib::make_string("[Extern:%s;dst/session]", spec.c_str())));
        ASSERT_TRUE(ss->send(std::move(msg)).isAccepted());
        ASSERT_TRUE((msg = dr.getMessage(TIMEOUT)));
        ds->acknowledge(std::move(msg));
        mbus::Reply::UP reply = sr.getReply(TIMEOUT);
        ASSERT_TRUE(reply);
    }
    {
        mbus::Slobrok ext;
        spec.append(vespalib::make_string(",tcp/localhost:%d", ext.port()));

        mbus::TestServer dst(mbus::Identity("dst"), mbus::RoutingSpec(), ext,
                             std::make_shared<DocumentProtocol>(_repo));
        mbus::DestinationSession::UP ds = dst.mb.createDestinationSession("session", true, dr);

        mbus::Message::UP msg = std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::"));
        msg->setRoute(mbus::Route::parse(vespalib::make_string("[Extern:%s;dst/session]", spec.c_str())));
        ASSERT_TRUE(ss->send(std::move(msg)).isAccepted());
        ASSERT_TRUE((msg = dr.getMessage(TIMEOUT)));
        ds->acknowledge(std::move(msg));
        mbus::Reply::UP reply = sr.getReply(TIMEOUT);
        ASSERT_TRUE(reply);
    }
}

TEST_F(PoliciesTest, test_local_service)
{
    // Prepare message.
    TestFrame frame(_repo, "docproc/cluster.default");
    frame.setMessage(make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::"))));

    // Test select with proper address.
    for (uint32_t i = 0; i < 10; ++i) {
        frame.getNetwork().registerSession(vespalib::make_string("%d/chain.default", i));
    }
    ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 10));
    frame.setHop(mbus::HopSpec("test", "docproc/cluster.default/[LocalService]/chain.default"));

    std::set<string> lst;
    for (uint32_t i = 0; i < 10; ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());

        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }
    EXPECT_EQ(10u, lst.size());

    // Test select with broken address.
    lst.clear();
    frame.setHop(mbus::HopSpec("test", "docproc/cluster.default/[LocalService:broken]/chain.default"));
    for (uint32_t i = 0; i < 10; ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());

        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }
    EXPECT_EQ(1u, lst.size());
    EXPECT_EQ("docproc/cluster.default/*/chain.default", *lst.begin());

    // Test merge behavior.
    frame.setHop(mbus::HopSpec("test", "[LocalService]"));
    EXPECT_TRUE(frame.testMergeOneReply("*"));
}

TEST_F(PoliciesTest, test_local_service_cache)
{
    TestFrame fooFrame(_repo, "docproc/cluster.default");
    mbus::HopSpec fooHop("foo", "docproc/cluster.default/[LocalService]/chain.foo");
    fooFrame.setMessage(make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::foo")));
    fooFrame.setHop(fooHop);

    TestFrame barFrame(fooFrame);
    mbus::HopSpec barHop("test", "docproc/cluster.default/[LocalService]/chain.bar");
    barFrame.setMessage(std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::bar")));
    barFrame.setHop(barHop);

    fooFrame.getMessageBus().setupRouting(
            mbus::RoutingSpec().addTable(mbus::RoutingTableSpec(DocumentProtocol::NAME)
                                         .addHop(std::move(fooHop))
                                         .addHop(std::move(barHop))));

    fooFrame.getNetwork().registerSession("0/chain.foo");
    fooFrame.getNetwork().registerSession("0/chain.bar");
    ASSERT_TRUE(fooFrame.waitSlobrok("docproc/cluster.default/0/*", 2));

    std::vector<mbus::RoutingNode*> fooSelected;
    ASSERT_TRUE(fooFrame.select(fooSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.foo", fooSelected[0]->getRoute().getHop(0).toString());

    std::vector<mbus::RoutingNode*> barSelected;
    ASSERT_TRUE(barFrame.select(barSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.bar", barSelected[0]->getRoute().getHop(0).toString());

    barSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());
    fooSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());

    ASSERT_TRUE(barFrame.getReceptor().getReply(TIMEOUT));
    ASSERT_TRUE(fooFrame.getReceptor().getReply(TIMEOUT));
}

TEST_F(PoliciesTest, test_round_robin)
{
    // Prepare message.
    TestFrame frame(_repo, "docproc/cluster.default");
    frame.setMessage(make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::"))));

    // Test select with proper address.
    for (uint32_t i = 0; i < 10; ++i) {
        frame.getNetwork().registerSession(vespalib::make_string("%d/chain.default", i));
    }
    ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 10));
    frame.setHop(mbus::HopSpec("test", "[RoundRobin]")
                 .addRecipient("docproc/cluster.default/3/chain.default")
                 .addRecipient("docproc/cluster.default/6/chain.default")
                 .addRecipient("docproc/cluster.default/9/chain.default"));
    EXPECT_TRUE(trySelect(frame, 32, StringList()
                          .add("docproc/cluster.default/3/chain.default")
                          .add("docproc/cluster.default/6/chain.default")
                          .add("docproc/cluster.default/9/chain.default")));
    frame.getNetwork().unregisterSession("6/chain.default");
    ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 9));
    EXPECT_TRUE(trySelect(frame, 32, StringList()
                          .add("docproc/cluster.default/3/chain.default")
                          .add("docproc/cluster.default/9/chain.default")));
    frame.getNetwork().unregisterSession("3/chain.default");
    ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 8));
    EXPECT_TRUE(trySelect(frame, 32, StringList()
                          .add("docproc/cluster.default/9/chain.default")));
    frame.getNetwork().unregisterSession("9/chain.default");
    ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 7));
    EXPECT_TRUE(trySelect(frame, 32, StringList()));

    // Test merge behavior.
    frame.setHop(mbus::HopSpec("test", "[RoundRobin]").addRecipient("docproc/cluster.default/0/chain.default"));
    EXPECT_TRUE(frame.testMergeOneReply("docproc/cluster.default/0/chain.default"));
}

TEST_F(PoliciesTest, test_round_robin_cache)
{
    TestFrame fooFrame(_repo, "docproc/cluster.default");
    mbus::HopSpec fooHop("foo", "[RoundRobin]");
    fooHop.addRecipient("docproc/cluster.default/0/chain.foo");
    fooFrame.setMessage(std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::foo")));
    fooFrame.setHop(fooHop);

    TestFrame barFrame(fooFrame);
    mbus::HopSpec barHop("bar", "[RoundRobin]");
    barHop.addRecipient("docproc/cluster.default/0/chain.bar");
    barFrame.setMessage(std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::bar")));
    barFrame.setHop(barHop);

    fooFrame.getMessageBus().setupRouting(
            mbus::RoutingSpec().addTable(mbus::RoutingTableSpec(DocumentProtocol::NAME)
                                         .addHop(std::move(fooHop))
                                         .addHop(std::move(barHop))));

    fooFrame.getNetwork().registerSession("0/chain.foo");
    fooFrame.getNetwork().registerSession("0/chain.bar");
    ASSERT_TRUE(fooFrame.waitSlobrok("docproc/cluster.default/0/*", 2));

    std::vector<mbus::RoutingNode*> fooSelected;
    ASSERT_TRUE(fooFrame.select(fooSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.foo", fooSelected[0]->getRoute().getHop(0).toString());

    std::vector<mbus::RoutingNode*> barSelected;
    ASSERT_TRUE(barFrame.select(barSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.bar", barSelected[0]->getRoute().getHop(0).toString());

    barSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());
    fooSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());

    ASSERT_TRUE(barFrame.getReceptor().getReply(TIMEOUT));
    ASSERT_TRUE(fooFrame.getReceptor().getReply(TIMEOUT));
}

TEST_F(PoliciesTest, multiple_get_replies_are_merged_to_found_document)
{
    TestFrame frame(_repo);
    frame.setHop(mbus::HopSpec("test", "[DocumentRouteSelector:raw:"
                               "route[2]\n"
                               "route[0].name \"foo\"\n"
                               "route[0].selector \"testdoc\"\n"
                               "route[0].feed \"myfeed\"\n"
                               "route[1].name \"bar\"\n"
                               "route[1].selector \"testdoc\"\n"
                               "route[1].feed \"myfeed\"\n]")
                 .addRecipient("foo")
                 .addRecipient("bar"));
    frame.setMessage(make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::yarn")));
    std::vector<mbus::RoutingNode*> selected;
    EXPECT_TRUE(frame.select(selected, 2));
    for (uint32_t i = 0, len = selected.size(); i < len; ++i) {
        Document::SP doc;
        if (i == 0) {
            doc = make_doc(DocumentId("id:ns:testdoc::yarn"));
            doc->setLastModified(123456ULL);
        }
        auto reply = std::make_unique<GetDocumentReply>(std::move(doc));
        selected[i]->handleReply(std::move(reply));
    }
    mbus::Reply::UP reply = frame.getReceptor().getReply(TIMEOUT);
    EXPECT_TRUE(reply);
    EXPECT_EQ(static_cast<uint32_t>(DocumentProtocol::REPLY_GETDOCUMENT), reply->getType());
    EXPECT_EQ(123456ULL, dynamic_cast<GetDocumentReply&>(*reply).getLastModified());
}

TEST_F(PoliciesTest, test_document_route_selector)
{
    // Test policy with usage safeguard.
    string okConfig = "raw:route[0]\n";
    string errConfig = "raw:"
        "route[1]\n"
        "route[0].name \"foo\"\n"
        "route[0].selector \"foo bar\"\n"
        "route[0].feed \"baz\"\n";
    {
        DocumentProtocol protocol(_repo, okConfig);
        EXPECT_TRUE(dynamic_cast<DocumentRouteSelectorPolicy*>(protocol.createPolicy("DocumentRouteSelector", "").get()) != nullptr);
        EXPECT_TRUE(dynamic_cast<ErrorPolicy*>(protocol.createPolicy("DocumentRouteSelector", errConfig).get()) != nullptr);
    }
    {
        DocumentProtocol protocol(_repo, errConfig);
        EXPECT_TRUE(dynamic_cast<ErrorPolicy*>(protocol.createPolicy("DocumentRouteSelector", "").get()) != nullptr);
        EXPECT_TRUE(dynamic_cast<DocumentRouteSelectorPolicy*>(protocol.createPolicy("DocumentRouteSelector", okConfig).get()) != nullptr);
    }

    // Test policy with proper config.
    TestFrame frame(_repo);
    frame.setHop(mbus::HopSpec("test", "[DocumentRouteSelector:raw:"
                               "route[2]\n"
                               "route[0].name \"foo\"\n"
                               "route[0].selector \"testdoc\"\n"
                               "route[0].feed \"myfeed\"\n"
                               "route[1].name \"bar\"\n"
                               "route[1].selector \"other\"\n"
                               "route[1].feed \"myfeed\"\n]")
                 .addRecipient("foo")
                 .addRecipient("bar"));

    frame.setMessage(make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::")));
    EXPECT_TRUE(frame.testSelect(StringList().add("foo")));

    mbus::Message::UP put = make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::")));
    frame.setMessage(std::move(put));
    EXPECT_TRUE(frame.testSelect( StringList().add("foo")));

    frame.setMessage(std::make_unique<RemoveDocumentMessage>(DocumentId("id:ns:testdoc::")));
    EXPECT_TRUE(frame.testSelect(StringList().add("foo")));

    frame.setMessage(make_unique<UpdateDocumentMessage>(
            make_shared<DocumentUpdate>(*_repo, *_docType, DocumentId("id:ns:testdoc::"))));
    EXPECT_TRUE(frame.testSelect(StringList().add("foo")));

    put = make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::")));
    frame.setMessage(std::move(put));
    EXPECT_TRUE(frame.testMergeOneReply("foo"));
}

TEST_F(PoliciesTest, test_document_route_selector_ignore)
{
    TestFrame frame(_repo);
    frame.setHop(mbus::HopSpec("test", "[DocumentRouteSelector:raw:"
                               "route[1]\n"
                               "route[0].name \"docproc/cluster.foo\"\n"
                               "route[0].selector \"testdoc and testdoc.stringfield == 'foo'\"\n"
                               "route[0].feed \"myfeed\"\n]")
                 .addRecipient("docproc/cluster.foo"));

    frame.setMessage(make_unique<PutDocumentMessage>(
            make_doc(DocumentId("id:yarn:testdoc:n=1234:fluff"))));
    std::vector<mbus::RoutingNode*> leaf;
    ASSERT_TRUE(frame.select(leaf, 0));
    mbus::Reply::UP reply = frame.getReceptor().getReply(TIMEOUT);
    ASSERT_TRUE(reply);
    EXPECT_EQ(uint32_t(DocumentProtocol::REPLY_DOCUMENTIGNORED), reply->getType());
    EXPECT_EQ(0u, reply->getNumErrors());

    frame.setMessage(make_unique<UpdateDocumentMessage>(
            make_shared<DocumentUpdate>(*_repo, *_docType, DocumentId("id:ns:testdoc::"))));
    EXPECT_TRUE(frame.testSelect(StringList().add("docproc/cluster.foo")));
}

namespace {

std::string
createDocumentRouteSelectorConfigWithTwoRoutes()
{
    return "[DocumentRouteSelector:raw:"
            "route[2]\n"
            "route[0].name \"testdoc-route\"\n"
            "route[0].selector \"testdoc and testdoc.stringfield != '0'\"\n"
            "route[0].feed \"\"\n"
            "route[1].name \"other-route\"\n"
            "route[1].selector \"other and other.intfield != '0'\"\n"
            "route[1].feed \"\"\n]";
}

std::unique_ptr<TestFrame>
createFrameWithTwoRoutes(const std::shared_ptr<const DocumentTypeRepo> & repo)
{
    auto result = std::make_unique<TestFrame>(repo);
    result->setHop(mbus::HopSpec("test", createDocumentRouteSelectorConfigWithTwoRoutes())
                           .addRecipient("testdoc-route").addRecipient("other-route"));
    return result;
}

std::unique_ptr<RemoveDocumentMessage>
makeRemove(std::string_view docId)
{
    return std::make_unique<RemoveDocumentMessage>(DocumentId(docId));
}

std::unique_ptr<GetDocumentMessage>
makeGet(std::string_view docId)
{
    return std::make_unique<GetDocumentMessage>(DocumentId(docId));
}

}

TEST_F(PoliciesTest, remove_document_messages_are_sent_to_the_route_handling_the_given_document_type)
{
    auto frame = createFrameWithTwoRoutes(_repo);

    frame->setMessage(makeRemove("id:ns:testdoc::1"));
    EXPECT_TRUE(frame->testSelect({"testdoc-route"}));

    frame->setMessage(makeRemove("id:ns:other::1"));
    EXPECT_TRUE(frame->testSelect({"other-route"}));
}

TEST_F(PoliciesTest, get_document_messages_are_sent_to_the_route_handling_the_given_document_type)
{
    auto frame = createFrameWithTwoRoutes(_repo);

    frame->setMessage(makeGet("id:ns:testdoc::1"));
    EXPECT_TRUE(frame->testSelect({"testdoc-route"}));

    frame->setMessage(makeGet("id:ns:other::1"));
    EXPECT_TRUE(frame->testSelect({"other-route"}));
}

namespace {
    string getDefaultDistributionConfig(
                    uint16_t redundancy = 2, uint16_t nodeCount = 10)
    {
        std::ostringstream ost;
        ost << "raw:redundancy " << redundancy << "\n"
            << "group[1]\n"
            << "group[0].index \"invalid\"\n"
            << "group[0].name \"invalid\"\n"
            << "group[0].partitions \"*\"\n"
            << "group[0].nodes[" << nodeCount << "]\n";
        for (uint16_t i=0; i<nodeCount; ++i) {
            ost << "group[0].nodes[" << i << "].index " << i << "\n";
        }
        return ost.str();
    }
}

TEST_F(PoliciesTest, test_load_balancer)
{
    LoadBalancer lb("foo", "");

    IMirrorAPI::SpecList entries;
    entries.emplace_back("foo/0/default", "tcp/bar:1");
    entries.emplace_back("foo/1/default", "tcp/bar:2");
    entries.emplace_back("foo/2/default", "tcp/bar:3");

    for (int i = 0; i < 99; i++) {
        std::pair<string, int> recipient = lb.getRecipient(entries);
        EXPECT_EQ((i % 3), recipient.second);
    }

    // Simulate that one node is overloaded. It returns busy twice as often as the others.
    for (int i = 0; i < 100; i++) {
        lb.received(0, true);
        lb.received(0, false);
        lb.received(0, false);
        lb.received(2, true);
        lb.received(2, false);
        lb.received(2, false);
        lb.received(1, true);
        lb.received(1, true);
        lb.received(1, false);
    }

    EXPECT_EQ(421, (int)(100 * lb.getWeight(0) / lb.getWeight(1)));
    EXPECT_EQ(421, (int)(100 * lb.getWeight(2) / lb.getWeight(1)));

    EXPECT_EQ(0 , lb.getRecipient(entries).second);
    EXPECT_EQ(0 , lb.getRecipient(entries).second);
    EXPECT_EQ(1 , lb.getRecipient(entries).second);
    EXPECT_EQ(2 , lb.getRecipient(entries).second);
    EXPECT_EQ(2 , lb.getRecipient(entries).second);
    EXPECT_EQ(2 , lb.getRecipient(entries).second);
    EXPECT_EQ(2 , lb.getRecipient(entries).second);
    EXPECT_EQ(0 , lb.getRecipient(entries).second);
    EXPECT_EQ(0 , lb.getRecipient(entries).second);
    EXPECT_EQ(0 , lb.getRecipient(entries).second);
}

TEST_F(PoliciesTest, require_that_content_policy_with_illegal_param_is_an_error_policy)
{
    EXPECT_TRUE(isErrorPolicy("Content", ""));
    EXPECT_TRUE(isErrorPolicy("Content", "config=foo;slobroks=foo"));
    EXPECT_TRUE(isErrorPolicy("Content", "slobroks=foo"));
}

TEST_F(PoliciesTest, require_that_content_policy_is_random_without_state)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));

    mbus::Slobrok slobrok;
    std::vector<mbus::TestServer*> servers;
    for (uint32_t i = 0; i < 5; ++i) {
        auto *srv = new mbus::TestServer(
                mbus::Identity(vespalib::make_string("storage/cluster.mycluster/distributor/%d", i)),
                mbus::RoutingSpec(), slobrok,
                std::make_shared<DocumentProtocol>(_repo));
        servers.push_back(srv);
        srv->net.registerSession("default");
    }
    string param = vespalib::make_string(
            "cluster=mycluster;slobroks=tcp/localhost:%d;clusterconfigid=%s;syncinit",
            slobrok.port(), getDefaultDistributionConfig(2, 5).c_str());
    WrappedPolicy wrapped_policy;
    ASSERT_NO_FATAL_FAILURE(setupContentPolicy(
            frame, param,
            "storage/cluster.mycluster/distributor/*/default", 5, wrapped_policy));
    auto& policy = wrapped_policy.value().get();
    ASSERT_FALSE(policy.getSystemState());

    std::set<string> lst;
    for (uint32_t i = 0; i < 666; i++) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());
        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
    }
    EXPECT_EQ(servers.size(), lst.size());
    for (auto & server : servers) {
        delete server;
    }
}

void
PoliciesTest::setupContentPolicy(TestFrame &frame, const string &param,
                                 const string &pattern, int32_t numEntries,
                                 WrappedPolicy& wrapped_policy)
{
    frame.setHop(mbus::HopSpec("test", vespalib::make_string("[Content:%s]", param.c_str())));
    mbus::MessageBus &mbus = frame.getMessageBus();
    const mbus::HopBlueprint *hop = mbus.getRoutingTable(DocumentProtocol::NAME)->getHop("test");
    const mbus::PolicyDirective & dir = dynamic_cast<const mbus::PolicyDirective&>(*hop->getDirective(0));
    ContentPolicy &policy = dynamic_cast<ContentPolicy&>(*mbus.getRoutingPolicy(DocumentProtocol::NAME, dir.getName(), dir.getParam()));
    policy.initSynchronous();
    ASSERT_NO_FATAL_FAILURE(assertMirrorReady(*policy.getMirror()));
    if (numEntries >= 0) {
        ASSERT_NO_FATAL_FAILURE(assertMirrorContains(*policy.getMirror(), pattern, numEntries));
    }
    wrapped_policy = std::ref(policy);
}

TEST_F(PoliciesTest, require_that_content_policy_is_targeted_with_state)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));

    mbus::Slobrok slobrok;
    std::vector<mbus::TestServer*> servers;
    for (uint32_t i = 0; i < 5; ++i) {
        auto *srv = new mbus::TestServer(
                mbus::Identity(vespalib::make_string("storage/cluster.mycluster/distributor/%d", i)),
                mbus::RoutingSpec(), slobrok,
                make_shared<DocumentProtocol>(_repo));
        servers.push_back(srv);
        srv->net.registerSession("default");
    }
    string param = vespalib::make_string(
            "cluster=mycluster;slobroks=tcp/localhost:%d;clusterconfigid=%s;syncinit",
            slobrok.port(), getDefaultDistributionConfig(2, 5).c_str());
    WrappedPolicy wrapped_policy;
    ASSERT_NO_FATAL_FAILURE(setupContentPolicy(frame, param, "storage/cluster.mycluster/distributor/*/default", 5, wrapped_policy));
    auto& policy = wrapped_policy.value().get();
    ASSERT_FALSE(policy.getSystemState());
    {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        leaf[0]->handleReply(std::make_unique<WrongDistributionReply>("distributor:5 storage:5"));
        ASSERT_TRUE(policy.getSystemState());
        EXPECT_EQ(policy.getSystemState()->toString(), "distributor:5 storage:5");
    }
    std::set<string> lst;
    for (int i = 0; i < 666; i++) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());
        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
    }
    EXPECT_EQ(1u, lst.size());
    for (auto & server : servers) {
        delete server;
    }
}

TEST_F(PoliciesTest, require_that_content_policy_combines_system_and_slobrok_state)
{
    TestFrame frame(_repo);
    frame.setMessage(newPutDocumentMessage("id:ns:testdoc::"));

    mbus::Slobrok slobrok;
    mbus::TestServer server(mbus::Identity("storage/cluster.mycluster/distributor/0"),
                            mbus::RoutingSpec(), slobrok,
                            make_shared<DocumentProtocol>(_repo));
    server.net.registerSession("default");

    string param = vespalib::make_string(
            "cluster=mycluster;slobroks=tcp/localhost:%d;clusterconfigid=%s;syncinit",
            slobrok.port(), getDefaultDistributionConfig(2, 5).c_str());
    WrappedPolicy wrapped_policy;
    ASSERT_NO_FATAL_FAILURE(setupContentPolicy(
            frame, param,
            "storage/cluster.mycluster/distributor/*/default", 1, wrapped_policy));
    auto& policy = wrapped_policy.value().get();
    ASSERT_FALSE(policy.getSystemState());
    {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        leaf[0]->handleReply(std::make_unique<WrongDistributionReply>("distributor:99 storage:99"));
        ASSERT_TRUE(policy.getSystemState());
        EXPECT_EQ(policy.getSystemState()->toString(), "distributor:99 storage:99");
    }
    for (int i = 0; i < 666; i++) {
        ASSERT_TRUE(frame.testSelect(StringList().add(server.net.getConnectionSpec() + "/default")));
    }
}

TEST_F(PoliciesTest, test_subset_service)
{
    // Prepare message.
    TestFrame frame(_repo, "docproc/cluster.default");
    frame.setMessage(make_unique<PutDocumentMessage>(make_doc(DocumentId("id:ns:testdoc::"))));

    // Test requerying for adding nodes.
    frame.setHop(mbus::HopSpec("test", "docproc/cluster.default/[SubsetService:2]/chain.default"));
    std::set<string> lst;
    for (uint32_t i = 1; i <= 10; ++i) {
        frame.getNetwork().registerSession(vespalib::make_string("%d/chain.default", i));
        ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", i));

        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        lst.insert(leaf[0]->getRoute().toString());
        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }
    ASSERT_TRUE(lst.size() > 1); // must have requeried

    // Test load balancing.
    string prev = "";
    for (uint32_t i = 1; i <= 10; ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));

        string next = leaf[0]->getRoute().toString();
        if (prev.empty()) {
            ASSERT_TRUE(!next.empty());
        } else {
            ASSERT_TRUE(prev != next);
        }

        prev = next;
        leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }

    // Test requerying for dropping nodes.
    lst.clear();
    for (uint32_t i = 1; i <= 10; ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        ASSERT_TRUE(frame.select(leaf, 1));
        string route = leaf[0]->getRoute().toString();
        lst.insert(route);

        frame.getNetwork().unregisterSession(route.substr(frame.getIdentity().length() + 1));
        ASSERT_TRUE(frame.waitSlobrok("docproc/cluster.default/*/chain.default", 10 - i));

        auto reply = std::make_unique<mbus::EmptyReply>();
        reply->addError(mbus::Error(mbus::ErrorCode::NO_ADDRESS_FOR_SERVICE, route));
        leaf[0]->handleReply(std::move(reply));
        ASSERT_TRUE(frame.getReceptor().getReply(TIMEOUT));
    }
    EXPECT_EQ(10u, lst.size());

    // Test merge behavior.
    frame.setHop(mbus::HopSpec("test", "[SubsetService]"));
    EXPECT_TRUE(frame.testMergeOneReply("*"));
}

TEST_F(PoliciesTest, test_subset_service_cache)
{
    TestFrame fooFrame(_repo, "docproc/cluster.default");
    mbus::HopSpec fooHop("foo", "docproc/cluster.default/[SubsetService:2]/chain.foo");
    fooFrame.setMessage(std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::foo")));
    fooFrame.setHop(fooHop);

    TestFrame barFrame(fooFrame);
    mbus::HopSpec barHop("bar", "docproc/cluster.default/[SubsetService:2]/chain.bar");
    barFrame.setMessage(std::make_unique<GetDocumentMessage>(DocumentId("id:ns:testdoc::bar")));
    barFrame.setHop(barHop);

    fooFrame.getMessageBus().setupRouting(
            mbus::RoutingSpec().addTable(mbus::RoutingTableSpec(DocumentProtocol::NAME)
                                         .addHop(std::move(fooHop))
                                         .addHop(std::move(barHop))));

    fooFrame.getNetwork().registerSession("0/chain.foo");
    fooFrame.getNetwork().registerSession("0/chain.bar");
    ASSERT_TRUE(fooFrame.waitSlobrok("docproc/cluster.default/0/*", 2));

    std::vector<mbus::RoutingNode*> fooSelected;
    ASSERT_TRUE(fooFrame.select(fooSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.foo", fooSelected[0]->getRoute().getHop(0).toString());

    std::vector<mbus::RoutingNode*> barSelected;
    ASSERT_TRUE(barFrame.select(barSelected, 1));
    EXPECT_EQ("docproc/cluster.default/0/chain.bar", barSelected[0]->getRoute().getHop(0).toString());

    barSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());
    fooSelected[0]->handleReply(std::make_unique<mbus::EmptyReply>());

    ASSERT_TRUE(barFrame.getReceptor().getReply(TIMEOUT));
    ASSERT_TRUE(fooFrame.getReceptor().getReply(TIMEOUT));
}

bool
PoliciesTest::trySelect(TestFrame &frame, uint32_t numSelects, const std::vector<string> &expected) {
    std::set<string> lst;
    for (uint32_t i = 0; i < numSelects; ++i) {
        std::vector<mbus::RoutingNode*> leaf;
        if (!expected.empty()) {
            frame.select(leaf, 1);
            lst.insert(leaf[0]->getRoute().toString());
            leaf[0]->handleReply(std::make_unique<mbus::EmptyReply>());
        } else {
            frame.select(leaf, 0);
        }
        if( ! frame.getReceptor().getReply(TIMEOUT)) {
            LOG(error, "Reply failed to propagate to reply handler.");
            return false;
        }
    }
    if (expected.size() != lst.size()) {
        LOG(error, "Expected %d recipients, got %d.", (uint32_t)expected.size(), (uint32_t)lst.size());
        return false;
    }
    auto it = lst.begin();
    for (uint32_t i = 0; i < expected.size(); ++i, ++it) {
        if (*it != expected[i]) {
            LOG(error, "Expected '%s', got '%s'.", expected[i].c_str(), it->c_str());
            return false;
        }
    }
    return true;
}

bool
PoliciesTest::isErrorPolicy(const string &name, const string &param)
{
    DocumentProtocol protocol(_repo);
    mbus::IRoutingPolicy::UP policy = protocol.createPolicy(name, param);

    return policy && dynamic_cast<ErrorPolicy*>(policy.get()) != nullptr;
}

GTEST_MAIN_RUN_ALL_TESTS()

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.prelude.fastsearch;

import com.yahoo.prelude.fastsearch.PartialSummaryHandler;
import com.yahoo.prelude.querytransform.QueryRewrite;
import com.yahoo.search.Query;
import com.yahoo.search.Result;
import com.yahoo.search.dispatch.Dispatcher;
import com.yahoo.search.dispatch.FillInvoker;
import com.yahoo.search.dispatch.SearchInvoker;
import com.yahoo.search.grouping.GroupingRequest;
import com.yahoo.search.grouping.request.GroupingOperation;
import com.yahoo.search.query.Ranking;
import com.yahoo.search.result.ErrorMessage;
import com.yahoo.search.result.Hit;
import com.yahoo.search.result.HitGroup;

import java.io.IOException;
import java.util.Optional;

/**
 * The searcher which forwards queries to fdispatch nodes, using the fnet/fs4
 * network layer.
 *
 * @author  bratseth
 */
// TODO: Clean up all the duplication in the various search methods by
// switching to doing all the error handling using exceptions below doSearch2.
// Right now half is done by exceptions handled in doSearch2 and half by setting
// errors on results and returning them. It could be handy to create a QueryHandlingErrorException
// or similar which could wrap an error message, and then just always throw that and
// catch and unwrap into a results with an error in high level methods.  -Jon
public class IndexedBackend extends VespaBackend {

    /** Used to dispatch directly to search nodes over RPC, replacing the old fnet communication path */
    private final Dispatcher dispatcher;

    /**
     * Creates a Fastsearcher.
     *
     * @param dispatcher the dispatcher used (when enabled) to send summary requests over the rpc protocol.
     *                   Eventually we will move everything to this protocol and never use dispatch nodes.
     *                   At that point we won't need a cluster searcher above this to select and pass the right
     *                   backend.
     * @param clusterParams the cluster number, and other cluster backend parameters
     */
    public IndexedBackend(ClusterParams clusterParams, Dispatcher dispatcher)
    {
        super(clusterParams);
        this.dispatcher = dispatcher;
    }

    @Override
    protected void transformQuery(Query query) {
        QueryRewrite.rewriteSddocname(query);
    }

    private void injectSource(HitGroup hits) {
        for (Hit hit : hits.asUnorderedHits()) {
            if (hit instanceof FastHit) {
                hit.setSource(getName());
            }
        }
    }

    @Override
    public Result doSearch2(String schema, Query query) {
        if (dispatcher.allGroupsHaveSize1())
            forceSinglePassGrouping(query);
        try (SearchInvoker invoker = getSearchInvoker(query)) {
            Result result = invoker.search(query);
            injectSource(result.hits());

            if (query.properties().getBoolean(Ranking.RANKFEATURES, false)) {
                // There is currently no correct choice for which
                // summary class we want to fetch at this point. If we
                // fetch the one selected by the user it may not
                // contain the data we need. If we fetch the default
                // one we end up fetching docsums twice unless the
                // user also requested the default one.
                fill(result, PartialSummaryHandler.resolveSummaryClass(result)); // ARGH
            }
            return result;
        } catch (TimeoutException e) {
            return new Result(query,ErrorMessage.createTimeout(e.getMessage()));
        } catch (IOException e) {
            Result result = new Result(query);
            if (query.getTrace().getLevel() >= 1)
                query.trace(getName() + " error response: " + result, false, 1);
            result.hits().addError(ErrorMessage.createBackendCommunicationError(getName() + " failed: "+ e.getMessage()));
            return result;
        }
    }

    /**
     * Perform a partial docsum fill for a temporary result
     * representing a partition of the complete fill request.
     *
     * @param result result containing a partition of the unfilled hits
     * @param summaryClass the summary class we want to fill with
     **/
    @Override
    protected void doPartialFill(Result result, String summaryClass) {
        if (result.isFilled(summaryClass)) return;

        Query query = result.getQuery();
        traceQuery(getName(), DispatchPhase.FILL, query, query.getOffset(), query.getHits(), 1, quotedSummaryClass(query, summaryClass));

        try (FillInvoker invoker = getFillInvoker(result)) {
            invoker.fill(result, summaryClass);
        }
    }

    /** When we only search a single node, doing all grouping in one pass is more efficient */
    private void forceSinglePassGrouping(Query query) {
        for (GroupingRequest groupingRequest : query.getSelect().getGrouping())
            forceSinglePassGrouping(groupingRequest.getRootOperation());
    }

    private void forceSinglePassGrouping(GroupingOperation operation) {
        operation.setForceSinglePass(true);
        for (GroupingOperation childOperation : operation.getChildren())
            forceSinglePassGrouping(childOperation);
    }

    /**
     * Returns an invocation object for use in a single search request. The specific implementation returned
     * depends on query properties with the default being an invoker that interfaces with a dispatcher
     * on the same host.
     */
    private SearchInvoker getSearchInvoker(Query query) {
        return dispatcher.getSearchInvoker(query, this);
    }

    /**
     * Returns an invocation object for use in a single fill request. The specific implementation returned
     * depends on query properties with the default being an invoker that uses RPC to interface with
     * content nodes.
     */
    private FillInvoker getFillInvoker(Result result) {
        return dispatcher.getFillInvoker(result, this);
    }

    private static Optional<String> quotedSummaryClass(Query q, String summaryClass) {
        return Optional.of(PartialSummaryHandler
                           .quotedSummaryClassName(summaryClass,
                                                   q.getPresentation().getSummaryFields()));
    }

    public String toString() {
        return "fast searcher (" + getName() + ") ";
    }

}

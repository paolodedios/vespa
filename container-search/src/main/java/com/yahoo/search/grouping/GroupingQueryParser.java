// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.search.grouping;

import com.yahoo.api.annotations.Beta;
import com.yahoo.component.chain.dependencies.After;
import com.yahoo.component.chain.dependencies.Before;
import com.yahoo.component.chain.dependencies.Provides;
import com.yahoo.processing.IllegalInputException;
import com.yahoo.processing.request.CompoundName;
import com.yahoo.search.Query;
import com.yahoo.search.Result;
import com.yahoo.search.Searcher;
import com.yahoo.search.grouping.request.GroupingOperation;
import com.yahoo.search.query.Select;
import com.yahoo.search.searchchain.Execution;
import com.yahoo.search.searchchain.PhaseNames;

import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.TimeZone;

/**
 * This searcher is responsible for turning the "select" parameter into a corresponding {@link GroupingRequest}. It will
 * also parse any "timezone" parameter as the timezone for time expressions such as {@link
 * com.yahoo.search.grouping.request.DayOfMonthFunction} and {@link com.yahoo.search.grouping.request.HourOfDayFunction}.
 *
 * @author Simon Thoresen Hult
 */
@After(PhaseNames.RAW_QUERY)
@Before(PhaseNames.TRANSFORMED_QUERY)
@Provides(GroupingQueryParser.SELECT_PARAMETER_PARSING)
public class GroupingQueryParser extends Searcher {

    public static final String SELECT_PARAMETER_PARSING = "SelectParameterParsing";
    public static final CompoundName PARAM_CONTINUE = CompoundName.from("continue");
    public static final CompoundName PARAM_REQUEST = CompoundName.from(Select.SELECT);
    public static final CompoundName PARAM_TIMEZONE = CompoundName.from("timezone");
    @Beta public static final CompoundName PARAM_DEFAULT_MAX_HITS = CompoundName.from("grouping.defaultMaxHits");
    @Beta public static final CompoundName PARAM_DEFAULT_MAX_GROUPS = CompoundName.from("grouping.defaultMaxGroups");
    @Beta public static final CompoundName PARAM_DEFAULT_PRECISION_FACTOR = CompoundName.from("grouping.defaultPrecisionFactor");
    @Beta public static final CompoundName GROUPING_GLOBAL_MAX_GROUPS = CompoundName.from("grouping.globalMaxGroups");
    private static final ThreadLocal<ZoneCache> zoneCache = new ThreadLocal<>();

    @Override
    public Result search(Query query, Execution execution) {
        try {
            validate(query);

            String reqParam = query.properties().getString(PARAM_REQUEST);
            if (reqParam == null) return execution.search(query);

            List<Continuation> continuations = getContinuations(query.properties().getString(PARAM_CONTINUE));
            for (GroupingOperation operation : GroupingOperation.fromStringAsList(reqParam))
                createGroupingRequestIn(query, operation, continuations);
            return execution.search(query);
        }
        catch (IllegalArgumentException e) {
            throw new IllegalInputException(e);
        }
    }

    public static void validate(Query query) {
        if (query.getHttpRequest().getProperty(GROUPING_GLOBAL_MAX_GROUPS.toString()) != null)
            throw new IllegalInputException(GROUPING_GLOBAL_MAX_GROUPS + " must be specified in a query profile.");
    }

    public static void createGroupingRequestIn(Query query, GroupingOperation operation, List<Continuation> continuations) {
        GroupingRequest request = GroupingRequest.newInstance(query);
        request.setRootOperation(operation);
        request.setTimeZone(getTimeZone(query.properties().getString(PARAM_TIMEZONE, "utc")));
        request.continuations().addAll(continuations);
    }

    private List<Continuation> getContinuations(String param) {
        if (param == null) {
            return List.of();
        }
        List<Continuation> ret = new LinkedList<>();
        for (String str : param.split(" ")) {
            ret.add(Continuation.fromString(str));
        }
        return ret;
    }

    private static TimeZone getTimeZone(String name) {
        ZoneCache cache = zoneCache.get();
        if (cache == null) {
            cache = new ZoneCache();
            zoneCache.set(cache);
        }
        TimeZone timeZone = cache.get(name);
        if (timeZone == null) {
            timeZone = TimeZone.getTimeZone(name);
            cache.put(name, timeZone);
        }
        return timeZone;
    }

    private static class ZoneCache extends LinkedHashMap<String, TimeZone> {

        ZoneCache() {
            super(16, 0.75f, true);
        }

        @Override
        protected boolean removeEldestEntry(Map.Entry<String, TimeZone> entry) {
            return size() > 128; // large enough to cache common cases
        }
    }
}

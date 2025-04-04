// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.jdisc.http.server.jetty;

import com.yahoo.jdisc.handler.CompletionHandler;

import java.util.List;


/**
 * Various utilities for {@link CompletionHandler}.
 *
 * @author Simon Thoresen Hult
 * @author bjorncs
 */
class CompletionHandlers {

    private CompletionHandlers() {}

    private static final CompletionHandler NOOP_COMPLETION_HANDLER = new CompletionHandler() {
        @Override public void completed() {}
        @Override public void failed(final Throwable t) {}
    };

    static CompletionHandler noop() { return NOOP_COMPLETION_HANDLER; }

    public static void tryComplete(CompletionHandler handler) {
        if (handler == null) {
            return;
        }
        try {
            handler.completed();
        } catch (Exception e) {
            // ignore
        }
    }

    public static void tryFail(CompletionHandler handler, Throwable t) {
        if (handler == null) {
            return;
        }
        try {
            handler.failed(t);
        } catch (Exception e) {
            // ignore
        }
    }

    public static CompletionHandler wrap(CompletionHandler... handlers) {
        return wrap(List.of(handlers));
    }

    public static CompletionHandler wrap(final Iterable<CompletionHandler> handlers) {
        return new CompletionHandler() {

            @Override
            public void completed() {
                for (CompletionHandler handler : handlers) {
                    tryComplete(handler);
                }
            }

            @Override
            public void failed(Throwable t) {
                for (CompletionHandler handler : handlers) {
                    tryFail(handler, t);
                }
            }
        };
    }
}

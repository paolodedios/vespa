// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

package com.yahoo.schema.parser;

import com.yahoo.schema.document.Sorting.Function;
import com.yahoo.schema.document.Sorting.Strength;

import java.util.Optional;

/**
 * This class holds the extracted information after parsing a "sorting"
 * block, using simple data structures as far as possible.  Do not put
 * advanced logic here!
 * @author arnej27959
 **/
public class ParsedSorting extends ParsedBlock {

    private boolean ascending = true;
    private Function sortFunction = null;
    private Strength sortStrength = null;
    private String sortLocale = null;

    ParsedSorting(String blockName, String blockType) {
        super(blockName, blockType);
    }

    boolean getAscending() { return this.ascending; }
    boolean getDescending() { return ! this.ascending; }
    Optional<Function> getFunction() { return Optional.ofNullable(sortFunction); }
    Optional<Strength> getStrength() { return Optional.ofNullable(sortStrength); }
    Optional<String> getLocale() { return Optional.ofNullable(sortLocale); }

    public void setAscending() { this.ascending = true; }

    public void setDescending() { this.ascending = false; }

    public void setLocale(String value) {
        verifyThat(sortLocale == null, "sorting already has locale", sortLocale);
        this.sortLocale = value;
    }
    public void setFunction(Function value) {
        verifyThat(sortFunction == null, "sorting already has function", sortFunction);
        this.sortFunction = value;
    }
    public void setStrength(Strength value) {
        verifyThat(sortStrength == null, "sorting already has strength", sortStrength);
        this.sortStrength = value;
    }
}

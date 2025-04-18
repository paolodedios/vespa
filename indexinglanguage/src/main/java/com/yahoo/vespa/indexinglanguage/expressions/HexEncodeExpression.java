// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.DataType;
import com.yahoo.document.datatypes.LongFieldValue;
import com.yahoo.document.datatypes.StringFieldValue;

/**
 * @author Simon Thoresen Hult
 */
public final class HexEncodeExpression extends Expression {

    @Override
    public DataType setInputType(DataType inputType, TypeContext context) {
        super.setInputType(inputType, DataType.LONG, context);
        return DataType.STRING;
    }

    @Override
    public DataType setOutputType(DataType outputType, TypeContext context) {
        super.setOutputType(DataType.STRING, outputType, null, context);
        return DataType.LONG;
    }

    @Override
    protected void doExecute(ExecutionContext context) {
        long input = ((LongFieldValue) context.getCurrentValue()).getLong();
        context.setCurrentValue(new StringFieldValue(Long.toHexString(input)));
    }

    @Override
    public String toString() { return "hexencode"; }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof HexEncodeExpression)) return false;
        return true;
    }

    @Override
    public int hashCode() {
        return getClass().hashCode();
    }

}

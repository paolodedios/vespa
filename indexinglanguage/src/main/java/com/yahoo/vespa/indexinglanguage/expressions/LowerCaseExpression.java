// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.DataType;
import com.yahoo.document.datatypes.StringFieldValue;

import static com.yahoo.language.LinguisticsCase.toLowerCase;

/**
 * @author Simon Thoresen Hult
 */
public final class LowerCaseExpression extends Expression {

    @Override
    public DataType setInputType(DataType inputType, TypeContext context) {
        return super.setInputType(inputType, DataType.STRING, context);
    }

    @Override
    public DataType setOutputType(DataType outputType, TypeContext context) {
        return super.setOutputType(DataType.STRING, outputType, null, context);
    }

    @Override
    protected void doExecute(ExecutionContext context) {
        context.setCurrentValue(new StringFieldValue(toLowerCase(String.valueOf(context.getCurrentValue()))));
    }

    @Override
    public String toString() { return "lowercase"; }

    @Override
    public boolean equals(Object obj) {
        return (obj instanceof LowerCaseExpression);
    }

    @Override
    public int hashCode() {
        return getClass().hashCode();
    }

}

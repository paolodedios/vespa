// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.DataType;
import com.yahoo.document.datatypes.FieldValue;
import com.yahoo.document.datatypes.LongFieldValue;
import com.yahoo.document.datatypes.StringFieldValue;
import com.yahoo.text.StringUtilities;

import java.util.Objects;

/**
 * @author Simon Thoresen Hult
 */
public final class ConstantExpression extends Expression {

    private final FieldValue value;

    public ConstantExpression(FieldValue value) {
        super(null);
        this.value = Objects.requireNonNull(value);
    }

    public FieldValue getValue() {
        return value;
    }

    @Override
    protected void doExecute(ExecutionContext context) {
        context.setValue(value);
    }

    @Override
    protected void doVerify(VerificationContext context) {
        context.setValueType(value.getDataType());
    }

    @Override
    public DataType createdOutputType() {
        return value.getDataType();
    }

    @Override
    public String toString() {
        if (value instanceof StringFieldValue) {
            return "\"" + StringUtilities.escape(value.toString(), '"') + "\"";
        }
        if (value instanceof LongFieldValue) {
            return value + "L";
        }
        return value.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof ConstantExpression rhs)) return false;
        if (!value.equals(rhs.value)) return false;
        return true;
    }

    @Override
    public int hashCode() {
        return getClass().hashCode() + value.hashCode();
    }

}
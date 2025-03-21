// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.ArrayDataType;
import com.yahoo.document.DataType;
import com.yahoo.document.DocumentType;
import com.yahoo.document.Field;
import com.yahoo.document.MapDataType;
import com.yahoo.document.StructDataType;
import com.yahoo.document.WeightedSetDataType;
import com.yahoo.document.datatypes.Array;
import com.yahoo.document.datatypes.FieldValue;
import com.yahoo.document.datatypes.MapFieldValue;
import com.yahoo.document.datatypes.Struct;
import com.yahoo.document.datatypes.WeightedSet;
import com.yahoo.vespa.indexinglanguage.ExpressionConverter;
import com.yahoo.vespa.indexinglanguage.FieldValueConverter;
import com.yahoo.vespa.objects.ObjectOperation;
import com.yahoo.vespa.objects.ObjectPredicate;

import java.util.Map;
import java.util.Objects;

/**
 * @author Simon Thoresen Hult
 */
public final class ForEachExpression extends CompositeExpression {

    private final Expression expression;

    public ForEachExpression(Expression expression) {
        this.expression = Objects.requireNonNull(expression);
    }

    @Override
    public boolean isMutating() { return expression.isMutating(); }

    public Expression getInnerExpression() { return expression; }

    @Override
    public ForEachExpression convertChildren(ExpressionConverter converter) {
        Expression converted = converter.convert(expression);
        return converted != null ?  new ForEachExpression(converted) : null;
    }

    @Override
    public void setStatementOutput(DocumentType documentType, Field field) {
        expression.setStatementOutput(documentType, field);
    }

    @Override
    public DataType setInputType(DataType inputType, TypeContext context) {
        super.setInputType(inputType, context);
        if (inputType == null) return null;

        if (inputType instanceof ArrayDataType || inputType instanceof WeightedSetDataType) {
            // Value type outside block becomes the collection type having the block output type as argument
            return withInnerType(expression.setInputType(inputType.getNestedType(), context), inputType);
        }
        else if (inputType instanceof StructDataType struct) {
            return verifyStructFields(struct, context);
        }
        if (inputType instanceof MapDataType) {
            // Inner value will be MapEntryFieldValue which has the same type as the map
            DataType outputType = expression.setInputType(inputType, context);
            if (outputType == null) return getOutputType(context);
            return DataType.getArray(outputType);
        }
        else {
            throw new VerificationException(this, "Expected Array, Struct, WeightedSet or Map input, got " +
                                                  inputType.getName());
        }
    }

    @Override
    public DataType setOutputType(DataType outputType, TypeContext context) {
        if (outputType == null) return null;
        super.setOutputType(outputType, context);

        if (outputType instanceof ArrayDataType || outputType instanceof WeightedSetDataType) {
            DataType innerInputType = expression.setOutputType(outputType.getNestedType(), context);
            if (innerInputType instanceof MapDataType mapDataType) // A map converted to an array of entries
                return mapDataType;
            else
                return withInnerType(innerInputType, outputType);
        }
        else if (outputType instanceof StructDataType struct) {
            return verifyStructFields(struct, context);
        }
        else if (outputType instanceof AnyDataType) {
            return outputType;
        }
        else {
            throw new VerificationException(this, "Expected Array, Struct, WeightedSet or Map input, got " +
                                                  outputType.getName());
        }
    }

    private DataType withInnerType(DataType innerType, DataType collectionType) {
        if (innerType == null) return null;
        if (collectionType instanceof WeightedSetDataType wset)
            return DataType.getWeightedSet(innerType, wset.createIfNonExistent(), wset.removeIfZero());
        else
            return DataType.getArray(innerType);
    }

    /**
     * Verifies that each struct field is compatible with the expression.
     * This is symmetric in both verification directions since the expression just need to be compatible with
     * all the struct fields.
     */
    private DataType verifyStructFields(StructDataType struct, TypeContext context) {
        for (Field field : struct.getFields()) {
            DataType fieldType = field.getDataType();
            DataType fieldOutputType = expression.setInputType(fieldType, context);
            if (fieldOutputType != null && ! fieldOutputType.isAssignableTo(fieldType))
                throw new VerificationException(this, "Struct field '" + field.getName() + "' has type " + fieldType.getName() +
                                                      " but expression produces " + fieldOutputType.getName());
            DataType fieldInputType = expression.setOutputType(fieldType, context);
            if (fieldOutputType != null && ! fieldType.isAssignableTo(fieldInputType))
                throw new VerificationException(this, "Struct field '" + field.getName() + "' has type " + fieldType.getName() +
                                                      " but expression requires " + fieldInputType.getName());
            if (fieldOutputType == null && fieldInputType == null)
                return null; // Neither direction could be inferred
        }
        return struct;
    }

    @Override
    protected void doExecute(ExecutionContext context) {
        FieldValue input = context.getCurrentValue();
        if (input instanceof Array || input instanceof WeightedSet) {
            FieldValue next = new ExecutionConverter(context, expression).convert(input);
            if (next == null)
                next = getOutputType().createFieldValue();
            context.setCurrentValue(next);
        } else if (input instanceof Struct || input instanceof Map) {
            context.setCurrentValue(new ExecutionConverter(context, expression).convert(input));
        } else {
            throw new IllegalArgumentException("Expected Array, Struct, WeightedSet or Map input, got " +
                                               input.getDataType().getName());
        }
    }

    @Override
    public String toString() {
        return "for_each { " + expression + " }";
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof ForEachExpression rhs)) return false;
        if (!expression.equals(rhs.expression)) return false;
        return true;
    }

    @Override
    public int hashCode() {
        return getClass().hashCode() + expression.hashCode();
    }

    /** Converts field values by executing the given expression on them. */
    private static final class ExecutionConverter extends FieldValueConverter {

        final ExecutionContext context;
        final Expression expression;
        int depth = 0;

        ExecutionConverter(ExecutionContext context, Expression expression) {
            this.context = context;
            this.expression = expression;
        }

        @Override
        protected boolean shouldConvert(FieldValue value) {
            return ++depth > 1;
        }

        /** Converts a map into an array by passing each entry through the expression. */
        @Override
        protected FieldValue convertMap(MapFieldValue<FieldValue, FieldValue> map) {
            var values = new Array<>(new ArrayDataType(expression.getOutputType()), map.size());
            for (var entry : map.entrySet())
                values.add(doConvert(new MapEntryFieldValue(entry.getKey(), entry.getValue())));
            return values;
        }

        @Override
        protected FieldValue doConvert(FieldValue value) {
            context.setCurrentValue(value).execute(expression);
            return context.getCurrentValue();
        }
    }

    @Override
    public void selectMembers(ObjectPredicate predicate, ObjectOperation operation) {
        select(expression, predicate, operation);
    }

}

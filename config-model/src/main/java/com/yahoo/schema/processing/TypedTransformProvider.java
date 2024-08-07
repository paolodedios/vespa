// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.processing;

import com.yahoo.document.DataType;
import com.yahoo.document.Field;
import com.yahoo.schema.Schema;
import com.yahoo.schema.document.Attribute;
import com.yahoo.vespa.documentmodel.SummaryField;
import com.yahoo.vespa.indexinglanguage.ValueTransformProvider;
import com.yahoo.vespa.indexinglanguage.expressions.AttributeExpression;
import com.yahoo.vespa.indexinglanguage.expressions.Expression;
import com.yahoo.vespa.indexinglanguage.expressions.IndexExpression;
import com.yahoo.vespa.indexinglanguage.expressions.OutputExpression;
import com.yahoo.vespa.indexinglanguage.expressions.SummaryExpression;

/**
 * @author Simon Thoresen Hult
 */
public abstract class TypedTransformProvider extends ValueTransformProvider {

    private final Schema schema;
    private DataType fieldType;

    TypedTransformProvider(Class<? extends Expression> transformClass, Schema schema) {
        super(transformClass);
        this.schema = schema;
    }

    @Override
    protected final boolean requiresTransform(Expression exp) {
        if (exp instanceof OutputExpression) {
            String fieldName = ((OutputExpression)exp).getFieldName();
            if (fieldName == null) {
                // Incomplete output expressions never require a transform.
                return false;
            }
            if (exp instanceof AttributeExpression) {
                Attribute attribute = schema.getAttribute(fieldName);
                if (attribute == null)
                    throw new IllegalArgumentException("Attribute '" + fieldName + "' not found.");
                fieldType = attribute.getDataType();
            }
            else if (exp instanceof IndexExpression) {
                Field field = schema.getConcreteField(fieldName);
                if (field == null)
                    throw new IllegalArgumentException("Index field '" + fieldName + "' not found.");
                fieldType = field.getDataType();
            }
            else if (exp instanceof SummaryExpression) {
                SummaryField field = schema.getSummaryField(fieldName);
                if (field == null) {
                    // Use document field if summary field is not found
                    var sdField = schema.getConcreteField(fieldName);
                    if (sdField != null && sdField.doesSummarying()) {
                        fieldType = sdField.getDataType();
                    } else {
                        throw new IllegalArgumentException("Summary field '" + fieldName + "' not found.");
                    }
                } else {
                    fieldType = field.getDataType();
                }
            }
            else {
                throw new UnsupportedOperationException();
            }
        }
        return requiresTransform(exp, fieldType);
    }

    @Override
    protected final Expression newTransform() {
        return newTransform(fieldType);
    }

    protected abstract boolean requiresTransform(Expression exp, DataType fieldType);

    protected abstract Expression newTransform(DataType fieldType);

}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.processing;

import com.yahoo.config.application.api.DeployLogger;
import com.yahoo.document.ArrayDataType;
import com.yahoo.document.DataType;
import com.yahoo.document.WeightedSetDataType;
import com.yahoo.schema.RankProfileRegistry;
import com.yahoo.schema.Schema;
import com.yahoo.schema.document.ComplexAttributeFieldUtils;
import com.yahoo.schema.document.ImmutableSDField;
import com.yahoo.vespa.documentmodel.DocumentSummary;
import com.yahoo.vespa.documentmodel.SummaryElementsSelector;
import com.yahoo.vespa.documentmodel.SummaryField;
import com.yahoo.vespa.model.container.search.QueryProfiles;

import static com.yahoo.schema.document.ComplexAttributeFieldUtils.isSupportedComplexField;

/**
 * Iterates all summary fields with 'matched-elements-only' or 'select-elements-by' and validates that the field type is supported.
 *
 * @author geirst
 */
public class SummaryElementsSelectorValidator extends Processor {

    public SummaryElementsSelectorValidator(Schema schema, DeployLogger deployLogger, RankProfileRegistry rankProfileRegistry, QueryProfiles queryProfiles) {
        super(schema, deployLogger, rankProfileRegistry, queryProfiles);
    }

    @Override
    public void process(boolean validate, boolean documentsOnly) {
        for (var entry : schema.getSummaries().entrySet()) {
            var summary = entry.getValue();
            for (var field : summary.getSummaryFields().values()) {
                if (field.getElementsSelector().getSelect() == SummaryElementsSelector.Select.BY_MATCH ||
                    field.getElementsSelector().getSelect() == SummaryElementsSelector.Select.BY_SUMMARY_FEATURE) {
                    processSummaryField(summary, field, validate);
                }
            }
        }
    }

    private void processSummaryField(DocumentSummary summary, SummaryField field, boolean validate) {
        var sourceField = schema.getField(field.getSingleSource());
        if (sourceField != null) {
            if (!isSupportedComplexField(sourceField) && !isSupportedMultiValueField(sourceField) && validate) {
                String selectMsg = "matched-elements-only";
                if (field.getElementsSelector().getSelect() == SummaryElementsSelector.Select.BY_SUMMARY_FEATURE) {
                    selectMsg = "select-elements-by";
                }
                fail(summary, field, "'" + selectMsg + "' is not supported for this field type. " +
                        "Supported field types are: array of primitive, weighted set of primitive, " +
                        "array of simple struct, map of primitive type to simple struct, " +
                        "and map of primitive type to primitive type");
            }
        }
        // else case is handled in SummaryFieldsMustHaveValidSource
    }

    private boolean isSupportedMultiValueField(ImmutableSDField sourceField) {
        var type = sourceField.getDataType();
        return (isArrayOfPrimitiveType(type) || isWeightedsetOfPrimitiveType(type));
    }

    private boolean isArrayOfPrimitiveType(DataType type) {
        if (type instanceof ArrayDataType) {
            var arrayType = (ArrayDataType) type;
            return ComplexAttributeFieldUtils.isPrimitiveType(arrayType.getNestedType());
        }
        return false;
    }

    private boolean isWeightedsetOfPrimitiveType(DataType type) {
        if (type instanceof WeightedSetDataType) {
            var wsetType = (WeightedSetDataType) type;
            return ComplexAttributeFieldUtils.isPrimitiveType(wsetType.getNestedType());
        }
        return false;
    }

    private void fail(DocumentSummary summary, SummaryField field, String msg) {
        throw new IllegalArgumentException(formatError(schema, summary, field, msg));
    }

    private String formatError(Schema schema, DocumentSummary summary, SummaryField field, String msg) {
        return "For " + schema + ", document-summary '" + summary.name()
               + "', summary field '" + field.getName() + "': " + msg;
    }

}

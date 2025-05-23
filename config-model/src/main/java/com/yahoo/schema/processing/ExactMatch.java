// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.processing;

import com.yahoo.config.application.api.DeployLogger;
import com.yahoo.document.CollectionDataType;
import com.yahoo.document.DataType;
import com.yahoo.schema.RankProfileRegistry;
import com.yahoo.schema.Schema;
import com.yahoo.schema.document.Case;
import com.yahoo.schema.document.MatchType;
import com.yahoo.schema.document.Matching;
import com.yahoo.schema.document.SDField;
import com.yahoo.schema.document.Stemming;
import com.yahoo.vespa.indexinglanguage.ExpressionSearcher;
import com.yahoo.vespa.indexinglanguage.expressions.ExactExpression;
import com.yahoo.vespa.indexinglanguage.expressions.Expression;
import com.yahoo.vespa.indexinglanguage.expressions.ForEachExpression;
import com.yahoo.vespa.indexinglanguage.expressions.IndexExpression;
import com.yahoo.vespa.indexinglanguage.expressions.OutputExpression;
import com.yahoo.vespa.indexinglanguage.expressions.ScriptExpression;
import com.yahoo.vespa.indexinglanguage.linguistics.AnnotatorConfig;
import com.yahoo.vespa.model.container.search.QueryProfiles;

/**
 * The implementation of exact matching
 *
 * @author bratseth
 */
public class ExactMatch extends Processor {

    public static final String DEFAULT_EXACT_TERMINATOR = "@@";

    ExactMatch(Schema schema, DeployLogger deployLogger, RankProfileRegistry rankProfileRegistry, QueryProfiles queryProfiles) {
        super(schema, deployLogger, rankProfileRegistry, queryProfiles);
    }

    @Override
    public void process(boolean validate, boolean documentsOnly) {
        for (SDField field : schema.allConcreteFields()) {
            processField(field, schema);
        }
    }

    private void processField(SDField field, Schema schema) {
        MatchType matching = field.getMatching().getType();
        if (matching.equals(MatchType.EXACT) || matching.equals(MatchType.WORD)) {
            implementExactMatch(field, schema);
        } else if (field.getMatching().getExactMatchTerminator() != null) {
            warn(schema, field, "exact-terminator requires 'exact' matching to have any effect.");
        }
        for (var structField : field.getStructFields()) {
            processField(structField, schema);
        }
    }

    private void implementExactMatch(SDField field, Schema schema) {
        field.setStemming(Stemming.NONE);
        field.getNormalizing().inferLowercase();

        if (field.getMatching().getType().equals(MatchType.WORD)) {
            field.addQueryCommand("word");
        } else { // exact
            String exactTerminator = DEFAULT_EXACT_TERMINATOR;
            if (field.getMatching().getExactMatchTerminator() != null
                && !field.getMatching().getExactMatchTerminator().isEmpty()) {
                exactTerminator = field.getMatching().getExactMatchTerminator();
            } else {
                info(schema, field,
                     "With 'exact' matching, an exact-terminator is needed," +
                     " using default value '" + exactTerminator +"' as terminator");
            }
            field.addQueryCommand("exact " + exactTerminator);

            // The following part illustrates how nice it would have been with canonical representation of indices
            if (field.doesIndexing()) {
                exactMatchSettingsForField(field);
            }
        }
        ScriptExpression script = field.getIndexingScript();
        if (new ExpressionSearcher<>(IndexExpression.class).containedIn(script)) {

            var exactTransformer = new ExactScriptTransformer(schema, toAnnotatorConfig(field.getMatching()));
            field.setIndexingScript(schema.getName(), (ScriptExpression)exactTransformer.convert(field.getIndexingScript()));
        }
    }

    private AnnotatorConfig toAnnotatorConfig(Matching matching) {
        var config = new AnnotatorConfig();
        if (matching.maxTokenLength() != null)
            config.setMaxTokenLength(matching.maxTokenLength());
        if (matching.getCase() == Case.CASED)
            config.setLowercase(false);
        return config;
    }

    private void exactMatchSettingsForField(SDField field) {
        field.getRanking().setFilter(true);
    }

    private static class ExactScriptTransformer extends TypedTransformProvider {

        private final AnnotatorConfig config;

        ExactScriptTransformer(Schema schema, AnnotatorConfig config) {
            super(ExactExpression.class, schema);
            this.config = config;
        }

        @Override
        protected boolean requiresTransform(Expression exp, DataType fieldType) {
            return exp instanceof OutputExpression;
        }

        @Override
        protected Expression newTransform(DataType fieldType) {
            Expression expression = new ExactExpression(config);
            if (fieldType instanceof CollectionDataType)
                expression = new ForEachExpression(expression);
            return expression;
        }

    }

}

// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.derived;

import com.yahoo.schema.Schema;
import com.yahoo.schema.document.GeoPos;
import com.yahoo.schema.document.ImmutableSDField;
import com.yahoo.vespa.configdefinition.IlscriptsConfig;
import com.yahoo.vespa.configdefinition.IlscriptsConfig.Ilscript.Builder;
import com.yahoo.vespa.indexinglanguage.ExpressionConverter;
import com.yahoo.vespa.indexinglanguage.ExpressionVisitor;
import com.yahoo.vespa.indexinglanguage.expressions.AttributeExpression;
import com.yahoo.vespa.indexinglanguage.expressions.ClearStateExpression;
import com.yahoo.vespa.indexinglanguage.expressions.Expression;
import com.yahoo.vespa.indexinglanguage.expressions.GuardExpression;
import com.yahoo.vespa.indexinglanguage.expressions.InputExpression;
import com.yahoo.vespa.indexinglanguage.expressions.OutputExpression;
import com.yahoo.vespa.indexinglanguage.expressions.PassthroughExpression;
import com.yahoo.vespa.indexinglanguage.expressions.ScriptExpression;
import com.yahoo.vespa.indexinglanguage.expressions.SetLanguageExpression;
import com.yahoo.vespa.indexinglanguage.expressions.StatementExpression;
import com.yahoo.vespa.indexinglanguage.expressions.TokenizeExpression;
import com.yahoo.vespa.indexinglanguage.expressions.ZCurveExpression;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * An indexing language script derived from a schema. An indexing script contains a set of indexing
 * statements, organized in a composite structure of indexing code snippets.
 *
 * @author bratseth
 */
public final class IndexingScript extends Derived {

    private final List<String> docFields = new ArrayList<>();
    private final List<Expression> expressions = new ArrayList<>();
    private List<ImmutableSDField> fieldsSettingLanguage;
    private final boolean isStreaming;

    public IndexingScript(Schema schema, boolean isStreaming) {
        this.isStreaming = isStreaming;
        derive(schema);
    }

    @Override
    protected void derive(Schema schema) {
        fieldsSettingLanguage = fieldsSettingLanguage(schema);
        if (fieldsSettingLanguage.size() == 1) // Assume this language should be used for all fields
            addExpression(fieldsSettingLanguage.get(0).getIndexingScript());
        super.derive(schema);
    }

    @Override
    protected void derive(ImmutableSDField field, Schema schema) {
        if (field.isImportedField()) return;

        if (field.hasFullIndexingDocprocRights())
            docFields.add(field.getName());

        if (field.usesStructOrMap() && ! GeoPos.isAnyPos(field)) return; // unsupported
        if (fieldsSettingLanguage.size() == 1 && fieldsSettingLanguage.get(0).equals(field)) return; // Already added

        addExpression(field.getIndexingScript());
    }

    private void addExpression(ScriptExpression expression) {
        if ( expression.isEmpty()) return;
        expressions.add(new StatementExpression(new ClearStateExpression(), new GuardExpression(expression)));
    }

    private List<ImmutableSDField> fieldsSettingLanguage(Schema schema) {
        return schema.allFieldsList().stream()
                     .filter(field -> ! field.isImportedField())
                     .filter(field -> field.containsExpression(SetLanguageExpression.class))
                     .toList();
    }

    public Iterable<Expression> expressions() {
        return Collections.unmodifiableCollection(expressions);
    }

    @Override
    public String getDerivedName() {
        return "ilscripts";
    }

    public void getConfig(IlscriptsConfig.Builder configBuilder) {
        // Append
        IlscriptsConfig.Ilscript.Builder ilscriptBuilder = new IlscriptsConfig.Ilscript.Builder();
        ilscriptBuilder.doctype(getName());
        ilscriptBuilder.docfield(docFields);
        addContentInOrder(ilscriptBuilder);
        configBuilder.ilscript(ilscriptBuilder);
    }

    public void export(String toDirectory) throws IOException {
        var builder = new IlscriptsConfig.Builder();
        getConfig(builder);
        export(toDirectory, builder.build());
    }

    private static class DropTokenize extends ExpressionConverter {
        @Override
        protected boolean shouldConvert(Expression exp) {
            return exp instanceof TokenizeExpression;
        }

        @Override
        protected Expression doConvert(Expression exp) {
            return null;
        }
    }

    // for streaming, drop zcurve conversion to attribute with suffix
    private static class DropZcurve extends ExpressionConverter {
        private static final String zSuffix = "_zcurve";
        private static final int zSuffixLen = zSuffix.length();
        private boolean seenZcurve = false;

        @Override
        protected boolean shouldConvert(Expression exp) {
            if (exp instanceof ZCurveExpression) {
                seenZcurve = true;
                return true;
            }
            if (seenZcurve && exp instanceof AttributeExpression attrExp) {
                return attrExp.getFieldName().endsWith(zSuffix);
            }
            return false;
        }

        @Override
        protected Expression doConvert(Expression exp) {
            if (exp instanceof ZCurveExpression) {
                return null;
            }
            if (exp instanceof AttributeExpression attrExp) {
                String orig = attrExp.getFieldName();
                int len = orig.length();
                if (len > zSuffixLen && orig.endsWith(zSuffix)) {
                    String fieldName = orig.substring(0, len - zSuffixLen);
                    var result = new AttributeExpression(fieldName);
                    return result;
                }
            }
            return exp;
        }
    }

    private void addContentInOrder(IlscriptsConfig.Ilscript.Builder ilscriptBuilder) {
        List<Expression> ordered = orderExpressions(expressions);
        Set<String> touchedFields = new HashSet<>();
        for (Expression expression : ordered) {
            if (isStreaming) {
                expression = expression.convertChildren(new DropTokenize());
                expression = expression.convertChildren(new DropZcurve());
            }
            ilscriptBuilder.content(expression.toString());
            FieldScanVisitor fieldFetcher = new FieldScanVisitor();
            fieldFetcher.visit(expression);
            touchedFields.addAll(fieldFetcher.touchedFields());
        }
        generateSyntheticStatementsForUntouchedFields(ilscriptBuilder, touchedFields);
    }

    private List<Expression> orderExpressions(List<Expression> expressions) {
        List<Expression> result = new ArrayList<>();
        List<ToProcess> toProcess = expressions.stream().map(ToProcess::new).collect(Collectors.toList());
        for (var entry : toProcess) {
            if (entry.done)
                continue;
            for (String modifyingField : entry.modifiesSelf) {
                // NOTE: loops over same list for simplicity
                for (var checkUsing : toProcess) {
                    if (checkUsing.done || checkUsing == entry)
                        continue;
                    if (checkUsing.inputs.contains(modifyingField)) {
                        result.add(checkUsing.getExpression());
                    }
                }
            }
            result.add(entry.getExpression());
        }
        return result;
    }

    private static class ToProcess {
        boolean done = false;
        final Set<String> inputs = new HashSet();
        final Set<String> outputs = new HashSet();
        final Set<String> modifiesSelf = new HashSet();
        private Expression expr;

        // should only be called once
        Expression getExpression() {
            done = true;
            return expr;
        }

        ToProcess(Expression expr) {
            this.expr = expr;
            // analyze expression:
            var visitor = new GetIOVisitor();
            visitor.visit(expr);
            for (String out : outputs) {
                if (inputs.contains(out)) {
                    modifiesSelf.add(out);
                }
            }
        }

        private class GetIOVisitor extends ExpressionVisitor {
            @Override
            protected void doVisit(Expression expression) {
                if (expression instanceof InputExpression in) {
                    inputs.add(in.getFieldName());
                }
                if (expression instanceof OutputExpression out) {
                    outputs.add(out.getFieldName());
                }
            }
        }
    }

    private void generateSyntheticStatementsForUntouchedFields(Builder ilscriptBuilder, Set<String> touchedFields) {
        Set<String> fieldsWithSyntheticStatements = new HashSet<>(docFields);
        fieldsWithSyntheticStatements.removeAll(touchedFields);
        List<String> orderedFields = new ArrayList<>(fieldsWithSyntheticStatements);
        Collections.sort(orderedFields);
        for (String fieldName : orderedFields) {
            StatementExpression copyField = new StatementExpression(new InputExpression(fieldName),
                    new PassthroughExpression(fieldName));
            ilscriptBuilder.content(copyField.toString());
        }
    }

    private boolean setsLanguage(Expression expression) {
        SetsLanguageVisitor visitor = new SetsLanguageVisitor();
        visitor.visit(expression);
        return visitor.setsLanguage;
    }

    private boolean modifiesSelf(Expression expression) {
        ModifiesSelfVisitor visitor = new ModifiesSelfVisitor();
        visitor.visit(expression);
        return visitor.modifiesSelf();
    }

    private static class ModifiesSelfVisitor extends ExpressionVisitor {

        private String inputField = null;
        private String outputField = null;

        public boolean modifiesSelf() { return outputField != null && outputField.equals(inputField); }

        @Override
        protected void doVisit(Expression expression) {
            if (modifiesSelf()) return;

            if (expression instanceof InputExpression) {
                inputField = ((InputExpression) expression).getFieldName();
            }
            if (expression instanceof OutputExpression) {
                outputField = ((OutputExpression) expression).getFieldName();
            }
        }
    }

    private static class SetsLanguageVisitor extends ExpressionVisitor {

        boolean setsLanguage = false;

        @Override
        protected void doVisit(Expression expression) {
            if (expression instanceof SetLanguageExpression)
                setsLanguage = true;
        }

    }

    private static class FieldScanVisitor extends ExpressionVisitor {
        List<String> touchedFields = new ArrayList<>();
        List<String> candidates = new ArrayList<>();

        @Override
        protected void doVisit(Expression exp) {
            if (exp instanceof OutputExpression) {
                touchedFields.add(((OutputExpression) exp).getFieldName());
            }
            if (exp instanceof InputExpression) {
                candidates.add(((InputExpression) exp).getFieldName());
            }
            if (exp instanceof ZCurveExpression) {
                touchedFields.addAll(candidates);
            }
        }

        Collection<String> touchedFields() {
            Collection<String> output = touchedFields;
            touchedFields = null; // deny re-use to try and avoid obvious bugs
            return output;
        }
    }
}

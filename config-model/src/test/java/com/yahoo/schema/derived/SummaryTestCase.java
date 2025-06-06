// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.derived;

import com.yahoo.config.model.application.provider.BaseDeployLogger;
import com.yahoo.config.model.deploy.TestProperties;
import com.yahoo.config.model.test.MockApplicationPackage;
import com.yahoo.document.PositionDataType;
import com.yahoo.schema.RankProfileRegistry;
import com.yahoo.schema.Schema;
import com.yahoo.schema.ApplicationBuilder;
import com.yahoo.schema.AbstractSchemaTestCase;
import com.yahoo.schema.document.SDDocumentType;
import com.yahoo.schema.document.SDField;
import com.yahoo.schema.parser.ParseException;
import com.yahoo.schema.processing.Processing;
import com.yahoo.vespa.config.search.SummaryConfig;
import com.yahoo.vespa.documentmodel.SummaryElementsSelector;
import com.yahoo.vespa.documentmodel.SummaryTransform;
import com.yahoo.vespa.model.container.search.QueryProfiles;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.util.Set;

import static com.yahoo.config.model.test.TestUtil.joinLines;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.fail;

/**
 * Tests summary extraction
 *
 * @author bratseth
 */
public class SummaryTestCase extends AbstractSchemaTestCase {

    @Test
    void deriveRawAsBase64() throws ParseException {
        String sd = joinLines(
                "schema s {",
                "  document s {",
                "      field raw_field type raw {",
                "          indexing: summary",
                "      }",
                "  }",
                "}");
        Schema schema = ApplicationBuilder.createFromString(sd).getSchema();
        SummaryClass summary = new SummaryClass(schema, schema.getSummary("default"), new BaseDeployLogger());
        assertEquals(SummaryClassField.Type.RAW, summary.fields().get("raw_field").getType());
    }

    @Test
    void deriveRawAsLegacy() throws ParseException {
        String sd = joinLines(
                "schema s {",
                "  raw-as-base64-in-summary: false",
                "  document s {",
                "      field raw_field type raw {",
                "          indexing: summary",
                "      }",
                "  }",
                "}");
        Schema schema = ApplicationBuilder.createFromString(sd).getSchema();
        SummaryClass summary = new SummaryClass(schema, schema.getSummary("default"), new BaseDeployLogger());
        assertEquals(SummaryClassField.Type.DATA, summary.fields().get("raw_field").getType());
    }

    @Test
    void testDeriving() throws IOException, ParseException {
        Schema schema = ApplicationBuilder.buildFromFile("src/test/examples/simple.sd");
        SummaryClass summary = new SummaryClass(schema, schema.getSummary("default"), new BaseDeployLogger());
        assertEquals("default", summary.getName());

        var fields = summary.fields().values().iterator();
        assertEquals(13, summary.fields().size());

        assertSummaryField("exactemento", SummaryClassField.Type.LONGSTRING, fields.next());
        assertSummaryField("exact", SummaryClassField.Type.LONGSTRING, fields.next());
        assertSummaryField("title", SummaryClassField.Type.LONGSTRING, fields.next());
        assertSummaryField("description", SummaryClassField.Type.LONGSTRING, fields.next());
        assertSummaryField("dyndesc", SummaryClassField.Type.LONGSTRING, "dynamicteaser", "description", fields.next());
        assertSummaryField("longdesc", SummaryClassField.Type.LONGSTRING, fields.next());
        assertSummaryField("longstat", SummaryClassField.Type.LONGSTRING, "copy", "longdesc", fields.next());
        assertSummaryField("dynlong", SummaryClassField.Type.LONGSTRING, "dynamicteaser", "longdesc", fields.next());
        assertSummaryField("dyndesc2", SummaryClassField.Type.LONGSTRING, "dynamicteaser", "longdesc", fields.next());
        assertSummaryField("measurement", SummaryClassField.Type.INTEGER, "attribute", "measurement", fields.next());
        assertSummaryField("rankfeatures", SummaryClassField.Type.FEATUREDATA, "rankfeatures", fields.next());
        assertSummaryField("summaryfeatures", SummaryClassField.Type.FEATUREDATA, "summaryfeatures", fields.next());
        assertSummaryField("documentid", SummaryClassField.Type.LONGSTRING, "documentid", fields.next());
    }

    @Test
    void reference_fields_can_be_part_of_summary_classes() throws ParseException {
        Schema adSchema = buildCampaignAdModel();

        SummaryClass defaultClass = new SummaryClass(adSchema, adSchema.getSummary("default"), new BaseDeployLogger());
        assertEquals(SummaryClassField.Type.LONGSTRING, defaultClass.fields().get("campaign_ref").getType());
        assertEquals(SummaryClassField.Type.LONGSTRING, defaultClass.fields().get("other_campaign_ref").getType());

        SummaryClass myClass = new SummaryClass(adSchema, adSchema.getSummary("my_summary"), new BaseDeployLogger());
        assertNull(myClass.fields().get("campaign_ref"));
        assertEquals(SummaryClassField.Type.LONGSTRING, myClass.fields().get("other_campaign_ref").getType());
    }

    private void assertSummaryField(String expName, SummaryClassField.Type expType, SummaryClassField field) {
        assertSummaryField(expName, expType, "", "", field);
    }

    private void assertSummaryField(String expName, SummaryClassField.Type expType, String expCommand, SummaryClassField field) {
        assertSummaryField(expName, expType, expCommand, "", field);
    }

    private void assertSummaryField(String expName, SummaryClassField.Type expType,
                                    String expCommand, String expSource, SummaryClassField field) {
        assertEquals(expName, field.getName());
        assertEquals(expType, field.getType());
        assertEquals(expCommand, field.getCommand());
        assertEquals(expSource, field.getSource());

    }

    private static Schema buildCampaignAdModel() throws ParseException {
        ApplicationBuilder builder = new ApplicationBuilder();
        builder.addSchema("search campaign { document campaign {} }");
        builder.addSchema(joinLines("search ad {",
                                    "  document ad {",
                                    "    field campaign_ref type reference<campaign> {",
                                    "      indexing: summary | attribute",
                                    "    }",
                                    "    field other_campaign_ref type reference<campaign> {",
                                    "      indexing: summary | attribute",
                                    "    }",
                                    "  }",
                                    "  document-summary my_summary {",
                                    "    summary other_campaign_ref {}",
                                    "  }",
                                    "}"));
        builder.build(true);
        return builder.getSchema("ad");
    }

    @Test
    void omit_summary_features_specified_for_document_summary() throws ParseException {
        String sd = joinLines(
                "schema test {",
                "  document test {",
                "    field foo type string { indexing: summary }",
                "  }",
                "  document-summary bar {",
                "    summary foo {}",
                "    omit-summary-features",
                "  }",
                "  document-summary baz {",
                "    summary foo {}",
                "  }",
                "}");
        var search = ApplicationBuilder.createFromString(sd).getSchema();
        assertOmitSummaryFeatures(true, search, "bar");
        assertOmitSummaryFeatures(false, search, "baz");
    }

    private void assertOmitSummaryFeatures(boolean expected, Schema schema, String summaryName) {
        var summary = new SummaryClass(schema, schema.getSummary(summaryName), new BaseDeployLogger());
        var config = new SummaryConfig.Classes(summary.getSummaryClassConfig());
        assertEquals(expected, config.omitsummaryfeatures());
    }

    @Test
    void testPositionDeriving() {
        Schema schema = new Schema("store", MockApplicationPackage.createEmpty());
        SDDocumentType document = new SDDocumentType("store");
        schema.addDocument(document);
        String fieldName = "location";
        SDField field = document.addField(fieldName, PositionDataType.INSTANCE);
        field.parseIndexingScript(schema.getName(), "{ attribute | summary }");
        new Processing(new TestProperties())
                .process(schema, new BaseDeployLogger(), new RankProfileRegistry(), new QueryProfiles(),
                         true, false, Set.of());

        var summary = new SummaryClass(schema, schema.getSummary("default"), new BaseDeployLogger());
        var fields = summary.fields().values().iterator();
        assertEquals(4, summary.fields().size());
        assertSummaryField(fieldName, SummaryClassField.Type.JSONSTRING, "geopos", "location_zcurve", fields.next());
        assertSummaryField("rankfeatures", SummaryClassField.Type.FEATUREDATA, "rankfeatures", fields.next());
        assertSummaryField("summaryfeatures", SummaryClassField.Type.FEATUREDATA, "summaryfeatures", fields.next());
        assertSummaryField("documentid", SummaryClassField.Type.LONGSTRING, "documentid", fields.next());
    }

    @Test
    void testFailOnSummaryFieldSourceCollision() {
        try {
            ApplicationBuilder.buildFromFile("src/test/examples/summaryfieldcollision.sd");
        } catch (Exception e) {
            assertTrue(e.getMessage().matches(".*equally named field.*"));
        }
    }

    @Test
    void matched_elements_only_works_with_attribute_combiner() throws ParseException {
        var schema = buildSchema(joinLines("field my_field type map<string, string> {",
                "  indexing: summary",
                "  summary: matched-elements-only",
                "  struct-field key { indexing: attribute }",
                "}"), "");
        assertOverride(schema, "my_field", "", "");
        assertElementSelect(schema, "my_field", SummaryConfig.Classes.Fields.Elements.Select.Enum.BY_MATCH, "", "default");

        schema = buildSchema(joinLines("field my_field type map<string, string> {",
                "  indexing: summary",
                "  summary: matched-elements-only",
                "  struct-field key { indexing: attribute }",
                "  struct-field value { indexing: attribute }",
                "}"), "");
        assertOverride(schema, "my_field", SummaryTransform.ATTRIBUTECOMBINER.getName(), "");
        assertElementSelect(schema, "my_field", SummaryConfig.Classes.Fields.Elements.Select.Enum.BY_MATCH, "", "default");
    }

    @Test
    void commands_that_are_dynamic_and_require_the_query() {
        assertTrue(SummaryClass.commandRequiringQuery("dynamicteaser"));
        assertFalse(SummaryClass.commandRequiringQuery(SummaryTransform.ATTRIBUTE.getName()));
    }

    @Test
    void elements_selectors_that_are_dynamic_and_require_the_query() {
        assertFalse(SummaryClass.elementsSelectorRequiringQuery(SummaryElementsSelector.selectAll()));
        assertTrue(SummaryClass.elementsSelectorRequiringQuery(SummaryElementsSelector.selectByMatch()));
    }

    @Test
    void documentid_summary_field_has_corresponding_summary_transform() throws ParseException {
        var schema = buildSchema("field foo type string { indexing: summary }",
                joinLines("document-summary bar {",
                        "    summary documentid {}",
                        "}"));
        assertOverride(schema, "documentid", SummaryTransform.DOCUMENT_ID.getName(), "", "bar");
    }

    private void check_tokens_override(boolean index, boolean attribute, SummaryTransform exp) throws ParseException {
        var schema = buildSchema("field foo type string { indexing: " +
                (index ? "index | " : "") +
                (attribute ? "attribute | " : "") + "summary }",
                joinLines("document-summary bar {",
                        "    summary baz {",
                        "        source: foo ",
                        "        tokens",
                        "     }",
                        "    from-disk",
                        "}"));
        assertOverride(schema, "baz", exp.getName(), "foo", "bar");
        assert(!schema.getSummary("default").getSummaryFields().containsKey("baz"));
    }

    @Test
    void tokens_override() throws ParseException {
        try {
            check_tokens_override(false, false, SummaryTransform.TOKENS);
            fail("Expected IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            assertEquals("For schema 'test', document-summary 'bar'" +
                    ", summary field 'baz', source field 'foo'" +
                    ": tokens summary field setting requires index or attribute for source field", e.getMessage());
        }
        check_tokens_override(false, true, SummaryTransform.ATTRIBUTE_TOKENS);
        check_tokens_override(true, false, SummaryTransform.TOKENS);
        check_tokens_override(true, true, SummaryTransform.TOKENS);
    }

    @Test
    void documentid_summary_transform_requires_disk_access() {
        assertFalse(SummaryTransform.DOCUMENT_ID.isInMemory());
    }

    @Test
    void matched_elements_only_sets_selement_selector() throws ParseException {
        var schema = buildSchema("field foo type array<string> { indexing: attribute | summary }",
                joinLines("document-summary bar {",
                        "    summary baz {",
                        "        source: foo ",
                        "        matched-elements-only",
                        "     }",
                        "    from-disk",
                        "}"));
        assertElementSelect(schema, "baz", SummaryConfig.Classes.Fields.Elements.Select.Enum.BY_MATCH, "", "bar");
        assert(!schema.getSummary("default").getSummaryFields().containsKey("baz"));
    }

    @Test
    void select_elements_by_sets_selement_selector() throws ParseException {
        var schema = buildSchema("field foo type array<string> { indexing: attribute | summary }",
            joinLines("document-summary bar {",
                "    summary baz {",
                "        source: foo ",
                "        select-elements-by: elementwise(bm25(foo),x,double)",
                "     }",
                "    from-disk",
                "}"));
        assertElementSelect(schema, "baz", SummaryConfig.Classes.Fields.Elements.Select.Enum.BY_SUMMARY_FEATURE,
            "elementwise(bm25(foo),x,double)", "bar");
        assert(!schema.getSummary("default").getSummaryFields().containsKey("baz"));
    }

    private void assertOverride(String fieldContent, String expFieldName, String expCommand) throws ParseException {
        assertOverride(buildSchema(fieldContent, ""), expFieldName, expCommand, expFieldName);
    }

    private void assertOverride(Schema schema, String expFieldName, String expCommand, String expSource) throws ParseException {
        assertOverride(schema, expFieldName, expCommand, expSource, "default");
    }

    private void assertOverride(Schema schema, String expFieldName, String expCommand, String expSource, String summaryClass) throws ParseException {
        var summary = new SummaryClass(schema, schema.getSummary(summaryClass), new BaseDeployLogger());
        var cfg = new SummaryConfig.Classes(summary.getSummaryClassConfig());
        var field = cfg.fields(0);
        assertEquals(expFieldName, field.name());
        assertEquals(expCommand, field.command());
        assertEquals(expSource, field.source());
    }

    private void assertElementSelect(Schema schema, String expFieldName, SummaryConfig.Classes.Fields.Elements.Select.Enum expSelect, String expSummaryFeature, String summaryClass) throws ParseException {
        var summary = new SummaryClass(schema, schema.getSummary(summaryClass), new BaseDeployLogger());
        var cfg = new SummaryConfig.Classes(summary.getSummaryClassConfig());
        var field = cfg.fields(0);
        assertEquals(expFieldName, field.name());
        assertEquals(expSelect, field.elements().select());
        assertEquals(expSummaryFeature, field.elements().summary_feature());
    }

    private Schema buildSchema(String field, String documentSummary) throws ParseException {
        var builder = new ApplicationBuilder(new RankProfileRegistry());
        builder.addSchema(joinLines("search test {",
                "  document test {",
                field,
                "  }",
                documentSummary,
                "}"));
        builder.build(true);
        return builder.getSchema();
    }

}

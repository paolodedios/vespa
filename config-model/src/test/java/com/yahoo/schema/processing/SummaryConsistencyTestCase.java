// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.processing;

import com.yahoo.schema.Schema;
import com.yahoo.schema.ApplicationBuilder;
import com.yahoo.schema.parser.ParseException;
import com.yahoo.vespa.documentmodel.SummaryElementsSelector;
import com.yahoo.vespa.documentmodel.SummaryTransform;
import org.junit.jupiter.api.Test;

import static com.yahoo.config.model.test.TestUtil.joinLines;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

public class SummaryConsistencyTestCase {

    @Test
    void attribute_combiner_transform_is_set_when_source_is_array_of_struct_with_only_struct_field_attributes() throws ParseException {
        String sd = joinLines(
                "search structmemorysummary {",
                "  document structmemorysummary {",
                "      struct elem {",
                "        field name type string {}",
                "        field weight type int {}\n",
                "      }",
                "      field elem_array type array<elem> {",
                "          indexing: summary",
                "          struct-field name {",
                "              indexing: attribute",
                "          }",
                "          struct-field weight {",
                "              indexing: attribute",
                "          }",
                "      }",
                "  }",
                "  document-summary unfiltered {",
                "      summary elem_array_unfiltered {",
                "          source: elem_array",
                "      }",
                "  }",
                "",
                "}"
        );
        Schema schema = ApplicationBuilder.createFromString(sd).getSchema();
        assertEquals(SummaryTransform.ATTRIBUTECOMBINER, schema.getSummaryField("elem_array_unfiltered").getTransform());
    }

    @Test
    void testDocumentSummaryWithInheritanceOfNonExistingSummary() {
        String schemaString = """
                schema foo {
                  document foo {
                    field foo type string {
                        indexing: summary
                    }
                  }
                  document-summary foo_summary inherits non-existent {
                    summary foo {
                        source: foo
                    }
                  }
                }
                """;
        assertThrows(IllegalArgumentException.class, () -> ApplicationBuilder.createFromString(schemaString).getSchema());
    }

    @Test
    void testMismatchedElementSelectorsMadeConsistent() throws ParseException {
        String sd = """
                schema foo {
                  document foo {
                    field foo type array<string> {
                        indexing: summary
                        summary: matched-elements-only
                    }
                  }
                  document-summary foo_summary {
                    summary foo {
                        source: foo
                    }
                  }
                }
                """;
        var builder = ApplicationBuilder.createFromString(sd);
        var schema = builder.getSchema();
        var summary = schema.getSummary("foo_summary");
        var summaryField = summary.getSummaryField("foo");
        assertEquals(SummaryElementsSelector.selectByMatch(), summaryField.getElementsSelector());
    }

}

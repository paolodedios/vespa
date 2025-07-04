// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
// Unit tests for documenttyperepo.

#include <vespa/document/base/testdocrepo.h>
#include <vespa/config/print/asciiconfigwriter.h>
#include <vespa/document/datatype/annotationreferencedatatype.h>
#include <vespa/document/datatype/arraydatatype.h>
#include <vespa/document/datatype/documenttype.h>
#include <vespa/document/datatype/mapdatatype.h>
#include <vespa/document/datatype/tensor_data_type.h>
#include <vespa/document/datatype/weightedsetdatatype.h>
#include <vespa/document/fieldvalue/fieldvalue.h>
#include <vespa/document/repo/configbuilder.h>
#include <vespa/document/repo/documenttyperepo.h>
#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/objects/identifiable.h>
#include <vespa/vespalib/test/test_path.h>
#include <vespa/vespalib/util/exceptions.h>
#include <set>
#include <string>


using config::AsciiConfigWriter;
using std::set;
using std::vector;
using vespalib::Identifiable;
using vespalib::IllegalArgumentException;
using std::string;

using namespace document::config_builder;
using namespace document;

namespace {

const string type_name = "test";
const int32_t doc_type_id = 787121340;
const string header_name = type_name + ".header";
const int32_t header_id = 30;
const string body_name = type_name + ".body";
const int32_t body_id = 31;
const string type_name_2 = "test_2";
const string header_name_2 = type_name_2 + ".header";
const string body_name_2 = type_name_2 + ".body";
const string field_name = "field_name";
const string derived_name = "derived";

TEST(DocumentTypeRepoTest, requireThatDocumentTypeCanBeLookedUp)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name).setId(header_id),
                     Struct(body_name).setId(body_id));
    DocumentTypeRepo repo(builder.config());

    const DocumentType *type = repo.getDocumentType(type_name);
    ASSERT_TRUE(type);
    EXPECT_EQ(type_name, type->getName());
    EXPECT_EQ(doc_type_id, type->getId());
    EXPECT_EQ(header_name, type->getFieldsType().getName());
/*
    TODO(vekterli): Check fields struct ID after it has been determined which ID it should get
    EXPECT_EQ(header_id, type->getHeader().getId());
    EXPECT_EQ(header_name, type->getHeader().getName());
    EXPECT_EQ(body_id, type->getBody().getId());
    EXPECT_EQ(body_name, type->getBody().getName());
*/
}

TEST(DocumentTypeRepoTest, requireThatDocumentTypeCanBeLookedUpWhenIdIsNotAHash)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id + 2, type_name,
                     Struct(header_name).setId(header_id),
                     Struct(body_name).setId(body_id));
    DocumentTypeRepo repo(builder.config());

    const DocumentType *type = repo.getDocumentType(type_name);
    ASSERT_TRUE(type);
}

TEST(DocumentTypeRepoTest, requireThatStructsCanHaveFields)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name, DataType::T_INT));
    DocumentTypeRepo repo(builder.config());

    const StructDataType &s = repo.getDocumentType(type_name)->getFieldsType();
    ASSERT_EQ(1u, s.getFieldCount());
    const Field &field = s.getField(field_name);
    EXPECT_EQ(DataType::T_INT, field.getDataType().getId());
}

template <typename T>
const T &getFieldDataType(const DocumentTypeRepo &repo) {
    const DataType &d = repo.getDocumentType(type_name)
                        ->getFieldsType().getField(field_name).getDataType();
    return dynamic_cast<const T&>(d);
}

TEST(DocumentTypeRepoTest, requireThatArraysCanBeConfigured)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             Array(DataType::T_STRING)));
    DocumentTypeRepo repo(builder.config());

    const ArrayDataType &a = getFieldDataType<ArrayDataType>(repo);
    EXPECT_EQ(DataType::T_STRING, a.getNestedType().getId());
}

TEST(DocumentTypeRepoTest, requireThatWsetsCanBeConfigured)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             Wset(DataType::T_INT)
                             .removeIfZero().createIfNonExistent()));
    DocumentTypeRepo repo(builder.config());

    const WeightedSetDataType &w = getFieldDataType<WeightedSetDataType>(repo);
    EXPECT_EQ(DataType::T_INT, w.getNestedType().getId());
    EXPECT_TRUE(w.createIfNonExistent());
    EXPECT_TRUE(w.removeIfZero());
}

TEST(DocumentTypeRepoTest, requireThatMapsCanBeConfigured)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             Map(DataType::T_INT, DataType::T_STRING)));
    DocumentTypeRepo repo(builder.config());

    const MapDataType &m = getFieldDataType<MapDataType>(repo);
    EXPECT_EQ(DataType::T_INT, m.getKeyType().getId());
    EXPECT_EQ(DataType::T_STRING, m.getValueType().getId());
}

TEST(DocumentTypeRepoTest, requireThatAnnotationReferencesCanBeConfigured)
{
    int32_t annotation_type_id = 424;
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             AnnotationRef(annotation_type_id)))
        .annotationType(annotation_type_id, "foo", -1);
    DocumentTypeRepo repo(builder.config());

    const AnnotationReferenceDataType &ar = getFieldDataType<AnnotationReferenceDataType>(repo);
    EXPECT_EQ(annotation_type_id, ar.getAnnotationType().getId());
}

TEST(DocumentTypeRepoTest, requireThatFieldsCanNotBeHeaderAndBody)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name).addField(field_name,
                             DataType::T_STRING),
                     Struct(body_name).addField(field_name,
                             DataType::T_INT));
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException,
                           "Failed to add field 'field_name' to struct 'test.header': "
                           "Name in use by field with different id");
}

TEST(DocumentTypeRepoTest, requireThatDocumentStructsAreCalledHeaderAndBody)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name, Struct("foo"), Struct("bar"));
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException,
                           "Previously defined as \"test.header\".");
}

TEST(DocumentTypeRepoTest, requireThatDocumentsCanInheritFields)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name, DataType::T_INT));
    builder.document(doc_type_id + 1, derived_name,
                     Struct("derived.header"),
                     Struct("derived.body").addField("derived_field",
                             DataType::T_STRING))
        .inherit(doc_type_id);
    DocumentTypeRepo repo(builder.config());

    const StructDataType &s =
        repo.getDocumentType(doc_type_id + 1)->getFieldsType();
    ASSERT_EQ(2u, s.getFieldCount());
    const Field &field = s.getField(field_name);
    const DataType &type = field.getDataType();
    EXPECT_EQ(DataType::T_INT, type.getId());
}

TEST(DocumentTypeRepoTest, requireThatDocumentsCanUseInheritedTypes)
{
    const int32_t id = 64;
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField("foo",
                             Array(DataType::T_INT).setId(id)));
    builder.document(doc_type_id + 1, derived_name,
                     Struct("derived.header"),
                     Struct("derived.body").addField(field_name, id))
        .inherit(doc_type_id);
    DocumentTypeRepo repo(builder.config());

    const DataType &type =
        repo.getDocumentType(doc_type_id + 1)->getFieldsType()
        .getField(field_name).getDataType();
    EXPECT_EQ(id, type.getId());
    EXPECT_TRUE(dynamic_cast<const ArrayDataType *>(&type));
}

TEST(DocumentTypeRepoTest, requireThatIllegalConfigsCausesExceptions)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name))
        .inherit(doc_type_id + 1);
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Unable to find document");

    builder = DocumenttypesConfigBuilderHelper();
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name));
    builder.config().documenttype[0].datatype[0].type =
        DocumenttypesConfig::Documenttype::Datatype::Type(-1);
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Unknown datatype type -1");

    builder = DocumenttypesConfigBuilderHelper();
    const int id = 10000;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             Array(DataType::T_INT).setId(id)));
    EXPECT_EQ(id, builder.config().documenttype[0].datatype[1].id);
    builder.config().documenttype[0].datatype[1].array.element.id = id;
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Unknown datatype 10000");

    builder = DocumenttypesConfigBuilderHelper();
    builder.document(doc_type_id, type_name,
                     Struct(header_name).setId(header_id),
                     Struct(body_name).addField("foo",
                             Struct("bar").setId(header_id)));
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Redefinition of data type");

    builder = DocumenttypesConfigBuilderHelper();
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name,
                             AnnotationRef(id)));
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Unknown AnnotationType");

    builder = DocumenttypesConfigBuilderHelper();
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name))
        .annotationType(id, type_name, DataType::T_STRING)
        .annotationType(id, type_name, DataType::T_INT);
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Redefinition of annotation type");

    builder = DocumenttypesConfigBuilderHelper();
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name))
        .annotationType(id, type_name, DataType::T_STRING)
        .annotationType(id, "foobar", DataType::T_STRING);
    VESPA_EXPECT_EXCEPTION(DocumentTypeRepo repo(builder.config()),
                           IllegalArgumentException, "Redefinition of annotation type");
}

TEST(DocumentTypeRepoTest, requireThatDataTypesCanBeLookedUpById)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name).setId(header_id), Struct(body_name));
    builder.document(doc_type_id + 1, type_name_2,
                     Struct(header_name_2), Struct(body_name_2));
    DocumentTypeRepo repo(builder.config());

    const DataType *type =
        repo.getDataType(*repo.getDocumentType(doc_type_id), header_id);
    ASSERT_TRUE(type);
    EXPECT_EQ(header_name, type->getName());
    EXPECT_EQ(header_id, type->getId());

    ASSERT_TRUE(!repo.getDataType(*repo.getDocumentType(doc_type_id), -1));
    ASSERT_TRUE(!repo.getDataType(*repo.getDocumentType(doc_type_id + 1),
                                  header_id));
}

TEST(DocumentTypeRepoTest, requireThatDataTypesCanBeLookedUpByName)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name).setId(header_id),
                     Struct(body_name));
    builder.document(doc_type_id + 1, type_name_2,
                     Struct(header_name_2), Struct(body_name_2));
    DocumentTypeRepo repo(builder.config());

    const DataType *type =
        repo.getDataType(*repo.getDocumentType(doc_type_id), header_name);
    ASSERT_TRUE(type);
    EXPECT_EQ(header_name, type->getName());
    EXPECT_EQ(header_id, type->getId());

    EXPECT_TRUE(repo.getDataType(*repo.getDocumentType(doc_type_id),
                                 type_name));
    EXPECT_TRUE(!repo.getDataType(*repo.getDocumentType(doc_type_id),
                                  field_name));
    EXPECT_TRUE(!repo.getDataType(*repo.getDocumentType(doc_type_id + 1),
                                  body_name));
}

TEST(DocumentTypeRepoTest, requireThatInheritingDocCanRedefineIdenticalField)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name)
                     .addField(field_name, DataType::T_STRING)
                     .setId(body_id));
    builder.document(doc_type_id + 1, derived_name,
                     Struct("derived.header"),
                     Struct("derived.body")
                     .addField(field_name, DataType::T_STRING)
                     .setId(body_id))
        .inherit(doc_type_id);
    DocumentTypeRepo repo(builder.config());

    const StructDataType &s = repo.getDocumentType(doc_type_id + 1)->getFieldsType();
    ASSERT_EQ(1u, s.getFieldCount());
}

TEST(DocumentTypeRepoTest, requireThatAnnotationTypesCanBeConfigured)
{
    const int32_t a_id = 654;
    const string a_name = "annotation_name";
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name))
        .annotationType(a_id, a_name, DataType::T_STRING);
    DocumentTypeRepo repo(builder.config());

    const DocumentType *type = repo.getDocumentType(doc_type_id);
    const AnnotationType *a_type = repo.getAnnotationType(*type, a_id);
    ASSERT_TRUE(a_type);
    EXPECT_EQ(a_name, a_type->getName());
    ASSERT_TRUE(a_type->getDataType());
    EXPECT_EQ(DataType::T_STRING, a_type->getDataType()->getId());
}

TEST(DocumentTypeRepoTest, requireThatDocumentsCanUseOtherDocumentTypes)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id + 1, type_name_2,
                     Struct(header_name_2),
                     Struct(body_name_2));
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(type_name_2,
                             doc_type_id + 1).setId(body_id));
    DocumentTypeRepo repo(builder.config());

    const DataType &type = repo.getDocumentType(doc_type_id)->getFieldsType()
                           .getField(type_name_2).getDataType();
    EXPECT_EQ(doc_type_id + 1, type.getId());
    EXPECT_TRUE(dynamic_cast<const DocumentType *>(&type));
}

TEST(DocumentTypeRepoTest, requireThatDocumentTypesCanBeIterated)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name));
    builder.document(doc_type_id + 1, type_name_2,
                     Struct(header_name_2), Struct(body_name_2));
    DocumentTypeRepo repo(builder.config());

    set<int> ids;
    repo.forEachDocumentType(
            [&ids](const DocumentType &type) { ids.insert(type.getId()); });

    EXPECT_EQ(3u, ids.size());
    ASSERT_TRUE(ids.count(DataType::T_DOCUMENT));
    ASSERT_TRUE(ids.count(doc_type_id));
    ASSERT_TRUE(ids.count(doc_type_id + 1));
}

TEST(DocumentTypeRepoTest, requireThatDocumentLookupChecksName)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name_2,
                     Struct(header_name_2), Struct(body_name_2));
    DocumentTypeRepo repo(builder.config());

    // "type_name" will generate the document type id
    // "doc_type_id". However, this config assigns that id to a
    // different type.
    const DocumentType *type = repo.getDocumentType(type_name);
    ASSERT_TRUE(!type);
}

TEST(DocumentTypeRepoTest, requireThatBuildFromConfigWorks)
{
    DocumentTypeRepo repo(readDocumenttypesConfig(TEST_PATH("documenttypes.cfg")));
    ASSERT_TRUE(repo.getDocumentType("document"));
    ASSERT_TRUE(repo.getDocumentType("types"));
    ASSERT_TRUE(repo.getDocumentType("types_search"));
}

TEST(DocumentTypeRepoTest, requireThatStructsCanBeRecursive)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name).setId(header_id).addField(field_name, header_id),
                     Struct(body_name));
    DocumentTypeRepo repo(builder.config());

    const StructDataType &s = repo.getDocumentType(type_name)->getFieldsType();
    ASSERT_EQ(1u, s.getFieldCount());
}

}  // namespace

TEST(DocumentTypeRepoTest, requireThatMissingFileCausesException)
{
    VESPA_EXPECT_EXCEPTION(readDocumenttypesConfig("illegal/missing_file"),
                           IllegalArgumentException, "Unable to open file");
}

TEST(DocumentTypeRepoTest, requireThatFieldsCanHaveAnyDocumentType) {
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name),
                     Struct(body_name).addField(field_name, doc_type_id + 1));
    // Circular dependency
    builder.document(doc_type_id + 1, type_name_2,
                     Struct(header_name_2),
                     Struct(body_name_2).addField(field_name, doc_type_id));
    DocumentTypeRepo repo(builder.config());
    const DocumentType *type1 = repo.getDocumentType(doc_type_id);
    const DocumentType *type2 = repo.getDocumentType(doc_type_id + 1);
    ASSERT_TRUE(type1);
    EXPECT_EQ(type1, repo.getDataType(*type1, doc_type_id));
    EXPECT_EQ(type2, repo.getDataType(*type1, doc_type_id + 1));
    EXPECT_TRUE(type1->getFieldsType().hasField(field_name));
    ASSERT_TRUE(type2);
    EXPECT_EQ(type1, repo.getDataType(*type2, doc_type_id));
    EXPECT_EQ(type2, repo.getDataType(*type2, doc_type_id + 1));
    EXPECT_TRUE(type2->getFieldsType().hasField(field_name));
}

TEST(DocumentTypeRepoTest, requireThatBodyCanOccurBeforeHeaderInConfig)
{
    DocumenttypesConfigBuilderHelper builder;
    // Add header and body in reverse order, then swap the ids.
    builder.document(doc_type_id, type_name,
                     Struct(body_name).setId(body_id).addField("bodystuff",
                             DataType::T_STRING),
                     Struct(header_name).setId(header_id).addField(
                             "headerstuff", DataType::T_INT));
    std::swap(builder.config().documenttype[0].headerstruct,
              builder.config().documenttype[0].bodystruct);

    DocumentTypeRepo repo(builder.config());
    const StructDataType &s = repo.getDocumentType(type_name)->getFieldsType();
    // Should have both fields in fields struct
    EXPECT_TRUE(s.hasField("headerstuff"));
    EXPECT_TRUE(s.hasField("bodystuff"));
}

TEST(DocumentTypeRepoTest, Require_that_Array_can_have_nested_DocumentType)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name, Struct(header_name),
                     Struct(body_name)
                     .addField(field_name, Array(doc_type_id)));
    DocumentTypeRepo repo(builder.config());
    const DocumentType *type = repo.getDocumentType(doc_type_id);
    ASSERT_TRUE(type);
}

TEST(DocumentTypeRepoTest, Reference_fields_are_resolved_to_correct_reference_type)
{
    const int doc_with_refs_id = 5678;
    const int type_2_id = doc_type_id + 1;
    const int ref1_id = 777;
    const int ref2_id = 888;
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name));
    builder.document(type_2_id, type_name_2,
                     Struct(header_name_2), Struct(body_name_2));
    builder.document(doc_with_refs_id, "doc_with_refs",
                     Struct("doc_with_refs.header")
                        .addField("ref1", ref1_id),
                     Struct("doc_with_refs.body")
                        .addField("ref2", ref2_id)
                        .addField("ref3", ref1_id))
        .referenceType(ref1_id, doc_type_id)
        .referenceType(ref2_id, type_2_id);

    DocumentTypeRepo repo(builder.config());
    const DocumentType *type = repo.getDocumentType(doc_with_refs_id);
    ASSERT_TRUE(type != nullptr);
    const auto* ref1_type(repo.getDataType(*type, ref1_id));
    const auto* ref2_type(repo.getDataType(*type, ref2_id));

    EXPECT_EQ(*ref1_type, type->getFieldsType().getField("ref1").getDataType());
    EXPECT_EQ(*ref2_type, type->getFieldsType().getField("ref2").getDataType());
    EXPECT_EQ(*ref1_type, type->getFieldsType().getField("ref3").getDataType());
}

TEST(DocumentTypeRepoTest, Config_with_no_imported_fields_has_empty_imported_fields_set_in_DocumentType)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name));
    DocumentTypeRepo repo(builder.config());
    const auto *type = repo.getDocumentType(doc_type_id);
    ASSERT_TRUE(type != nullptr);
    EXPECT_TRUE(type->imported_field_names().empty());
    EXPECT_FALSE(type->has_imported_field_name("foo"));
}

TEST(DocumentTypeRepoTest, Configured_imported_field_names_are_available_in_the_DocumentType)
{
    const int type_2_id = doc_type_id + 1;
    // Note: we cheat a bit by specifying imported field names in types that have no
    // reference fields. Add to test if we add config read-time validation of this. :)
    DocumenttypesConfigBuilderHelper builder;
    // Type with one imported field
    builder.document(doc_type_id, type_name,
                     Struct(header_name), Struct(body_name))
                     .imported_field("my_cool_field");
    // Type with two imported fields
    builder.document(type_2_id, type_name_2,
                     Struct(header_name_2), Struct(body_name_2))
                     .imported_field("my_awesome_field")
                     .imported_field("my_swag_field");

    DocumentTypeRepo repo(builder.config());
    const auto* type = repo.getDocumentType(doc_type_id);
    ASSERT_TRUE(type != nullptr);
    EXPECT_EQ(1u, type->imported_field_names().size());
    EXPECT_TRUE(type->has_imported_field_name("my_cool_field"));
    EXPECT_FALSE(type->has_imported_field_name("my_awesome_field"));

    type = repo.getDocumentType(type_2_id);
    ASSERT_TRUE(type != nullptr);
    EXPECT_EQ(2u, type->imported_field_names().size());
    EXPECT_TRUE(type->has_imported_field_name("my_awesome_field"));
    EXPECT_TRUE(type->has_imported_field_name("my_swag_field"));
    EXPECT_FALSE(type->has_imported_field_name("my_cool_field"));
}

namespace {

const TensorDataType &
asTensorDataType(const DataType &dataType) {
    return dynamic_cast<const TensorDataType &>(dataType);
}

}

TEST(DocumentTypeRepoTest, Tensor_fields_have_tensor_types)
{
    DocumenttypesConfigBuilderHelper builder;
    builder.document(doc_type_id, type_name, 
                     Struct(header_name),
                     Struct(body_name).
                     addTensorField("tensor1", "tensor(x[3])").
                     addTensorField("tensor2", "tensor(y{})").
                     addTensorField("tensor3", "tensor(x[3])"));
    DocumentTypeRepo repo(builder.config());
    auto *docType = repo.getDocumentType(doc_type_id);
    ASSERT_TRUE(docType != nullptr);
    auto &tensorField1 = docType->getField("tensor1");
    auto &tensorField2 = docType->getField("tensor2");
    EXPECT_EQ("tensor(x[3])", asTensorDataType(tensorField1.getDataType()).getTensorType().to_spec());
    EXPECT_EQ("tensor(y{})", asTensorDataType(tensorField2.getDataType()).getTensorType().to_spec());
    auto &tensorField3 = docType->getField("tensor3");
    EXPECT_TRUE(&tensorField1.getDataType() == &tensorField3.getDataType());
    auto tensorFieldValue1 = tensorField1.getDataType().createFieldValue();
    EXPECT_TRUE(&tensorField1.getDataType() == tensorFieldValue1->getDataType());
}

GTEST_MAIN_RUN_ALL_TESTS()

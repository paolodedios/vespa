# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_library(searchlib_attribute OBJECT
    SOURCES
    address_space_components.cpp
    address_space_usage.cpp
    array_iterator.cpp
    attribute.cpp
    attribute_blueprint_factory.cpp
    attribute_header.cpp
    attribute_object_visitor.cpp
    attribute_operation.cpp
    attribute_read_guard.cpp
    attribute_weighted_set_blueprint.cpp
    attributecontext.cpp
    attributefactory.cpp
    attributefilebufferwriter.cpp
    attributefilesavetarget.cpp
    attributefilewriter.cpp
    attributeguard.cpp
    attributeiterators.cpp
    attributemanager.cpp
    attributememoryfilebufferwriter.cpp
    attributememoryfilewriter.cpp
    attributememorysavetarget.cpp
    attributesaver.cpp
    attributevector.cpp
    attrvector.cpp
    basename.cpp
    bitvector_search_cache.cpp
    blob_sequence_reader.cpp
    changevector.cpp
    configconverter.cpp
    copy_multi_value_read_view.cpp
    createarrayfastsearch.cpp
    createarraystd.cpp
    createsetfastsearch.cpp
    createsetstd.cpp
    createsinglefastsearch.cpp
    createsinglestd.cpp
    defines.cpp
    dfa_fuzzy_matcher.cpp
    dfa_string_comparator.cpp
    direct_multi_term_blueprint.cpp
    distance_metric_utils.cpp
    diversity.cpp
    empty_search_context.cpp
    enum_store_compaction_spec.cpp
    enum_store_dictionary.cpp
    enum_store_loaders.cpp
    enumattribute.cpp
    enumattributesaver.cpp
    enumcomparator.cpp
    enumerated_multi_value_read_view.cpp
    enumhintsearchcontext.cpp
    enummodifier.cpp
    enumstore.cpp
    extendable_numeric_array_multi_value_read_view.cpp
    extendable_numeric_weighted_set_multi_value_read_view.cpp
    extendable_string_array_multi_value_read_view.cpp
    extendable_string_weighted_set_multi_value_read_view.cpp
    extendableattributes.cpp
    fixedsourceselector.cpp
    flagattribute.cpp
    floatbase.cpp
    i_direct_posting_store.cpp
    i_enum_store.cpp
    iattributemanager.cpp
    iattributesavetarget.cpp
    imported_attribute_vector.cpp
    imported_attribute_vector_factory.cpp
    imported_attribute_vector_read_guard.cpp
    imported_multi_value_read_view.cpp
    imported_search_context.cpp
    integerbase.cpp
    ipostinglistsearchcontext.cpp
    load_utils.cpp
    loadedenumvalue.cpp
    loadednumericvalue.cpp
    loadedvalue.cpp
    make_sort_blob_writer.cpp
    multi_enum_search_context.cpp
    multi_numeric_enum_search_context.cpp
    multi_numeric_flag_search_context.cpp
    multi_numeric_search_context.cpp
    multi_string_enum_hint_search_context.cpp
    multi_string_enum_search_context.cpp
    multi_term_or_filter_search.cpp
    multi_value_mapping.cpp
    multi_value_mapping_base.cpp
    multienumattribute.cpp
    multienumattributesaver.cpp
    multinumericattribute.cpp
    multinumericattributesaver.cpp
    multinumericenumattribute.cpp
    multinumericpostattribute.cpp
    multistringattribute.cpp
    multistringpostattribute.cpp
    multivalueattribute.cpp
    multivalueattributesaver.cpp
    multivalueattributesaverutils.cpp
    not_implemented_attribute.cpp
    numeric_matcher.cpp
    numeric_range_matcher.cpp
    numeric_search_context.cpp
    numeric_sort_blob_writer.cpp
    numericbase.cpp
    posting_iterator_pack.cpp
    posting_list_merger.cpp
    postingchange.cpp
    postinglistattribute.cpp
    postinglistsearchcontext.cpp
    postinglisttraits.cpp
    postingstore.cpp
    predicate_attribute.cpp
    predicate_attribute_saver.cpp
    raw_attribute.cpp
    raw_buffer_store.cpp
    raw_buffer_store_reader.cpp
    raw_buffer_store_writer.cpp
    raw_multi_value_read_view.cpp
    readerbase.cpp
    reference_attribute.cpp
    reference_attribute_saver.cpp
    reference_mappings.cpp
    save_utils.cpp
    search_context.cpp
    searchcontextelementiterator.cpp
    single_enum_search_context.cpp
    single_numeric_enum_search_context.cpp
    single_numeric_search_context.cpp
    single_numeric_sort_blob_writer.cpp
    single_raw_attribute.cpp
    single_raw_attribute_loader.cpp
    single_raw_attribute_saver.cpp
    single_raw_ext_attribute.cpp
    single_small_numeric_search_context.cpp
    single_string_enum_hint_search_context.cpp
    single_string_enum_search_context.cpp
    singleboolattribute.cpp
    singleenumattribute.cpp
    singleenumattributesaver.cpp
    singlenumericattribute.cpp
    singlenumericattributesaver.cpp
    singlenumericenumattribute.cpp
    singlenumericpostattribute.cpp
    singlesmallnumericattribute.cpp
    singlestringattribute.cpp
    singlestringpostattribute.cpp
    sourceselector.cpp
    string_matcher.cpp
    string_search_context.cpp
    string_search_helper.cpp
    string_sort_blob_writer.cpp
    string_to_number.cpp
    stringbase.cpp
    valuemodifier.cpp
    DEPENDS
)

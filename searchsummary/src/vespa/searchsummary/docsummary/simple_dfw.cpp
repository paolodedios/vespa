// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "simple_dfw.h"

namespace search::docsummary {

void
SimpleDFW::insert_field(uint32_t docid, const IDocsumStoreDocument *, GetDocsumsState& state,
                        ElementIds, vespalib::slime::Inserter &target) const
{
    insertField(docid, state, target);
}

}

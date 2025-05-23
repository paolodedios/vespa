// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "tensor_nodes.h"
#include "node_visitor.h"

namespace vespalib::eval::nodes {

void TensorMap            ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorMapSubspaces   ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorFilterSubspaces::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorJoin           ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorMerge          ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorReduce         ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorRename         ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorConcat         ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorCellCast       ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorCellOrder      ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorCreate         ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorLambda         ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }
void TensorPeek           ::accept(NodeVisitor &visitor) const { visitor.visit(*this); }

std::string
TensorMap::dump(DumpContext &ctx) const {
    std::string str;
    str += "map(";
    str += _child->dump(ctx);
    str += ",";
    str += _lambda->dump_as_lambda();
    str += ")";
    return str;
}

std::string
TensorMapSubspaces::dump(DumpContext &ctx) const {
    std::string str;
    str += "map_subspaces(";
    str += _child->dump(ctx);
    str += ",";
    str += _lambda->dump_as_lambda();
    str += ")";
    return str;
}

std::string
TensorFilterSubspaces::dump(DumpContext &ctx) const {
    std::string str;
    str += "filter_subspaces(";
    str += _child->dump(ctx);
    str += ",";
    str += _lambda->dump_as_lambda();
    str += ")";
    return str;
}

std::string
TensorJoin::dump(DumpContext &ctx) const {
    std::string str;
    str += "join(";
    str += _lhs->dump(ctx);
    str += ",";
    str += _rhs->dump(ctx);
    str += ",";
    str += _lambda->dump_as_lambda();
    str += ")";
    return str;
}

std::string
TensorMerge::dump(DumpContext &ctx) const {
    std::string str;
    str += "join(";
    str += _lhs->dump(ctx);
    str += ",";
    str += _rhs->dump(ctx);
    str += ",";
    str += _lambda->dump_as_lambda();
    str += ")";
    return str;
}

std::string
TensorReduce::dump(DumpContext &ctx) const {
    std::string str;
    str += "reduce(";
    str += _child->dump(ctx);
    str += ",";
    str += *AggrNames::name_of(_aggr);
    for (const auto &dimension: _dimensions) {
        str += ",";
        str += dimension;
    }
    str += ")";
    return str;
}

std::string
TensorRename::dump(DumpContext &ctx) const  {
    std::string str;
    str += "rename(";
    str += _child->dump(ctx);
    str += ",";
    str += flatten(_from);
    str += ",";
    str += flatten(_to);
    str += ")";
    return str;
}

std::string
TensorRename::flatten(const std::vector<std::string> &list) {
    if (list.size() == 1) {
        return list[0];
    }
    std::string str = "(";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) {
            str += ",";
        }
        str += list[i];
    }
    str += ")";
    return str;
}

std::string
TensorConcat::dump(DumpContext &ctx) const {
    std::string str;
    str += "concat(";
    str += _lhs->dump(ctx);
    str += ",";
    str += _rhs->dump(ctx);
    str += ",";
    str += _dimension;
    str += ")";
    return str;
}

std::string
TensorCellCast::dump(DumpContext &ctx) const {
    std::string str;
    str += "cell_cast(";
    str += _child->dump(ctx);
    str += ",";
    str += value_type::cell_type_to_name(_cell_type);
    str += ")";
    return str;
}

std::string
TensorCellOrder::dump(DumpContext &ctx) const {
    std::string str;
    str += "cell_order(";
    str += _child->dump(ctx);
    str += ",";
    str += as_string(_cell_order);
    str += ")";
    return str;
}

std::string
TensorCreate::dump(DumpContext &ctx) const {
    std::string str = _type.to_spec();
    str += ":{";
    CommaTracker child_list;
    for (const Child &child: _cells) {
        child_list.maybe_add_comma(str);
        str += as_string(child.first);
        str += ":";
        str += child.second->dump(ctx);
    }
    str += "}";
    return str;
}

std::string
TensorLambda::dump(DumpContext &) const {
    std::string str = _type.to_spec();
    std::string expr = _lambda->dump();
    if (expr.starts_with("(")) {
        str += expr;
    } else {
        str += "(";
        str += expr;
        str += ")";
    }
    return str;
}

std::string
TensorPeek::dump(DumpContext &ctx) const {
    std::string str = _param->dump(ctx);
    str += "{";
    CommaTracker dim_list;
    for (const auto &dim : _dim_list) {
        dim_list.maybe_add_comma(str);
        str += dim.first;
        str += ":";
        if (dim.second.is_expr()) {
            std::string expr = dim.second.expr->dump(ctx);
            if (expr.starts_with("(")) {
                str += expr;
            } else {
                str += "(";
                str += expr;
                str += ")";
            }
        } else {
            str += as_quoted_string(dim.second.label);
        }
    }
    str += "}";
    return str;
}

}

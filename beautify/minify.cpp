#include "minify.hpp"
#include "Luau/Ast.h"
#include "beautify.hpp"
#include "solve.hpp"

#include "Luau/Lexer.h"

#define convert minify

#define optionalSpace \
if (!isSpace(result[result.length() - 1]) && result[result.length() - 1] != ';') \
    result.append(" ")

#define minifyFunction(expr_function) \
    tuple(expr_function->args, AstLocal); \
    if (expr_function->vararg) { \
        result.erase(result.size() - 1, 1); \
        if (expr_function->args.size > 0) \
            result.append(","); \
        result.append("...)"); \
    }; \
    m_dont_append_do = true; \
    result.append(minify(expr_function->body)); \
    optionalSpace; \
    result.append("end")

std::string minify(AstLocal* local) {
    return local->name.value;
};


#ifdef listBody
    #undef listBody
#endif

#define listBody(array, type) \
for (type* obj : array) { \
    list_index++; \
    result.append(convert(obj)); \
    if (list_index < list_size) { \
        result += ','; \
    }; \
}

bool m_is_root = true; // aka is first minify call
bool m_dont_append_do = false;

std::string minify(Luau::AstNode* node) {
    std::string result = "";

    if (AstExpr* expr = node->asExpr()) {
        if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {
            result = '(';
            if (isSolvable(expr_group)) {
                appendSolve(expr_group, minify);
            } else {
                result.append(minify(expr_group->expr));
            };
            result.append(")");
        } else if (AstExprConstantNil* expr_nil = expr->as<AstExprConstantNil>()) {
            result = "nil";
        } else if (AstExprConstantBool* expr_bool = expr->as<AstExprConstantBool>()) {
            result = expr_bool->value ? "true" : "false";
        } else if (AstExprConstantNumber* expr_number = expr->as<AstExprConstantNumber>()) {
            result = convertNumber(expr_number->value);
        } else if (AstExprConstantString* expr_string = expr->as<AstExprConstantString>()) {
            result = fixString(expr_string->value);
        } else if (AstExprLocal* expr_local = expr->as<AstExprLocal>()) {
            result = expr_local->local->name.value;
        } else if (AstExprGlobal* expr_global = expr->as<AstExprGlobal>()) {
            result = expr_global->name.value;
        } else if (AstExprVarargs* expr_varargs = expr->as<AstExprVarargs>()) {
            result = "...";
        } else if (AstExprCall* expr_call = expr->as<AstExprCall>()) {
            result = minify(expr_call->func);
            tuple(expr_call->args, AstExpr);
        } else if (AstExprIndexName* expr_index_name = expr->as<AstExprIndexName>()) {
            result = minify(expr_index_name->expr);
            result.append(std::string{expr_index_name->op});
            result.append(expr_index_name->index.value);
        } else if (AstExprIndexExpr* expr_index_expr = expr->as<AstExprIndexExpr>()) {
            result = minify(expr_index_expr->expr);
            result.append("[");
            result.append(minify(expr_index_expr->index));
            result.append("]");
        } else if (AstExprFunction* expr_function = expr->as<AstExprFunction>()) {
            result = "function";
            minifyFunction(expr_function);
        } else if (AstExprTable* expr_table = expr->as<AstExprTable>()) {
            size_t size = expr_table->items.size;
            if (size > 0) {
                result = "{";

                int index = 0;
                for (AstExprTable::Item item : expr_table->items) {
                    index++;

                    switch (item.kind) {
                        case AstExprTable::Item::Kind::List:
                            result.append(minify(item.value));
                            break;
                        case AstExprTable::Item::Kind::Record:
                            result.append(item.key->as<AstExprConstantString>()->value.data);
                            result.append("=");
                            result.append(minify(item.value));
                            break;
                        case AstExprTable::Item::Kind::General:
                            result.append("[");
                            result.append(minify(item.key));
                            result.append("]=");
                            result.append(minify(item.value));
                            break;
                    };

                    if (index < size)
                        result.append(",");
                };

                result.append("}");
            } else {
                result.append("{}");
            };
        } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
            if (isSolvable(expr_unary)) {
                appendSolve(expr, minify);
            } else {
                result.append(unary_operators[expr_unary->op]);
                result.append(minify(expr_unary->expr));
            }
        } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
            if (isSolvable(expr_binary)) {
                appendSolve(expr_binary, minify);
            } else {
                const char* space = (expr_binary->op == AstExprBinary::And || expr_binary->op == AstExprBinary::Or) ? " " : "";
                result.append(minify(expr_binary->left));
                result.append(space);
                result.append(binary_operators[expr_binary->op]);
                result.append(space);
                result.append(minify(expr_binary->right));
            }
        } else if (AstExprIfElse* expr_if_else = expr->as<AstExprIfElse>()) {
            result.append("if ");
            result.append(minify(expr_if_else->condition));
            result.append(" then ");
            result.append(minify(expr_if_else->trueExpr));
            result.append(" else ");
            result.append(minify(expr_if_else->falseExpr));
        } else if (AstExprInterpString* expr_interp_string = expr->as<AstExprInterpString>()) {
            result.append("`");

            int index = 0;
            size_t size = expr_interp_string->strings.size;
            for (AstArray<char> string : expr_interp_string->strings) {
                index++;
                result.append(string.data);
                if (index < size) {
                    result.append("{");
                    result.append(minify(expr_interp_string->expressions.data[index - 1]));
                    result.append("}");
                };
            };

            result.append("`");
        } else {
            result.append("--[[ error: unknown expression type! ]]");
        };
    } else if (AstStat* stat = node->asStat()) {
        if (AstStatBlock* stat2 = stat->as<AstStatBlock>()) {
            bool append_do = stat2->hasEnd && !m_dont_append_do;
            if (m_is_root) {
                append_do = false;
                m_is_root = false;
            };

            if (append_do)
                result.append("do ");

            m_dont_append_do = false;

            for (AstStat* child : stat2->body) {
                result.append(minify(child));
            };

            if (append_do) {
                optionalSpace;
                result.append("end;");
            };
        } else if (AstStatIf* stat_if = stat->as<AstStatIf>()) {
            result.append("if ");
            result.append(minify(stat_if->condition));
            result.append(" then ");

            m_dont_append_do = true;
            result.append(minify(stat_if->thenbody));

            if (stat_if->elsebody) {
                optionalSpace;
                result.append("else ");
                m_dont_append_do = true;
                result.append(minify(stat_if->elsebody));
            }

            optionalSpace;
            result.append("end;");
        } else if (AstStatWhile* stat_while = stat->as<AstStatWhile>()) {
            result.append("while ");
            result.append(minify(stat_while->condition));
            result.append(" do ");

            m_dont_append_do = true;
            result.append(minify(stat_while->body));

            optionalSpace;
            result.append("end;");
        } else if (AstStatRepeat* stat_repeat = stat->as<AstStatRepeat>()) {
            result.append("repeat ");

            m_dont_append_do = true;
            result.append(minify(stat_repeat->body));

            optionalSpace;
            result.append("until ");
            result.append(minify(stat_repeat->condition));
            result.append(";");
        } else if (AstStatBreak* stat_break = stat->as<AstStatBreak>()) {
            result.append("break;");
        } else if (AstStatContinue* stat_break = stat->as<AstStatContinue>()) {
            result.append("continue;");
        } else if (AstStatReturn* stat_return = stat->as<AstStatReturn>()) {
            result.append("return");

            if (stat_return->list.size > 0)
                result.append(" ");

            astlist(stat_return->list, AstExpr);
            result.append(";");
        } else if (AstStatExpr* stat_expr = stat->as<AstStatExpr>()) {
            result.append(minify(stat_expr->expr));
            result.append(";");
        } else if (AstStatLocal* stat_local = stat->as<AstStatLocal>()) {
            result.append("local ");
            astlist(stat_local->vars, AstLocal);
            if (stat_local->values.size > 0) {
                result.append("=");
                astlist2(stat_local->values, AstExpr);
            };
            result.append(";");
        } else if (AstStatFor* stat_for = stat->as<AstStatFor>()) {
            result.append("for ");
            result.append(minify(stat_for->var));
            result.append("=");
            result.append(minify(stat_for->from));
            result += ',';
            result.append(minify(stat_for->to));
            if (stat_for->step) {
                result += ',';
                result.append(minify(stat_for->step));
            };

            result.append(" do ");

            m_dont_append_do = true;
            result.append(minify(stat_for->body));

            optionalSpace;
            result.append("end;");
        } else if (AstStatForIn* stat_for_in = stat->as<AstStatForIn>()) {
            result.append("for ");
            astlist(stat_for_in->vars, AstLocal);
            result.append(" in ");
            astlist2(stat_for_in->values, AstExpr);
            result.append(" do ");

            m_dont_append_do = true;
            result.append(minify(stat_for_in->body));

            optionalSpace;
            result.append("end;");
        } else if (AstStatAssign* stat_assign = stat->as<AstStatAssign>()) {
            astlist(stat_assign->vars, AstExpr);
            if (stat_assign->values.size > 0) {
                result.append("=");
                astlist2(stat_assign->values, AstExpr);
            };
            result.append(";");
        } else if (AstStatCompoundAssign* stat_compound_assign = stat->as<AstStatCompoundAssign>()) {
            result.append(minify(stat_compound_assign->var));
            // result.append(" ");
            result.append(binary_operators[stat_compound_assign->op]);
            result.append("=");
            result.append(minify(stat_compound_assign->value));
            result.append(";");
        } else if (AstStatFunction* stat_function = stat->as<AstStatFunction>()) {
            if (AstExprIndexName* expr_index_name = stat_function->name->as<AstExprIndexName>(); expr_index_name->op == ':') {
                result.append("function ");
                result.append(minify(expr_index_name));
                minifyFunction(stat_function->func);
            } else {
                result.append(minify(stat_function->name));
                result.append("=");
                result.append(minify(stat_function->func));
                result.append(";");
            }
        } else if (AstStatLocalFunction* stat_local_function = stat->as<AstStatLocalFunction>()) {
            result.append("local function ");
            result.append(minify(stat_local_function->name));
            minifyFunction(stat_local_function->func);
            result.append(";");
        } else {
            result.append("--[[ error: unknown stat type! ]]");
        };
    };

    return result;
};

std::string minifyRoot(Luau::AstStatBlock* root, bool nosolve, bool ignore_types) {
    setupSolve(nosolve, ignore_types);
    return minify(root);
};
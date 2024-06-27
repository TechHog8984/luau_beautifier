#include "beautify.hpp"
#include "solve.hpp"

#include <cstring>
#include <string>

#include "math.h"

#include "Luau/Lexer.h"

using namespace Luau;

#define convert beautify

#define addIndents \
if (skip_first_indent) indent--; \
for (int _ = 0; _ < indent; _++) { \
    result.append("    "); \
}; \
if (skip_first_indent) { \
    indent++; \
    skip_first_indent = false; \
}

#define optionalNewlinePre \
int optional_newline_index = result.length() - 1

#define optionalNewlineBody \
while (isSpace(result[optional_newline_index])) \
    optional_newline_index--; \
if (result[optional_newline_index + 1] != '\n') { \
    result.append("\n"); \
} \
addIndents

#define optionalNewline \
optionalNewlinePre; \
optionalNewlineBody

#define optionalNewline2 \
optional_newline_index = result.length() - 1; \
optionalNewlineBody


std::string fixString(AstArray<char> value) {
    std::string result = "\"";

    for (char ch : value) {
        if (ch > 31 && ch < 127 && ch != '"' && ch != '\\')
            result.append(std::string{ch});
        else {
            switch (ch) {
                case 7:
                    result.append("\\a");
                    break;
                case 8:
                    result.append("\\b");
                    break;
                case 9:
                    result.append("\\t");
                    break;
                case 10:
                    result.append("\\n");
                    break;
                case 11:
                    result.append("\\v");
                    break;
                case 12:
                    result.append("\\f");
                    break;
                case 13:
                    result.append("\\r");
                    break;

                case '"':
                    result.append("\\\"");
                    break;
                case '\\':
                    result.append("\\\\");
                    break;

                default:
                    result.append("\\");

                    unsigned char n = (unsigned char) ch;
                    if (n < 10)
                        result.append("00");
                    else if (n < 100)
                        result.append("0");

                    result.append(std::to_string((unsigned char)ch));
            };
        };
    };

    result.append("\"");

    return result;
};

std::string convertNumber(double value) {
    double decimal, integer;

    decimal = modf(value, &integer);

    std::string result;

    if (decimal == 0) {
        result = std::to_string(integer);
    } else {
        char str[500];
        sprintf(str, "%.15f", value);

        result = str;
    };

    if (result == "inf") {
        result = "math.huge";
    } else {
        while (result.length() > 2 && result[result.length() - 1] == '0')
            result.erase(result.length() - 1, 1);

        if (result[result.length() - 1] == '.')
            result.erase(result.length() - 1, 1);
    };

    return result;
};

std::string beautify(AstLocal* local) {
    return local->name.value;
};

int indent = 0;
bool skip_first_indent = false;
bool b_is_root = true; // aka is first beautify call
bool b_dont_append_do = false;

std::string beautify(AstNode* node) {
    std::string result = "";

    if (AstExpr* expr = node->asExpr()) {
        if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {
            result = '(';
            if (isSolvable(expr_group)) {
                result.append(convertNumber(solve(expr_group)));
            } else {
                result.append(beautify(expr_group->expr));
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
            result = beautify(expr_call->func);
            tuple(expr_call->args, AstExpr);
        } else if (AstExprIndexName* expr_index_name = expr->as<AstExprIndexName>()) {
            result = beautify(expr_index_name->expr);
            result.append(std::string{expr_index_name->op});
            result.append(expr_index_name->index.value);
        } else if (AstExprIndexExpr* expr_index_expr = expr->as<AstExprIndexExpr>()) {
            result = beautify(expr_index_expr->expr);
            result.append("[");
            result.append(beautify(expr_index_expr->index));
            result.append("]");
        } else if (AstExprFunction* expr_function = expr->as<AstExprFunction>()) {
            result = "function";
            tuple(expr_function->args, AstLocal);
            if (expr_function->vararg) {
                result.erase(result.size() - 1, 1);
                if (expr_function->args.size > 0)
                    result.append(", ");
                result.append("...)");
            };
            result.append("\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(expr_function->body));
            indent--;

            optionalNewline;
            result.append("end");
        } else if (AstExprTable* expr_table = expr->as<AstExprTable>()) {
            size_t size = expr_table->items.size;
            if (size > 0) {
                result = "{\n";

                indent++;
                int index = 0;
                for (AstExprTable::Item item : expr_table->items) {
                    index++;
                    addIndents;

                    switch (item.kind) {
                        case AstExprTable::Item::Kind::List:
                            result.append(beautify(item.value));
                            break;
                        case AstExprTable::Item::Kind::Record:
                            result.append(item.key->as<AstExprConstantString>()->value.data);
                            result.append(" = ");
                            result.append(beautify(item.value));
                            break;
                        case AstExprTable::Item::Kind::General:
                            result.append("[");
                            result.append(beautify(item.key));
                            result.append("] = ");
                            result.append(beautify(item.value));
                            break;
                    };

                    if (index < size)
                        result.append(",");

                    result.append("\n");
                };
                indent--;

                addIndents;
                result.append("}");
            } else {
                result.append("{}");
            };
        } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
            if (isSolvable(expr_unary)) {
                result.append(convertNumber(solve(expr_unary)));
            } else {
                result.append(unary_operators[expr_unary->op]);
                result.append(beautify(expr_unary->expr));
            }
        } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
            if (isSolvable(expr_binary)) {
                result.append(convertNumber(solve(expr_binary)));
            } else {
                result.append(beautify(expr_binary->left));
                result.append(" ");
                result.append(binary_operators[expr_binary->op]);
                result.append(" ");
                result.append(beautify(expr_binary->right));
            }
        } else if (AstExprIfElse* expr_if_else = expr->as<AstExprIfElse>()) {
            result.append("if ");
            result.append(beautify(expr_if_else->condition));
            result.append(" then ");
            result.append(beautify(expr_if_else->trueExpr));
            result.append(" else ");
            result.append(beautify(expr_if_else->falseExpr));
        } else if (AstExprInterpString* expr_interp_string = expr->as<AstExprInterpString>()) {
            result.append("`");

            int index = 0;
            size_t size = expr_interp_string->strings.size;
            for (AstArray<char> string : expr_interp_string->strings) {
                index++;
                result.append(string.data);
                if (index < size) {
                    result.append("{");
                    result.append(beautify(expr_interp_string->expressions.data[index - 1]));
                    result.append("}");
                };
            };

            result.append("`");
        } else {
            result.append("--[[ error: unknown expression type! ]]");
        };
    } else if (AstStat* stat = node->asStat()) {
        if (AstStatBlock* stat2 = stat->as<AstStatBlock>()) {
            bool append_do = stat2->hasEnd && !b_dont_append_do;
            if (b_is_root) {
                append_do = false;
                b_is_root = false;
            };

            if (append_do) {
                addIndents;
                result.append("do\n");
                indent++;
            };
            b_dont_append_do = false;

            for (AstStat* child : stat2->body) {
                result.append(beautify(child));
                result.append("\n");
            };

            if (append_do) {
                indent--;
                optionalNewline;
                result.append("end;");
            };
        } else if (AstStatIf* stat_if = stat->as<AstStatIf>()) {
            addIndents;
            result.append("if ");
            result.append(beautify(stat_if->condition));
            result.append(" then\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(stat_if->thenbody));
            indent--;

            if (stat_if->elsebody) {
                optionalNewline;
                result.append("else\n");
                indent++;
                result.append(beautify(stat_if->elsebody));
                indent--;
            }

            optionalNewline;
            result.append("end;");
        } else if (AstStatWhile* stat_while = stat->as<AstStatWhile>()) {
            addIndents;
            result.append("while ");
            result.append(beautify(stat_while->condition));
            result.append(" do\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(stat_while->body));
            indent--;

            optionalNewline;
            result.append("end;");
        } else if (AstStatRepeat* stat_repeat = stat->as<AstStatRepeat>()) {
            addIndents;
            result.append("repeat\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(stat_repeat->body));
            indent--;

            optionalNewline;
            result.append("until ");
            result.append(beautify(stat_repeat->condition));
            result.append(";");
        } else if (AstStatBreak* stat_break = stat->as<AstStatBreak>()) {
            addIndents;
            result.append("break;");
        } else if (AstStatContinue* stat_break = stat->as<AstStatContinue>()) {
            addIndents;
            result.append("continue;");
        } else if (AstStatReturn* stat_return = stat->as<AstStatReturn>()) {
            addIndents;
            result.append("return");

            if (stat_return->list.size > 0)
                result.append(" ");

            astlist(stat_return->list, AstExpr);
            result.append(";");
        } else if (AstStatExpr* stat_expr = stat->as<AstStatExpr>()) {
            addIndents;
            result.append(beautify(stat_expr->expr));
            result.append(";");
        } else if (AstStatLocal* stat_local = stat->as<AstStatLocal>()) {
            addIndents;
            result.append("local ");
            astlist(stat_local->vars, AstLocal);
            if (stat_local->values.size > 0) {
                result.append(" = ");
                astlist2(stat_local->values, AstExpr);
            };
            result.append(";");
        } else if (AstStatFor* stat_for = stat->as<AstStatFor>()) {
            addIndents;
            result.append("for ");
            result.append(beautify(stat_for->var));
            result.append(" = ");
            result.append(beautify(stat_for->from));
            result.append(", ");
            result.append(beautify(stat_for->to));
            if (stat_for->step) {
                result.append(", ");
                result.append(beautify(stat_for->step));
            };

            result.append(" do\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(stat_for->body));
            indent--;

            optionalNewline;
            result.append("end;");
        } else if (AstStatForIn* stat_for_in = stat->as<AstStatForIn>()) {
            addIndents;
            result.append("for ");
            astlist(stat_for_in->vars, AstLocal);
            result.append(" in ");
            astlist2(stat_for_in->values, AstExpr);
            result.append(" do\n");

            indent++;
            b_dont_append_do = true;
            result.append(beautify(stat_for_in->body));
            indent--;

            optionalNewline;
            result.append("end;");
        } else if (AstStatAssign* stat_assign = stat->as<AstStatAssign>()) {
            addIndents;
            astlist(stat_assign->vars, AstExpr);
            if (stat_assign->values.size > 0) {
                result.append(" = ");
                astlist2(stat_assign->values, AstExpr);
            };
            result.append(";");
        } else if (AstStatCompoundAssign* stat_compound_assign = stat->as<AstStatCompoundAssign>()) {
            addIndents;
            result.append(beautify(stat_compound_assign->var));
            result.append(" ");
            result.append(binary_operators[stat_compound_assign->op]);
            result.append("= ");
            result.append(beautify(stat_compound_assign->value));
            result.append(";");
        } else if (AstStatFunction* stat_function = stat->as<AstStatFunction>()) {
            addIndents;
            result.append(beautify(stat_function->name));
            result.append(" = ");
            result.append(beautify(stat_function->func));
            result.append(";");
        } else if (AstStatLocalFunction* stat_local_function = stat->as<AstStatLocalFunction>()) {
            addIndents;
            result.append("local ");
            result.append(beautify(stat_local_function->name));
            result.append(" = ");
            result.append(beautify(stat_local_function->func));
            result.append(";");
        } else {
            result.append("--[[ error: unknown stat type! ]]");
        };
    };

    return result;
};

std::string beautifyRoot(AstStatBlock* root, bool nosolve_in) {
    setNoSolve(nosolve_in);
    return beautify(root);
};
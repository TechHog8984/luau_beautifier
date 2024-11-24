#include "beautify.hpp"
#include "Luau/Ast.h"
#include "solve.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "math.h"

#include "Luau/Lexer.h"

using namespace Luau;

#define convert beautify

#define addIndents { \
    int old_indent = indent; \
    if (skip_first_indent) indent--; \
    else if (b_ignore_indent) indent = 0; \
    for (int _ = 0; _ < indent; _++) { \
        result.append("    "); \
    }; \
    if (skip_first_indent) { \
        indent++; \
        skip_first_indent = false; \
    } else if (b_ignore_indent) { \
        indent = old_indent; \
        b_ignore_indent = false; \
    } \
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


#define beautifyFunction(expr) \
tuple(expr->args, AstLocal); \
if (expr->vararg) { \
    result.erase(result.size() - 1, 1); \
    if (expr->args.size > 0) \
        result.append(", "); \
    result.append("...)"); \
}; \
result.append("\n"); \
indent++; \
b_dont_append_do = true; \
result.append(beautify(expr->body)); \
indent--; \
optionalNewline; \
result.append("end")

Injection INJECTION_NONE {};

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

std::string beautify(AstLocal* local) {
    return local->name.value;
};

int indent = 0;
bool skip_first_indent = false;
int skip_count = -1;
bool b_is_root = true; // aka is first beautify call
bool b_dont_append_do = false;
bool b_ignore_indent = false;
bool b_dont_append_end = false;
bool b_inside_group = false;

std::string getIndents(int offset) {
    std::string result = "";
    for (int _ = 0; _ < indent + offset; _++) {
        result.append("    ");
    };

    return result;
};

InjectCallback* inject_callback;
void* inject_callback_data;

void setupInjectCallback(InjectCallback* callback, void* data) {
    inject_callback = callback;
    inject_callback_data = data;
};

void dontAppendDo() {
    b_dont_append_do = true;
};

bool replace_if_expressions;
bool extra1;

void replaceIfElse(std::string* out, AstExprIfElse* expr, std::string var, bool use_local = false);

/*
    obfuscators commonly employ techniques to make control flow hard to read
    one of these is a for loop with a break at the end and no continue
    example:
    for i = 1, 10 do
        // code here
        break
    end
    this can be simpilfied to just
    // code here

    to detect these loops we take advantage of AstVisitors
*/
class DummyForLoopVisitor : public AstVisitor {
    bool first_run = true;

    public:
    const char* var = nullptr;
    bool success = false;
    DummyForLoopVisitor() {}

    bool visit(AstExprLocal* expr_local) override {
        if (strcmp(var, expr_local->local->name.value) == 0)
            success = false;
        return true;
    }
    bool visit(AstStatContinue* stat_continue) override {
        success = false;
        return true;
    }
    bool visit(AstStatFor* stat_for) override {
        if (first_run) {
            if (stat_for->body->body.size > 0 && stat_for->body->body.data[stat_for->body->body.size - 1]->is<AstStatBreak>())
                success = true;
        }
        first_run = false;
        return true;
    }
};

struct StatementExtractionResult {
    const char* counter_name = nullptr;
    // AstArray<AstStat*> list; // copy on a TempVector

    struct Branch {
        bool is_end = false;
        double target;
        double condition;
    };
    std::vector<Branch> branch_list;
};

bool testBinaryWithVariable(AstExprBinary* expr, const char* variable, double num) {
    bool num_is_left = false;

    // AstExpr* variable_expr = nullptr;
    AstExprConstantNumber* num_expr = nullptr;

    if (AstExprLocal* left_local = getRootExpr(expr->left)->as<AstExprLocal>()) {
        if (strcmp(variable, left_local->local->name.value) != 0)
            return false;
        // else
        //     variable_expr = left_local;
    } else if (AstExprLocal* right_local = getRootExpr(expr->right)->as<AstExprLocal>()) {
        if (strcmp(variable, right_local->local->name.value) != 0)
            return false;
        // else
        //     variable_expr = right_local;
    }

    if (AstExprConstantNumber* left_number = getRootExpr(expr->left)->as<AstExprConstantNumber>()) {
        num_expr = left_number;
        num_is_left = true;
    } else if (AstExprConstantNumber* right_number = getRootExpr(expr->left)->as<AstExprConstantNumber>()) {
        num_expr = right_number;
    }

    double expr_num = num_expr->value;

    switch (expr->op) {
        case AstExprBinary::CompareNe: {
            return num != expr_num;
        }
        case AstExprBinary::CompareEq: {
            return num == expr_num;
        }
        case AstExprBinary::CompareLt: {
            break;
        }
        case AstExprBinary::CompareLe: {
            break;
        }
        case AstExprBinary::CompareGt: {
            break;
        }
        case AstExprBinary::CompareGe: {
            break;
        }
        default:
            break;
    }

    return false;
}

void handleStatementExtractionIf(AstStatIf* stat, StatementExtractionResult& result, double num) {
    auto then_body = stat->thenbody->body;

    AstStat* else_body = stat->elsebody;
    if (else_body) {
        if (AstStatIf* inner = else_body->as<AstStatIf>())
            handleStatementExtractionIf(inner, result, num);
    }

    if (then_body.size < 1)
        return;

    if (then_body.size == 1)
        if (AstStatIf* inner = then_body.data[0]->as<AstStatIf>())
            return handleStatementExtractionIf(inner, result, num);

    AstExprBinary* condition = getRootExpr(stat->condition)->as<AstExprBinary>();
    if (!condition)
        return;

    StatementExtractionResult::Branch branch;
    if (testBinaryWithVariable(condition, result.counter_name, num)) {
        branch.condition = num;
        // branch.target = ;
    }

    bool is_end = false;
    AstStat* last_stat = then_body.data[then_body.size - 1];
    if (last_stat->is<AstStatBreak>())
        is_end = true;

    branch.is_end = is_end;

    result.branch_list.push_back(branch);
}

bool attemptStatementExtraction(AstStatBlock* block) {
    auto block_body = block->body;
    if (block_body.size < 2) {
        return false;
    }

    const char* counter_name = nullptr;
    double counter_initial;

    AstStat* first_stat = block_body.data[0];
    if (AstStatLocal* first_stat = first_stat->as<AstStatLocal>()) {
        auto vars = first_stat->vars;
        auto values = first_stat->values;
        if (values.size > 0)
            if (AstExprConstantNumber* expr_number = getRootExpr(values.data[0])->as<AstExprConstantNumber>()) {
                counter_name = vars.data[0]->name.value;
                counter_initial = expr_number->value;
            }
    }

    if (counter_name == nullptr)
        return false;

    StatementExtractionResult result;
    result.counter_name = counter_name;

    AstStat* second_stat = block_body.data[1];
    if (AstStatWhile* second_stat = second_stat->as<AstStatWhile>()) {
        auto second_body = second_stat->body->body;
        if (second_body.size > 0)
            if (AstStatIf* third_stat = second_body.data[0]->as<AstStatIf>()) {
                handleStatementExtractionIf(third_stat, result, counter_initial);
            }
    }

    printf("count: %zu\n", result.branch_list.size());

    return false;
}

std::string beautify(AstNode* node) {
    std::string result = "";

    if (AstExpr* expr = node->asExpr()) {
        if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {

            bool parenthesis = !b_inside_group;

            auto root = getRootExpr(expr_group->expr);
            bool is_solvable = isSolvable(root);
            if (root->is<AstExprUnary>() || (!is_solvable && (root->is<AstExprFunction>() || root->is<AstExprBinary>())))
                parenthesis = true;

            if (parenthesis)
                result = '(';

            b_inside_group = true;
            if (is_solvable) {
                appendSolve(expr_group, beautify);
            } else {
                result.append(beautify(expr_group->expr));
            };
            b_inside_group = false;

            if (parenthesis)
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
            if (isSolvable(expr_call)) {
                appendSolve(expr_call, beautify);
            } else {
                result = beautify(expr_call->func);
                tuple(expr_call->args, AstExpr);
            };
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
            beautifyFunction(expr_function);
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
                appendSolve(expr_unary, beautify);
            } else {
                result.append(unary_operators[expr_unary->op]);
                result.append(beautify(expr_unary->expr));
            }
        } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
            if (isSolvable(expr_binary)) {
                appendSolve(expr_binary, beautify);
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
        } else if (AstExprTypeAssertion* expr_type_assertion = expr->as<AstExprTypeAssertion>()) {
            result.append(beautify(expr_type_assertion->expr))
                .append("::").append(beautify(expr_type_assertion->annotation));
        } else {
            result.append("--[[ error: unknown expression type ").append(std::to_string(expr->classIndex)).append("! ]]");
        };
    } else if (AstStat* stat = node->asStat()) {
        Injection injection = inject_callback ? inject_callback(stat, b_is_root, inject_callback_data) : INJECTION_NONE;
        bool skip = skip_count == 0 || injection.skip;

        if (skip_count >= 0) skip_count--;

        if (skip || injection.replace) {
            if (b_is_root)
                b_is_root = false;

            if (skip)
                return "";

            return injection.replace.value();
        };

        if (injection.prepend) {
            addIndents;
            result.append(*injection.prepend);
        };

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
            bool dont_append_end = b_dont_append_end;
            b_dont_append_end = false;

            addIndents;
            result.append("if ");
            result.append(beautify(stat_if->condition));
            result.append(" then\n");

            AstStatIf* if_break_simplify = nullptr;
            if (extra1 && stat_if->thenbody->body.size > 1) {
                if (AstStatIf* second_stat_if = stat_if->thenbody->body.data[0]->as<AstStatIf>()) {
                    if (!second_stat_if->elsebody && second_stat_if->thenbody->body.size > 0 && second_stat_if->thenbody->body.data[second_stat_if->thenbody->body.size - 1]->is<AstStatBreak>()) {
                        second_stat_if->thenbody->body.size--; // this is probably a memory violation idrk

                        if_break_simplify = second_stat_if;
                    }
                }
            }

            indent++;
            if (if_break_simplify) {
                addIndents;

                result.append("if ")
                    .append(beautify(if_break_simplify->condition))
                    .append(" then\n");

                indent++;
                b_dont_append_do = true;
                result.append(beautify(if_break_simplify->thenbody));
                indent--;

                addIndents;

                result.append("else");

                indent++;
                b_dont_append_do = true;
                skip_count = 1;
                result.append(beautify(stat_if->thenbody));
                indent--;

                addIndents;

                result.append("end;");
            } else {
                b_dont_append_do = true;
                result.append(beautify(stat_if->thenbody));
            }
            indent--;

            if (stat_if->elsebody) {
                optionalNewline;
                result.append("else");

                bool is_if = stat_if->elsebody->is<AstStatIf>();
                if (is_if) {
                    b_ignore_indent = true;
                    b_dont_append_end = true;
                } else {
                    indent++;
                    result += '\n';
                }

                b_dont_append_do = true;
                result.append(beautify(stat_if->elsebody));
                if (!is_if)
                    indent--;
            }

            if (!dont_append_end) {
                optionalNewline;
                result.append("end;");
            }
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
            bool has_values = stat_local->values.size > 0;
            bool all_values_are_ifelse_exprs = has_values;
            if (has_values)
                for (AstExpr* expr : stat_local->values)
                    if (!expr->is<AstExprIfElse>()) {
                        all_values_are_ifelse_exprs = false;
                        break;
                    };

            if (replace_if_expressions && all_values_are_ifelse_exprs) {
                AstExprIfElse** expr_if_else = reinterpret_cast<AstExprIfElse**>(stat_local->values.data);
                for (int index = 0; index < stat_local->values.size; index++) {
                    replaceIfElse(&result, (*expr_if_else), beautify(stat_local->vars.data[index]), true);

                    if (index == stat_local->values.size - 1)
                        result.erase(result.length() - 1, 1);
                    expr_if_else++;
                };
            } else {
                addIndents;
                result.append("local ");
                astlist(stat_local->vars, AstLocal);
                if (has_values) {
                    result.append(" = ");
                    astlist2(stat_local->values, AstExpr);
                };
                result.append(";");
            };
        } else if (AstStatFor* stat_for = stat->as<AstStatFor>()) {
            DummyForLoopVisitor* visitor = new DummyForLoopVisitor();
            visitor->var = stat_for->var->name.value;

            if (extra1)
                stat_for->visit(visitor);

            if (visitor->success) {
                b_dont_append_do = true;
                stat_for->body->body.size--; // this is probably a memory violation idrk
                result.append(beautify(stat_for->body));
            } else {
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
            };
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
            bool has_values = stat_assign->values.size > 0;
            bool all_values_are_ifelse_exprs = has_values;
            if (has_values)
                for (AstExpr* expr : stat_assign->values)
                    if (!expr->is<AstExprIfElse>()) {
                        all_values_are_ifelse_exprs = false;
                        break;
                    };

            if (replace_if_expressions && all_values_are_ifelse_exprs) {
                AstExprIfElse** expr_if_else = reinterpret_cast<AstExprIfElse**>(stat_assign->values.data);
                for (int index = 0; index < stat_assign->values.size; index++) {
                    replaceIfElse(&result, (*expr_if_else), beautify(stat_assign->vars.data[index]));

                    if (index == stat_assign->values.size - 1)
                        result.erase(result.length() - 1, 1);
                    expr_if_else++;
                };
            } else {
                addIndents;
                astlist(stat_assign->vars, AstExpr);
                if (has_values) {
                    result.append(" = ");
                    astlist2(stat_assign->values, AstExpr);
                };
                result.append(";");
            };
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

            result.append("local function ");
            result.append(beautify(stat_local_function->name));

            beautifyFunction(stat_local_function->func);
        } else {
            result.append("--[[ error: unknown stat type ").append(std::to_string(stat->classIndex)).append("! ]]");
        };

        if (injection.append) {
            result.append(*injection.append);
        };
    } else if (AstType* type = node->asType()) {
        if (AstTypeReference* type_reference = type->as<AstTypeReference>()) {
            result.append(type_reference->name.value);
            if (type_reference->hasParameterList) {
                result += '<';
                for (AstTypeOrPack type_or_pack : type_reference->parameters) {
                    result.append(beautify(type_or_pack.typePack == nullptr ? type_or_pack.type->asType() : type_or_pack.typePack->asType()));
                    result.append(", ");
                }
                result.erase(result.size() - 2, 2);
                result += '>';
            }
        } else {
            result.append("--[[ error: unknown type type ").append(std::to_string(type->classIndex)).append("! ]]");
        }
    }

    return result;
};

void replaceIfElse(std::string* out, AstExprIfElse* expr, std::string var, bool use_local) {
    std::string result = *out;
    addIndents;
    if (use_local) {
        result.append("local ");
        result.append(var);
        result.append(";\n");
        addIndents;
    };

    result.append("if ");
    result.append(beautify(expr->condition));
    result.append(" then\n");

    indent++;
    if (getRootExpr(expr->trueExpr)->is<AstExprIfElse>())
        replaceIfElse(&result, getRootExpr(expr->trueExpr)->as<AstExprIfElse>(), var);
    else {
        addIndents;
        result.append(var);
        result.append(" = ");
        result.append(beautify(expr->trueExpr));
        result.append(";\n");
    };
    indent--;

    addIndents;
    result.append("else\n");
    indent++;
    if (getRootExpr(expr->falseExpr)->is<AstExprIfElse>())
        replaceIfElse(&result, getRootExpr(expr->falseExpr)->as<AstExprIfElse>(), var);
    else {
        addIndents;
        result.append(var);
        result.append(" = ");
        result.append(beautify(expr->falseExpr));
        result.append(";\n");
    };
    indent--;

    addIndents;
    result.append("end;\n");

    out->replace(0, out->size(), result.c_str());
};


std::string beautifyRoot(AstStatBlock* root, bool nosolve_in, bool replace_if_expressions_in, bool extra1_in) {
    replace_if_expressions = replace_if_expressions_in;
    extra1 = extra1_in;
    setNoSolve(nosolve_in);
    return beautify(root);
};
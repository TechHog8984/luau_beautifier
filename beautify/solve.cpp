#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include "solve.hpp"

#include "Luau/Ast.h"
#include "Luau/Lexer.h"

using namespace Luau;

Allocator* allocator = nullptr;

void setAllocator(Luau::Allocator* allocator_in) {
    allocator = allocator_in;
}

bool nosolve;
bool s_ignore_types;

AstExpr* getRootExpr(AstExpr* expr) {
    if (s_ignore_types) {
        while (true) {
            if (auto expr_group = expr->as<AstExprGroup>()) {
                expr = expr_group->expr;
                continue;
            } else if (auto expr_type_assertion = expr->as<AstExprTypeAssertion>()) {
                expr = expr_type_assertion->expr;
                continue;
            }
            break;
        }
    } else {
        AstExprGroup* expr_group;
        while ((expr_group = expr->as<AstExprGroup>()))
            expr = expr_group->expr;
    }

    return expr;
};

// NOTE: if we add function to this list, beautify on AstExprGroup will need to be adjusted
enum SolveResultType {
    None,
    Bool,
    Number,
    String,
    Unknown
};

bool isConstant(AstExpr* expr);
bool isConstantNumber(AstExpr* expr);
bool isConstantString(AstExpr* expr);
bool isConstantTable(AstExpr* expr);

bool isBinaryMath(AstExprBinary* expr_binary) {
    switch (expr_binary->op) {
        case AstExprBinary::Op::Add:
        case AstExprBinary::Op::Sub:
        case AstExprBinary::Op::Mul:
        case AstExprBinary::Op::Div:
        case AstExprBinary::Op::FloorDiv:
        case AstExprBinary::Op::Mod:
        case AstExprBinary::Op::Pow:
            return true;

        default:
            return false;
    }
};

double solveBinary(AstExprBinary::Op op, double left, double right) {
    switch (op) {
        case AstExprBinary::Op::Add:
            return left + right;
            break;
        case AstExprBinary::Op::Sub:
            return left - right;
            break;
        case AstExprBinary::Op::Mul:
            return left * right;
            break;
        case AstExprBinary::Op::Div:
            return left / right;
            break;
        case AstExprBinary::Op::FloorDiv:
            // TODO: double check floordiv implementation
            return floor(left / right);
            break;
        case AstExprBinary::Op::Mod:
            return (int) left % (int) right;
            break;
        case AstExprBinary::Op::Pow:
            return pow(left, right);
            break;

        default:
            fprintf(stderr, "solveBinary expected a math operator\n");
            exit(1);
    };
};

std::optional<std::vector<AstExpr*>> getConstantList(std::vector<AstExpr*> list) {
    size_t list_size = list.size();
    std::vector<AstExpr*> const_list;

    for (size_t i = 0; i < list_size; i++) {
        AstExpr* value = list.at(i);

        if (isConstant(value))
            const_list.push_back(value);
        else if (auto value_call = value->as<AstExprCall>()) {
            if (auto func = getRootExpr(value_call->func)->as<AstExprFunction>()) {
                auto body = func->body->body;
                // we need to ensure that there is only one return
                // this could easily be improved using a visitor that looks for return stats
                if (body.size != 1)
                    return std::nullopt;

                auto return_stat = body.data[0]->as<AstStatReturn>();
                if (!return_stat)
                    return std::nullopt;

                std::vector<AstExpr*> ret_list;

                bool vararg = func->args.size == 0 && func->vararg;
                auto return_list = return_stat->list;
                auto return_count = return_list.size;
                if (return_count == 0) {
                    if (allocator) {
                        const_list.push_back(allocator->alloc<AstExprConstantNil>(Location(Position(0, 0), 0)));
                        continue;
                    }
                    else
                        return std::nullopt;
                }

                if (i + 1 == list_size) {
                    for (unsigned index = 0; index < return_count; index++)
                        ret_list.push_back(return_list.data[index]);

                    if (auto last = getRootExpr(ret_list.back())->as<AstExprVarargs>()) {
                        ret_list.pop_back();
                        for (auto arg : value_call->args)
                            ret_list.push_back(arg);
                    }

                    auto ret_const_list = getConstantList(ret_list);
                    if (ret_const_list.has_value()) {
                        const_list.insert(std::end(const_list), std::begin(ret_const_list.value()), std::end(ret_const_list.value()));
                    } else
                        return std::nullopt;
                } else
                    const_list.push_back(return_list.data[0]);
            }
        } else
            return std::nullopt;
    }

    return const_list;
}
std::optional<size_t> getTableSize(AstExprTable* table) {
    std::vector<AstExpr*> list;
    auto items = table->items;

    if (items.size == 0)
        return 0;

    for (unsigned index = 0; index < items.size; index++) {
        auto item = items.data[index];
        if (item.kind != AstExprTable::Item::List)
            return std::nullopt;

        list.push_back(item.value);
    }

    auto const_list = getConstantList(list);
    if (!const_list.has_value())
        return std::nullopt;

    /*
    ** Try to find a boundary in table `t'. A `boundary' is an integer index
    ** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
    */

    int j = const_list->size();

    if (j > 0) {
        auto expr = getRootExpr(const_list->at(j - 1));
        if (!isSolvable(expr))
            return std::nullopt;

        auto solved_result = solve(expr);
        if (solved_result.type == Solved::Expression && getRootExpr(solved_result.expression_result)->is<AstExprConstantNil>()) {
            #define solveAndCheckNil(oldexpr) expr = getRootExpr(oldexpr); \
                if (!isSolvable(expr)) \
                    return std::nullopt; \
                solved_result = solve(expr); \
                bool is_nil = solved_result.type == Solved::Expression && getRootExpr(solved_result.expression_result)->is<AstExprConstantNil>();

            AstExpr** base = const_list->data();
            int rest = j;
            while (int half = rest >> 1) {
                solveAndCheckNil(base[half])
                base = is_nil ? base : base + half;
                rest -= half;
            }

            solveAndCheckNil(*base);

            int boundary = !is_nil + int(base - const_list->data());
            return boundary;
        }
    }

    return j;
}

typedef struct {
    AstExprBinary::Op op;
    size_t length;
    double number;
} InlineNumberThroughStringLenFunctionResult;

std::optional<InlineNumberThroughStringLenFunctionResult> testInlineNumberThroughStringLenFunction(AstExpr* expr) {
    auto expr_call = getRootExpr(expr)->as<AstExprCall>();
    if (!expr_call || expr_call->args.size != 1)
        return std::nullopt;

    auto expr_function = getRootExpr(expr_call->func)->as<AstExprFunction>();
    if (!expr_function || expr_function->body->body.size != 1
        || expr_function->args.size != 1)
        return std::nullopt;

    auto stat_return = expr_function->body->body.data[0]->as<AstStatReturn>();
    if (!stat_return || stat_return->list.size != 1)
        return std::nullopt;

    auto expr_binary = getRootExpr(stat_return->list.data[0])->as<AstExprBinary>();
    if (!expr_binary || !isSolvable(expr_binary->right)
        || !isBinaryMath(expr_binary))
        return std::nullopt;

    Solved binary_right = solve(expr_binary->right);
    if (binary_right.type != Solved::Number)
        return std::nullopt;

    auto left = getRootExpr(expr_binary->left)->as<AstExprUnary>();
    if (left->op != AstExprUnary::Len)
        return std::nullopt;

    auto unary_expr = getRootExpr(left->expr)->as<AstExprLocal>();
    if (!unary_expr)
        return std::nullopt;

    auto arg_passed = getRootExpr(expr_call->args.data[0])->as<AstExprConstantString>();
    if (!arg_passed)
        return std::nullopt;

    auto arg_received = expr_function->args.data[0];
    if (strcmp(arg_received->name.value, unary_expr->local->name.value) == 0) {
        InlineNumberThroughStringLenFunctionResult result {
            .op = expr_binary->op,
            .length = strlen(arg_passed->value.data),
            .number = binary_right.number_result
        };
        return result;
    }

    return std::nullopt;
}

std::optional<AstExpr*> testSimpleFunctionCall(AstExpr* expr) {
    auto expr_call = getRootExpr(expr)->as<AstExprCall>();
    if (!expr_call)
        return std::nullopt;

    auto function = getRootExpr(expr_call->func)->as<AstExprFunction>();
    if (!function)
        return std::nullopt;

    auto body = function->body->body;
    if (body.size != 1)
        return std::nullopt;

    auto stat_return = body.data[0]->as<AstStatReturn>();
    if (!stat_return)
        return std::nullopt;

    auto list = stat_return->list;
    // TODO: change the return type to a vector of expressions, and we can remove this check and loop through each
    if (list.size != 1) {
        if (list.size == 0 && allocator)
            return allocator->alloc<AstExprConstantNil>(Location(Position(0, 0), 0));
        return std::nullopt;
    }

    // auto value = list.data[0];

    // if (isConstant(value))
    //     return value;

    // return std::nullopt;

    return list.data[0];
}

SolveResultType getSolveResultType(AstExpr* expr) {
    SolveResultType result = None;
    if (nosolve)
        return result;

    if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
        switch (expr_unary->op) {
            case AstExprUnary::Op::Not:
                if (isConstant(expr_unary->expr))
                    result = Bool;
                break;
            case AstExprUnary::Op::Minus:
                if (isConstantNumber(expr_unary->expr))
                    result = Number;
                break;
            case AstExprUnary::Op::Len:
                if (isConstantString(expr_unary->expr)) {
                    result = Number;
                } else if (isConstantTable(expr_unary->expr)) {
                    auto table = getRootExpr(expr_unary->expr)->as<AstExprTable>();
                    std::optional<size_t> size = getTableSize(table);
                    if (size.has_value())
                        result = Number;
                }
                break;
            default:
                break;
        };
    } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
        // TODO: number concat
        if (expr_binary->op != AstExprBinary::Op::Concat && isConstantNumber(expr_binary->left) && isConstantNumber(expr_binary->right))
            result = Number;
        else if (isConstantString(expr_binary->left) && isConstantString(expr_binary->right)) {
            switch (expr_binary->op) {
                case AstExprBinary::Op::CompareNe:
                case AstExprBinary::Op::CompareEq:
                case AstExprBinary::Op::CompareLt:
                case AstExprBinary::Op::CompareLe:
                case AstExprBinary::Op::CompareGt:
                case AstExprBinary::Op::CompareGe:
                    result = Bool;
                    break;

                case AstExprBinary::Op::And:
                case AstExprBinary::Op::Or:
                    result = String;
                    break;

                default:
                    break;
            };
        };
    } else if (getRootExpr(expr)->is<AstExprConstantNumber>())
        result = Number;
    else if (getRootExpr(expr)->is<AstExprConstantString>())
        result = String;
    else if (getRootExpr(expr)->is<AstExprConstantNil>() || getRootExpr(expr)->is<AstExprConstantBool>())
        result = Unknown;
    // (function(A) return (#A - 9) end)("some string")
    else if (testInlineNumberThroughStringLenFunction(expr))
        result = Number;
    else if (testSimpleFunctionCall(expr))
        result = Unknown;

    return result;
};

bool isConstant(AstExpr* expr) {
    if (isSolvable(expr))
        return true;

    expr = getRootExpr(expr);

    return expr->is<AstExprConstantNil>() || expr->is<AstExprConstantBool>() || expr->is<AstExprConstantNumber>() || expr->is<AstExprConstantString>();
};
bool isConstantNumber(AstExpr* expr) {
    expr = getRootExpr(expr);

    return expr->is<AstExprConstantNumber>() || getSolveResultType(expr) == Number;
};
bool isConstantString(AstExpr* expr) {
    expr = getRootExpr(expr);

    return expr->is<AstExprConstantString>() || getSolveResultType(expr) == String;
};
bool isConstantTable(AstExpr* expr) {
    expr = getRootExpr(expr);

    return expr->is<AstExprTable>();
};

bool isSolvable(AstExpr* expr) {
    return getSolveResultType(expr) != None;
};

bool isFalsey(AstExpr* expr) {
    if (AstExprConstantBool* expr_bool = expr->as<AstExprConstantBool>())
        return expr_bool->value == false;

    return expr->is<AstExprConstantNil>();
};

Solved solve(AstExpr* expr) {
    expr = getRootExpr(expr);
    assert(isSolvable(expr));

    Solved result = {};

    if (AstExprConstantNumber* expr_number = expr->as<AstExprConstantNumber>()) {
        result.type = Solved::Type::Number;
        result.number_result = expr_number->value;
    } else if (AstExprConstantString* expr_string = expr->as<AstExprConstantString>()) {
        result.type = Solved::Type::Expression;
        result.expression_result = expr_string;
    } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
        switch (expr_unary->op) {
            case AstExprUnary::Op::Not:
                if (isConstant(expr_unary->expr)) {
                    result.type = Solved::Type::Bool;
                    result.bool_result = isFalsey(getRootExpr(expr_unary->expr));
                };
                break;
            case AstExprUnary::Op::Minus:
                if (isConstantNumber(expr_unary->expr)) {
                    result.type = Solved::Type::Number;
                    result.number_result = -solve(expr_unary->expr).number_result;
                };
                break;
            case AstExprUnary::Op::Len:
                if (isConstantString(expr_unary->expr)) {
                    result.type = Solved::Type::Number;
                    result.number_result = solve(expr_unary->expr).expression_result->as<AstExprConstantString>()->value.size;
                } else if (isConstantTable(expr_unary->expr)) {
                    result.type = Solved::Type::Number;

                    auto table = getRootExpr(expr_unary->expr)->as<AstExprTable>();
                    std::optional<size_t> size = getTableSize(table);
                    assert(size.has_value());

                    result.number_result = size.value();
                };

                break;

            default:
                break;
        };
    } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
        if (isConstantNumber(expr_binary->left) && isConstantNumber(expr_binary->right)) {
            Solved left = solve(expr_binary->left);
            Solved right = solve(expr_binary->right);

            switch (expr_binary->op) {
                case AstExprBinary::Op::Add:
                    result.type = Solved::Type::Number;
                    result.number_result = left.number_result + right.number_result;
                    break;
                case AstExprBinary::Op::Sub:
                    result.type = Solved::Type::Number;
                    result.number_result = left.number_result - right.number_result;
                    break;
                case AstExprBinary::Op::Mul:
                    result.type = Solved::Type::Number;
                    result.number_result = left.number_result * right.number_result;
                    break;
                case AstExprBinary::Op::Div:
                    result.type = Solved::Type::Number;
                    result.number_result = left.number_result / right.number_result;
                    break;
                case AstExprBinary::Op::FloorDiv:
                    result.type = Solved::Type::Number;
                    // TODO: double check floordiv implementation
                    result.number_result = floor(left.number_result / right.number_result);
                    break;
                case AstExprBinary::Op::Mod:
                    result.type = Solved::Type::Number;
                    result.number_result = (int) left.number_result % (int) right.number_result;
                    break;
                case AstExprBinary::Op::Pow:
                    result.type = Solved::Type::Number;
                    result.number_result = pow(left.number_result, right.number_result);
                    break;
                case AstExprBinary::Op::Concat:
                    // TODO: number concat
                    break;
                case AstExprBinary::Op::CompareNe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result != right.number_result;
                    break;
                case AstExprBinary::Op::CompareEq:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result == right.number_result;
                    break;
                case AstExprBinary::Op::CompareLt:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result < right.number_result;
                    break;
                case AstExprBinary::Op::CompareLe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result <= right.number_result;
                    break;
                case AstExprBinary::Op::CompareGt:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result > right.number_result;
                    break;
                case AstExprBinary::Op::CompareGe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = left.number_result >= right.number_result;
                    break;
                case AstExprBinary::Op::And:
                    result.type = Solved::Type::Number;
                    result.number_result = right.number_result;
                    break;
                case AstExprBinary::Op::Or:
                    result.type = Solved::Type::Number;
                    result.number_result = left.number_result;
                    break;

                default:
                    break;
            };
        } else if (isConstantString(expr_binary->left) && isConstantString(expr_binary->right)) {
            char* left = getRootExpr(expr_binary->left)->as<AstExprConstantString>()->value.data;
            char* right = getRootExpr(expr_binary->right)->as<AstExprConstantString>()->value.data;

            int res = strcmp(left, right);

            switch (expr_binary->op) {
                case AstExprBinary::Op::CompareNe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res != 0;
                    break;
                case AstExprBinary::Op::CompareEq:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res == 0;
                    break;
                case AstExprBinary::Op::CompareLt:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res < 0;
                    break;
                case AstExprBinary::Op::CompareLe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res <= 0;
                    break;
                case AstExprBinary::Op::CompareGt:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res > 0;
                    break;
                case AstExprBinary::Op::CompareGe:
                    result.type = Solved::Type::Bool;
                    result.bool_result = res >= 0;
                    break;

                case AstExprBinary::Op::And:
                    result.type = Solved::Type::Expression;
                    result.expression_result = getRootExpr(expr_binary->right);
                    break;
                case AstExprBinary::Op::Or:
                    result.type = Solved::Type::Expression;
                    result.expression_result = getRootExpr(expr_binary->left);
                    break;

                default:
                    break;
            };
        };
    } else if (AstExprConstantNil* expr_nil = expr->as<AstExprConstantNil>()) {
        result.type = Solved::Type::Expression;
        result.expression_result = expr_nil;
    } else if (AstExprConstantBool* expr_bool = expr->as<AstExprConstantBool>()) {
        result.type = Solved::Type::Expression;
        result.expression_result = expr_bool;
    } else if (auto constant_wrap = testInlineNumberThroughStringLenFunction(expr)) {
        result.type = Solved::Type::Number;
        result.number_result = solveBinary(constant_wrap->op, constant_wrap->length, constant_wrap->number);
    } else if (auto simple = testSimpleFunctionCall(expr)) {
        result.type = Solved::Expression;
        result.expression_result = simple.value();
    }

    return result;
};

void setupSolve(bool nosolve_in, bool ignore_types_in) {
    nosolve = nosolve_in;
    s_ignore_types = ignore_types_in;
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

    if (result == "inf")
        result = "math.huge";
    else if (result == "nan" or result == "-nan")
        result = "(0/0)";
    else {
        while (result.length() > 2 && result[result.length() - 1] == '0')
            result.erase(result.length() - 1, 1);

        if (result[result.length() - 1] == '.')
            result.erase(result.length() - 1, 1);
    };

    return result;
};
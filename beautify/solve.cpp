#include <cassert>
#include <cmath>
#include <cstring>
#include "solve.hpp"

#include "Luau/Ast.h"

using namespace Luau;

bool nosolve;

AstExpr* getRootExpr(AstExpr* expr) {
    AstExprGroup* expr_group;
    while ((expr_group = expr->as<AstExprGroup>()))
        expr = expr_group->expr;

    return expr;
};

enum SolveResultType {
    None,
    Bool,
    Number,
    String,
};

bool isConstant(AstExpr* expr);
bool isConstantNumber(AstExpr* expr);
bool isConstantString(AstExpr* expr);
bool isConstantTable(AstExpr* expr);

SolveResultType getSolveResultType(AstExpr* expr) {
    SolveResultType result = None;
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
                if (isConstantString(expr_unary->expr) || isConstantTable(expr_unary->expr))
                    result = Number;
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
                    result.number_result = getRootExpr(expr_unary->expr)->as<AstExprTable>()->items.size;
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
    };

    return result;
};

void setNoSolve(bool nosolve_in) {
    nosolve = nosolve_in;
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
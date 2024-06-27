#include <cmath>
#include "solve.hpp"

#include "Luau/Ast.h"

using namespace Luau;

bool nosolve;

bool isSolvable(AstExpr* expr) {
    if (nosolve)
        return false;

    if (expr->is<AstExprConstantNumber>()) {
        return true;
    } else if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {
        return isSolvable(expr_group->expr);
    } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
        return isSolvable(expr_unary->expr);
    } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
        bool valid_operation;
        switch (expr_binary->op) {
            case AstExprBinary::Op::Add:
            case AstExprBinary::Op::Sub:
            case AstExprBinary::Op::Mul:
            case AstExprBinary::Op::Div:
            case AstExprBinary::Op::FloorDiv:
            case AstExprBinary::Op::Mod:
            case AstExprBinary::Op::Pow:
            case AstExprBinary::Op::CompareNe:
            case AstExprBinary::Op::CompareEq:
            case AstExprBinary::Op::CompareLt:
            case AstExprBinary::Op::CompareLe:
            case AstExprBinary::Op::CompareGt:
            case AstExprBinary::Op::CompareGe:
                valid_operation = true;
                break;

            default:
                valid_operation = false;
        };

        return valid_operation && isSolvable(expr_binary->left) && isSolvable(expr_binary->right);
    };

    return false;
};

Solved solve(AstExpr* expr) {
    Solved result { .type = Solved::Type::Math, .math_result = 0 };

    if (!isSolvable(expr))
        return result;

    if (AstExprConstantNumber* expr_number = expr->as<AstExprConstantNumber>()) {
        result.math_result = expr_number->value;
    } else if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {
        result = solve(expr_group->expr);
    } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
        result = solve(expr_unary->expr);

        switch (expr_unary->op) {
            case AstExprUnary::Op::Minus:
                result.math_result = -result.math_result;
                break;
            default:
                break;
        };
    } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
        Solved left = solve(expr_binary->left);
        Solved right = solve(expr_binary->right);

        switch (expr_binary->op) {
            case AstExprBinary::Op::Add:
                result.math_result = left.math_result + right.math_result;
                break;
            case AstExprBinary::Op::Sub:
                result.math_result = left.math_result - right.math_result;
                break;
            case AstExprBinary::Op::Mul:
                result.math_result = left.math_result * right.math_result;
                break;
            case AstExprBinary::Op::Div:
                result.math_result = left.math_result / right.math_result;
                break;
            case AstExprBinary::Op::FloorDiv:
                // FIXME: this is wrong
                result.math_result = floor(left.math_result / right.math_result);
                break;
            case AstExprBinary::Op::Mod:
                result.math_result = (int) left.math_result % (int) right.math_result;
                break;
            case AstExprBinary::Op::Pow:
                result.math_result = pow(left.math_result, right.math_result);
                break;
            case AstExprBinary::Op::CompareNe:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result != right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result != right.comparison_result;
                        break;
                };
                break;
            case AstExprBinary::Op::CompareEq:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result == right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result == right.comparison_result;
                        break;
                };
                break;
            case AstExprBinary::Op::CompareLt:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result < right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result < right.comparison_result;
                        break;
                };
                break;
            case AstExprBinary::Op::CompareLe:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result <= right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result <= right.comparison_result;
                        break;
                };
                break;
            case AstExprBinary::Op::CompareGt:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result > right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result > right.comparison_result;
                        break;
                };
                break;
            case AstExprBinary::Op::CompareGe:
                result.type = Solved::Type::Comparison;
                switch (left.type) {
                    case Solved::Type::Math:
                        result.comparison_result = left.math_result >= right.math_result;
                        break;
                    case Solved::Type::Comparison:
                        result.comparison_result = left.comparison_result >= right.comparison_result;
                        break;
                };
                break;

            default:
                break;
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
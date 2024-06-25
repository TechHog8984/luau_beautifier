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
        return isSolvable(expr_binary->left) && isSolvable(expr_binary->right);
    };

    return false;
};

double solve(AstExpr* expr) {
    if (!isSolvable(expr))
        return 0;

    double value;
    if (AstExprConstantNumber* expr_number = expr->as<AstExprConstantNumber>()) {
        value = expr_number->value;
    } else if (AstExprGroup* expr_group = expr->as<AstExprGroup>()) {
        value = solve(expr_group->expr);
    } else if (AstExprUnary* expr_unary = expr->as<AstExprUnary>()) {
        value = solve(expr_unary->expr);

        switch (expr_unary->op) {
            case AstExprUnary::Op::Minus:
                value = -value;
                break;
            default:
                break;
        };
    } else if (AstExprBinary* expr_binary = expr->as<AstExprBinary>()) {
        double left = solve(expr_binary->left);
        double right = solve(expr_binary->right);

        switch (expr_binary->op) {
            case AstExprBinary::Op::Add:
                value = left + right;
                break;
            case AstExprBinary::Op::Sub:
                value = left - right;
                break;
            case AstExprBinary::Op::Mul:
                value = left * right;
                break;
            case AstExprBinary::Op::Div:
                value = left / right;
                break;
            case AstExprBinary::Op::FloorDiv:
                // FIXME: this is wrong
                value = floor(left / right);
                break;
            case AstExprBinary::Op::Mod:
                value = (int) left % (int) right;
                break;
            case AstExprBinary::Op::Pow:
                value = pow(left, right);
                break;
            default:
                break;
        };
    };

    return value;
};

void setNoSolve(bool nosolve_in) {
    nosolve = nosolve_in;
};
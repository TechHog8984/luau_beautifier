#pragma once

#include "Luau/Ast.h"

using namespace Luau;

struct Solved {
    enum Type {
        Math,
        Comparison
    } type;
    double math_result;
    bool comparison_result;
};

#define appendSolve(expr) \
Solved solved = solve(expr); \
switch (solved.type) { \
    case Solved::Type::Math: \
        result.append(convertNumber(solved.math_result)); \
        break; \
    case Solved::Type::Comparison: \
        result.append(solved.comparison_result ? "true" : "false"); \
        break; \
}

bool isSolvable(AstExpr* expr);
Solved solve(AstExpr* expr);
void setNoSolve(bool nosolve);

std::string convertNumber(double value);
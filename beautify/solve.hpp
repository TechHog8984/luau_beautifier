#pragma once

#include "Luau/Ast.h"
#include "Luau/Lexer.h"

using namespace Luau;

struct Solved {
    enum Type {
        Number,
        Bool,
        Expression
    } type;
    double number_result;
    bool bool_result;
    AstExpr* expression_result;
};

AstExpr* getRootExpr(AstExpr* expr);

#define appendSolve(expr, format) \
Solved solved = solve(expr); \
switch (solved.type) { \
    case Solved::Type::Number: \
        result.append(convertNumber(solved.number_result)); \
        break; \
    case Solved::Type::Bool: \
        result.append(solved.bool_result ? "true" : "false"); \
        break; \
    case Solved::Type::Expression: \
        result.append(format(solved.expression_result)); \
        break; \
}

bool isSolvable(AstExpr* expr);
Solved solve(AstExpr* expr);
void setupSolve(bool nosolve, bool ignore_types);

void setAllocator(Luau::Allocator* allocator);

std::string convertNumber(double value);
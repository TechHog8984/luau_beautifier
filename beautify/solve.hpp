#pragma once

#include "Luau/Ast.h"

using namespace Luau;

bool isSolvable(AstExpr* expr);
double solve(AstExpr* expr);
void setNoSolve(bool nosolve);
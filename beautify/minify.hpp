#pragma once

#include <string>

#include "Luau/Ast.h"

std::string minifyRoot(Luau::AstStatBlock* root, bool nosolve, bool ignore_types);
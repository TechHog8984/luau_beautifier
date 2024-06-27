#pragma once

#include <string>

#include "Luau/Ast.h"

inline const char* unary_operators[3] = {"not ", "-", "#"};
inline const char* binary_operators[16] = {"+", "-", "*", "/", "//", "%", "^", 
    "..", "~=", "==", "<", "<=", ">", ">=", "and", "or"};

#define listPre(array) \
int list_index = 0; \
size_t list_size = array.size

#define listBody(array, type) \
for (type* obj : array) { \
    list_index++; \
    result.append(convert(obj)); \
    if (list_index < list_size) { \
        result.append(", "); \
    }; \
}

#define astlist(array, type) \
listPre(array); \
listBody(array, type)

#define astlist2(array, type) \
list_index = 0; \
list_size = array.size; \
listBody(array, type)


#define tuple(array, type) \
result.append("("); \
astlist(array, type); \
result.append(")")


std::string convertNumber(double value);
std::string fixString(Luau::AstArray<char> value);
std::string beautifyRoot(Luau::AstStatBlock* root, bool nosolve);
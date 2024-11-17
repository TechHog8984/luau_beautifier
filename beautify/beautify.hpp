#pragma once

#include <optional>
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

std::string getIndents(int offset = 0);

struct Injection {
    std::optional<std::string> replace; // replace
    std::optional<std::string> prepend; // before
    std::optional<std::string> append; // after
    bool skip;
};

typedef Injection InjectCallback(Luau::AstStat* stat, bool is_root, void* data);
void setupInjectCallback(InjectCallback, void* data = nullptr);
void dontAppendDo();

std::string fixString(Luau::AstArray<char> value);
std::string beautifyRoot(Luau::AstStatBlock* root, bool nosolve, bool replace_if_expressions, bool extra1);
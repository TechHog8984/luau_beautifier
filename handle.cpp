#include "handle.hpp"

#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"
#include "Luau/ToString.h"

#include "beautify.hpp"
#include "minify.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/bind.h>
#endif

// left here for demonstration purposes
// struct Data {
//     int a = 100;
// };
// InjectCallback comment_callback;
// Injection comment_callback(Luau::AstStat* stat, bool is_root, void* d) {
//     Data* data = (Data*) d;
//     if (stat->is<Luau::AstStatExpr>())
//         return { .skip = true };
//     else if (!is_root && stat->is<Luau::AstStatBlock>()) {
//         std::string append = getIndents();
//         append.append("-- ^ this is a block\n");
//         return {
//             .prepend = std::string("-- this is a block, data is ")
//                 .append(std::to_string(data->a)) += '\n',
//             .append = append
//         };
//     };

//     return {};
// };


std::string handleSource(std::string source, bool minify, bool nosolve, bool replace_if_expressions, bool extra1) {
    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = true;

    Luau::ParseResult parse_result = Luau::Parser::parse(source.data(), source.size(), names, allocator, options);

    if (parse_result.errors.size() > 0) {
        fprintf(stderr, "Parse errors were encountered\n");
        for (const Luau::ParseError& error : parse_result.errors) {
            fprintf(stderr, "   %s - %s\n", Luau::toString(error.getLocation()).c_str(), error.getMessage().c_str());
        };
        fprintf(stderr, "\n");

        exit(1);
    };

    Luau::AstStatBlock* root = parse_result.root;

    // left here for demonstration purposes
    // Data d;
    // d.a += 10;
    // setupInjectCallback(comment_callback, &d);

    if (minify)
        return minifyRoot(root, nosolve);
    else {
        std::string preface;

        for (Luau::HotComment hot_comment : parse_result.hotcomments) {
            preface.append("--!")
                .append(hot_comment.content);
            preface += '\n';
        }

        return preface.append(beautifyRoot(root, nosolve, replace_if_expressions, extra1));
    };
};

#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("handleSource", &handleSource);
}
#endif
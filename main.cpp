#include <cstdio>
#include <optional>

#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"
#include "Luau/ToString.h"
#include "FileUtils.h"

#include "beautify.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s filepath\n", argv[0]);
        return 1;
    };

    std::optional<std::string> source = readFile(argv[1]);

    if (!source) {
        fprintf(stderr, "failed to read file %s\n", argv[1]);
        return 1;
    };

    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = true;

    Luau::ParseResult parse_result = Luau::Parser::parse(source->data(), source->size(), names, allocator, options);

    if (parse_result.errors.size() > 0) {
        fprintf(stderr, "Parse errors were encountered\n");
        for (const Luau::ParseError& error : parse_result.errors) {
            fprintf(stderr, "   %s - %s\n", Luau::toString(error.getLocation()).c_str(), error.getMessage().c_str());
        };
        fprintf(stderr, "\n");

        return 1;
    };

    Luau::AstStatBlock* root = parse_result.root;

    printf("%s", beautify(root).c_str());

    return 0;
}
#include <cstdio>
#include <cstring>
#include <optional>

#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"
#include "Luau/ToString.h"
#include "FileUtils.h"

#include "beautify.hpp"
#include "minify.hpp"

int displayHelp(char* path) {
    printf("Usage: %s [options] [file]\n\n", path);

    printf("options:\n");
    printf("  --minify: switches output mode from beautify to minify\n");
    printf("  --nosolve: doesn't solve simple expressions\n");

    return 0;
};

int parseArgs(int* argc, char** argv, char** filepath, bool* minify, bool* nosolve) {
    bool found_file = false;

    for (int i = 1; i < *argc; i++) {
        if (strncmp("--", argv[i], 2) == 0) {
            argv[i] += 2;
            if (strcmp(argv[i], "minify") == 0)
                *minify = true;
            else if (strcmp(argv[i], "nosolve") == 0)
                *nosolve = true;
            else {
                fprintf(stderr, "Error: unrecognized option '%s'\n\n", (char*) argv[i] - 2);
                return 1;
            };
        } else {
            if (found_file) {
                fprintf(stderr, "Error: multiple files are not supported\n\n");
                return 1;
            };

            found_file = true;
            *filepath = argv[i];
        };
    };

    return 0;
};

int main(int argc, char** argv) {
    if (argc == 0) // what?
        return displayHelp((char*) "luau-beautifier");

    if (argc == 1)
        return displayHelp(argv[0]);

    char* filepath;

    bool minify;
    bool nosolve;

    if (parseArgs(&argc, argv, &filepath, &minify, &nosolve)) {
        return displayHelp(argv[0]);
    };

    std::optional<std::string> source = readFile(filepath);

    if (!source) {
        fprintf(stderr, "failed to read file %s\n", filepath);
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

    if (minify)
        printf("%s", minifyRoot(root, nosolve).c_str());
    else
        printf("%s", beautifyRoot(root, nosolve).c_str());

    return 0;
}
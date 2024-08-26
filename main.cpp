#include <cstdio>
#include <cstring>
#include <optional>

#include "FileUtils.h"

#include "handle.hpp"

int displayHelp(char* path) {
    printf("Usage: %s [options] [file]\n\n", path);

    printf("options:\n");
    printf("  --minify: switches output mode from beautify to minify\n");
    printf("  --nosolve: doesn't solve simple expressions\n");
    printf("  --replaceifelseexpr: tries to replace if else expressions with statements\n");
    printf("  --extra1: tries to replace certain statements / expression using potentially dangerous methods\n");

    return 0;
};

int parseArgs(int* argc, char** argv, char** filepath, bool* minify, bool* nosolve, bool* replace_if_expressions, bool* extra1) {
    bool found_file = false;

    for (int i = 1; i < *argc; i++) {
        if (strncmp("--", argv[i], 2) == 0) {
            argv[i] += 2;
            if (strcmp(argv[i], "minify") == 0)
                *minify = true;
            else if (strcmp(argv[i], "nosolve") == 0)
                *nosolve = true;
            else if (strcmp(argv[i], "replaceifelseexpr") == 0)
                *replace_if_expressions = true;
            else if (strcmp(argv[i], "extra1") == 0)
                *extra1 = true;
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
    bool replace_if_expressions;
    bool extra1;

    if (parseArgs(&argc, argv, &filepath, &minify, &nosolve, &replace_if_expressions, &extra1)) {
        return displayHelp(argv[0]);
    };

    std::optional<std::string> source = readFile(filepath);

    if (!source) {
        fprintf(stderr, "failed to read file %s\n", filepath);
        return 1;
    };

    printf("%s", handleSource(source.value(), minify, nosolve, replace_if_expressions, extra1).c_str());

    return 0;
}
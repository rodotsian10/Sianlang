/* Keep the original one-command build: gcc main.c -o Sianlang.exe */
#include "src/common.h"
#include "src/lexer.h"
#include "src/parser.h"
#include "src/runtime.h"

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts("SianLang 0.2.0");
        return 0;
    }
    if (argc != 2) {
        fputs("Usage: Sianlang.exe file.sian\n", stderr);
        return 1;
    }
    source_name = argv[1];
    size_t length = strlen(source_name);
    if (length < 5 || strcmp(source_name + length - 5, ".sian") != 0) {
        error_at((Location){0, 0}, "file must end with .sian");
        return 1;
    }
#ifdef _WIN32
    UINT old_input = GetConsoleCP(), old_output = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    wchar_t *path = windows_source_path();
    char *utf8_path = NULL;
    FILE *file = NULL;
    if (path) {
        int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
        if (size > 0) {
            utf8_path = resize(NULL, (size_t)size);
            WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path, size, NULL, NULL);
            source_name = utf8_path;
        }
        file = _wfopen(path, L"rb");
        free(path);
    }
#else
    FILE *file = fopen(source_name, "rb");
#endif
    if (!file) error_at((Location){0, 0}, "cannot open file");
    char *source_text = NULL;
    TokenList tokens = {0};
    Arena arena = {0};
    if (file) {
        source_text = read_source(file);
        fclose(file);
    }
    if (!has_error) lex(source_text, &tokens, &arena);
    Statement *program = NULL;
    if (!has_error) program = parse_program(&tokens, &arena);
    if (!has_error) run_program(program);
    free(tokens.items);
    arena_free(&arena);
    free(source_text);
#ifdef _WIN32
    if (old_input) SetConsoleCP(old_input);
    if (old_output) SetConsoleOutputCP(old_output);
    free(utf8_path);
#endif
    return has_error ? 1 : 0;
}

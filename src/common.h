#ifndef SIAN_COMMON_H
#define SIAN_COMMON_H

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
/* Parser jumps release their arenas explicitly. MinGW's SEH unwinder may reject
   a nested parser frame after a caught formatting error; plain C setjmp avoids
   invoking OS exception unwinding for this internal parser control flow. */
#ifdef __MINGW32__
#define __USE_MINGW_SETJMP_NON_SEH
#endif
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define SOURCE_LIMIT (16u * 1024u * 1024u)
#define TEXT_LIMIT (16u * 1024u * 1024u)
#define NAME_LIMIT 255u
#define ARG_LIMIT 256u
#define DEPTH_LIMIT 128u
#define SYNTAX_LIMIT 128u

typedef struct { int line, column; } Location;
static const char *source_name = "<source>";
static int has_error;
static Location error_location;
static char error_message[4096];
static char error_trace[8192];

static void error_at(Location at, const char *format, ...) {
    if (has_error) return;
    has_error = 1;
    error_location = at;
    error_trace[0] = '\0';
    va_list args;
    va_start(args, format);
    vsnprintf(error_message, sizeof(error_message), format, args);
    va_end(args);
}

static void print_error(void) {
    if (has_error) fprintf(stderr, "[Error] line %d: %s:%d: %s\n%s",
        error_location.line, source_name, error_location.column, error_message, error_trace);
}

static void trace_error(const char *name, Location at) {
    size_t length = strlen(error_trace);
    snprintf(error_trace + length, sizeof(error_trace) - length,
        "  called from %s at line %d, column %d\n", name, at.line, at.column);
}

static void *resize(void *ptr, size_t size) {
    void *result = realloc(ptr, size ? size : 1);
    if (!result) {
        fputs("[Error] out of memory\n", stderr);
        exit(1);
    }
    return result;
}

/* Syntax data lives until program exit. Runtime strings have separate ownership. */
typedef struct Allocation { struct Allocation *next; } Allocation;
typedef struct { Allocation *head; } Arena;

static void *arena_alloc(Arena *arena, size_t size) {
    Allocation *block = resize(NULL, sizeof(*block) + size);
    block->next = arena->head;
    arena->head = block;
    void *data = block + 1;
    memset(data, 0, size);
    return data;
}

static char *arena_text(Arena *arena, const char *text, size_t size) {
    char *copy = arena_alloc(arena, size + 1);
    memcpy(copy, text, size);
    return copy;
}

static void arena_free(Arena *arena) {
    while (arena->head) {
        Allocation *next = arena->head->next;
        free(arena->head);
        arena->head = next;
    }
}

static char *read_source(FILE *file) {
    size_t count = 0, capacity = 4096;
    char *text = resize(NULL, capacity);
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (count == SOURCE_LIMIT) {
            error_at((Location){0, 0}, "source exceeds 16 MiB limit");
            break;
        }
        if (ch == 0) {
            error_at((Location){0, 0}, "source contains a NUL byte");
            break;
        }
        if (count + 1 >= capacity) {
            capacity *= 2;
            text = resize(text, capacity);
        }
        text[count++] = (char)ch;
    }
    if (ferror(file)) error_at((Location){0, 0}, "failed to read source");
    text[count] = '\0';
    return text;
}

static int ascii_letter(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static int ascii_digit(unsigned char ch) { return ch >= '0' && ch <= '9'; }

#ifdef _WIN32
static wchar_t *windows_source_path(void) {
    /* Read the original Unicode argument: narrow argv loses names outside ACP.
       Resolve dynamically to preserve the existing gcc main.c build command. */
    typedef LPWSTR *(WINAPI *SplitCommandLine)(LPCWSTR, int *);
    HMODULE shell = LoadLibraryExW(L"shell32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!shell) return NULL;
    FARPROC address = GetProcAddress(shell, "CommandLineToArgvW");
    SplitCommandLine split = NULL;
    _Static_assert(sizeof(split) == sizeof(address), "Windows function pointer size");
    memcpy(&split, &address, sizeof(split));
    int count = 0;
    LPWSTR *args = split ? split(GetCommandLineW(), &count) : NULL;
    wchar_t *path = NULL;
    if (args && count == 2) {
        size_t bytes = (wcslen(args[1]) + 1) * sizeof(wchar_t);
        path = resize(NULL, bytes); memcpy(path, args[1], bytes);
    }
    if (args) LocalFree(args);
    FreeLibrary(shell);
    return path;
}
#endif

#endif

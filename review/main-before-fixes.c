#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 2048
#define MAX_VARS 256
#define MAX_FUNCS 128
#define MAX_ARGS 16
#define MAX_TEXT 1024

typedef enum { V_INT, V_FLOAT, V_STR, V_BOOL, V_VOID } ValueType;
typedef struct { ValueType type; long i; double f; int b; char *s; int from_input; } Value;
typedef struct { char name[64]; Value value; } Variable;
typedef struct Env { Variable vars[MAX_VARS]; int count; struct Env *parent; } Env;
typedef struct { char *text; int indent; int number; } SourceLine;
typedef struct { char name[64]; char params[MAX_ARGS][64]; int parameter_count; int body; int end; } Function;
typedef struct { int returned; Value value; } Flow;

static SourceLine source[MAX_LINES];
static int source_count = 0;
static Function functions[MAX_FUNCS];
static int function_count = 0;
static int has_error = 0;

static char *duplicate_text(const char *text) { char *copy = (char *)malloc(strlen(text) + 1); if (!copy) exit(1); strcpy(copy, text); return copy; }
static Value int_value(long n) { Value v = { V_INT, n, (double)n, n != 0, NULL, 0 }; return v; }
static Value float_value(double n) { Value v = { V_FLOAT, (long)n, n, n != 0, NULL, 0 }; return v; }
static Value bool_value(int n) { Value v = { V_BOOL, n != 0, (double)(n != 0), n != 0, NULL, 0 }; return v; }
static Value void_value(void) { Value v = { V_VOID, 0, 0, 0, NULL, 0 }; return v; }
static Value string_value(const char *text) { Value v = void_value(); v.type = V_STR; v.s = duplicate_text(text); return v; }
static void free_value(Value *v) { if (v->type == V_STR) free(v->s); v->s = NULL; }
static Value copy_value(Value v) { if (v.type == V_STR) { Value copy = string_value(v.s); copy.from_input = v.from_input; return copy; } return v; }
static void report_error(int line, const char *message) { fprintf(stderr, "[Error] line %d: %s\n", line, message); has_error = 1; }

static void trim(char *text) {
    size_t begin = 0, end = strlen(text);
    while (begin < end && isspace((unsigned char)text[begin])) begin++;
    while (end > begin && isspace((unsigned char)text[end - 1])) end--;
    memmove(text, text + begin, end - begin); text[end - begin] = '\0';
}

static int indent_of(const char *text) { int count = 0; while (*text == ' ' || *text == '\t') { count += *text == '\t' ? 4 : 1; text++; } return count; }

static void strip_comments(char *text, int *inside_block) {
    char output[MAX_TEXT]; int out = 0; size_t i = 0;
    while (text[i] && out < MAX_TEXT - 1) {
        if (*inside_block) { if (text[i] == '^' && text[i + 1] == '^') { *inside_block = 0; i += 2; } else i++; }
        else if (text[i] == '|' && text[i + 1] == '|') break;
        else if (text[i] == '^' && text[i + 1] == '^') { *inside_block = 1; i += 2; }
        else output[out++] = text[i++];
    }
    output[out] = '\0'; strcpy(text, output);
}

static void load_source(FILE *file) {
    char buffer[MAX_TEXT]; int block_comment = 0, number = 0;
    while (fgets(buffer, sizeof(buffer), file) && source_count < MAX_LINES) {
        int indent = indent_of(buffer); number++; strip_comments(buffer, &block_comment); trim(buffer);
        source[source_count].text = duplicate_text(buffer); source[source_count].indent = buffer[0] ? indent : 0; source[source_count].number = number; source_count++;
    }
}

static Variable *lookup(Env *env, const char *name) {
    int i; while (env) { for (i = 0; i < env->count; i++) if (strcmp(env->vars[i].name, name) == 0) return &env->vars[i]; env = env->parent; } return NULL;
}

static void assign(Env *env, const char *name, Value value) {
    Variable *variable = lookup(env, name);
    if (!variable) { if (env->count >= MAX_VARS) exit(1); variable = &env->vars[env->count++]; strcpy(variable->name, name); variable->value = void_value(); }
    free_value(&variable->value); variable->value = copy_value(value);
}

static int matches_type(const char *type, Value value) {
    if (strcmp(type, "int") == 0) return value.type == V_INT;
    if (strcmp(type, "float") == 0) return value.type == V_INT || value.type == V_FLOAT;
    if (strcmp(type, "str") == 0) return value.type == V_STR;
    return value.type == V_BOOL;
}

static int convert_input(Value *value, const char *type) {
    char *end; char *start; char *last;
    long integer;
    double decimal;
    if (!value->from_input || value->type != V_STR || strcmp(type, "str") == 0) return 1;
    start = value->s;
    while (isspace((unsigned char)*start)) start++;
    last = start + strlen(start);
    while (last > start && isspace((unsigned char)last[-1])) last--;
    *last = '\0';
    if (strcmp(type, "int") == 0) {
        integer = strtol(start, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (*start == '\0' || *end != '\0') return 0;
        free_value(value); *value = int_value(integer); return 1;
    }
    if (strcmp(type, "float") == 0) {
        decimal = strtod(start, &end);
        while (isspace((unsigned char)*end)) end++;
        if (*start == '\0' || *end != '\0') return 0;
        free_value(value); *value = float_value(decimal); return 1;
    }
    if (strcmp(type, "bool") == 0 || strcmp(type, "TF") == 0) {
        if (strcmp(start, "true") == 0) { free_value(value); *value = bool_value(1); return 1; }
        if (strcmp(start, "false") == 0) { free_value(value); *value = bool_value(0); return 1; }
        return 0;
    }
    return 0;
}

typedef struct { const char *text; int position; int line; Env *env; } Parser;
static Value expression(Parser *parser);
extern Flow execute_range(int start, int end, Env *env);
static void spaces(Parser *p) { while (isspace((unsigned char)p->text[p->position])) p->position++; }
static int take(Parser *p, char character) { spaces(p); if (p->text[p->position] == character) { p->position++; return 1; } return 0; }
static int truth(Value v) { return v.type == V_STR ? v.s[0] != '\0' : v.b; }

static Value primary(Parser *p) {
    char token[256]; int length = 0; spaces(p);
    if (take(p, '(')) { Value v = expression(p); if (!take(p, ')')) report_error(p->line, "need a closing parenthesis"); return v; }
    if (p->text[p->position] == '"') {
        p->position++; while (p->text[p->position] && p->text[p->position] != '"' && length < 255) token[length++] = p->text[p->position++]; token[length] = '\0';
        if (p->text[p->position] == '"') p->position++; else report_error(p->line, "need a closing quote"); return string_value(token);
    }
    if (isdigit((unsigned char)p->text[p->position]) || (p->text[p->position] == '-' && isdigit((unsigned char)p->text[p->position + 1]))) {
        int decimal = 0; while (isdigit((unsigned char)p->text[p->position]) || p->text[p->position] == '.') { if (p->text[p->position] == '.') decimal = 1; token[length++] = p->text[p->position++]; } token[length] = '\0'; return decimal ? float_value(strtod(token, NULL)) : int_value(strtol(token, NULL, 10));
    }
    if (isalpha((unsigned char)p->text[p->position]) || p->text[p->position] == '_') {
        while (isalnum((unsigned char)p->text[p->position]) || p->text[p->position] == '_') token[length++] = p->text[p->position++];
        token[length] = '\0';
        if (strcmp(token, "true") == 0) return bool_value(1);
        if (strcmp(token, "false") == 0) return bool_value(0);
        spaces(p);
        if (p->text[p->position] == '(') {
            Function *function = NULL; Value args[MAX_ARGS]; int argc = 0, i; p->position++;
            if (!take(p, ')')) { do { if (argc < MAX_ARGS) args[argc++] = expression(p); } while (take(p, ',')); if (!take(p, ')')) report_error(p->line, "need a closing parenthesis for the call"); }
            if (strcmp(token, "input") == 0) {
                char buffer[MAX_TEXT];
                if (argc > 1 || (argc == 1 && args[0].type != V_STR)) { report_error(p->line, "input prompt must be a string"); return void_value(); }
                if (argc == 1) { printf("%s", args[0].s); fflush(stdout); }
                if (!fgets(buffer, sizeof(buffer), stdin)) return string_value("");
                buffer[strcspn(buffer, "\r\n")] = '\0';
                { Value input = string_value(buffer); input.from_input = 1; return input; }
            }
            for (i = 0; i < function_count; i++) if (strcmp(functions[i].name, token) == 0) function = &functions[i];
            if (!function) { report_error(p->line, "function not found"); return void_value(); }
            if (argc != function->parameter_count) { report_error(p->line, "wrong number of function arguments"); return void_value(); }
            { Env local; Flow flow; local.count = 0; local.parent = p->env; for (i = 0; i < argc; i++) assign(&local, function->params[i], args[i]); flow = execute_range(function->body, function->end, &local); return flow.returned ? flow.value : void_value(); }
        }
        { Variable *variable = lookup(p->env, token); if (!variable) { report_error(p->line, "variable not found"); return void_value(); } return copy_value(variable->value); }
    }
    report_error(p->line, "cannot read this expression"); return void_value();
}

static Value unary(Parser *p) { if (take(p, '!')) return bool_value(!truth(unary(p))); if (take(p, '-')) { Value v = unary(p); return v.type == V_FLOAT ? float_value(-v.f) : int_value(-v.i); } return primary(p); }
static Value factor(Parser *p) {
    Value left = unary(p); while (1) { Value right; char op; spaces(p); if (p->text[p->position] != '*' && p->text[p->position] != '/' && p->text[p->position] != '%') return left; op = p->text[p->position++]; right = unary(p); if (op == '/' && right.f == 0) { report_error(p->line, "cannot divide by zero"); return void_value(); } if (left.type == V_FLOAT || right.type == V_FLOAT) left = op == '*' ? float_value(left.f * right.f) : float_value(left.f / right.f); else if (op == '*') left = int_value(left.i * right.i); else if (op == '/') left = int_value(left.i / right.i); else left = int_value(left.i % right.i); }
}
static Value sum(Parser *p) {
    Value left = factor(p); while (1) { Value right; char op; spaces(p); if (p->text[p->position] != '+' && p->text[p->position] != '-') return left; op = p->text[p->position++]; right = factor(p); if (op == '+' && (left.type == V_STR || right.type == V_STR)) { char buffer[MAX_TEXT]; snprintf(buffer, sizeof(buffer), "%s%s", left.type == V_STR ? left.s : "", right.type == V_STR ? right.s : ""); free_value(&left); free_value(&right); left = string_value(buffer); } else left = op == '+' ? float_value(left.f + right.f) : float_value(left.f - right.f); }
}
static Value expression(Parser *p) {
    Value left = sum(p), right; char op[3] = {0}; spaces(p);
    if ((p->text[p->position] == '=' || p->text[p->position] == '!') && p->text[p->position + 1] == '=') { op[0] = p->text[p->position++]; op[1] = p->text[p->position++]; }
    else if (p->text[p->position] == '>' || p->text[p->position] == '<') { op[0] = p->text[p->position++]; if (p->text[p->position] == '=') op[1] = p->text[p->position++]; }
    if (!op[0]) return left;
    right = sum(p);
    if (strcmp(op, "==") == 0) return bool_value(left.type == V_STR && right.type == V_STR ? strcmp(left.s, right.s) == 0 : left.f == right.f);
    if (strcmp(op, "!=") == 0) return bool_value(left.f != right.f);
    if (op[0] == '>') return bool_value(op[1] ? left.f >= right.f : left.f > right.f);
    return bool_value(op[1] ? left.f <= right.f : left.f < right.f);
}
static Value evaluate(const char *text, int line, Env *env) { Parser p = {text, 0, line, env}; return expression(&p); }

static int keyword(const char *text, const char *word) { size_t length = strlen(word); return strncmp(text, word, length) == 0 && (text[length] == '\0' || isspace((unsigned char)text[length]) || text[length] == '('); }
static int end_of_block(int start, int end, int parent_indent) { while (start < end && (!source[start].text[0] || source[start].indent > parent_indent)) start++; return start; }

static void register_functions(void) {
    int i; for (i = 0; i < source_count; i++) {
        char name[64], params[MAX_TEXT]; int matched = sscanf(source[i].text, "def %63[^ (](%1023[^)])", name, params) == 2 || sscanf(source[i].text, "func %63[^ (](%1023[^)])", name, params) == 2 || sscanf(source[i].text, "f %63[^ (](%1023[^)])", name, params) == 2 || sscanf(source[i].text, "function %63[^ (](%1023[^)])", name, params) == 2;
        if (matched && function_count < MAX_FUNCS) { Function *f = &functions[function_count++]; char *part, *cursor = params; strcpy(f->name, name); f->parameter_count = 0; f->body = i + 1; f->end = end_of_block(i + 1, source_count, source[i].indent); while ((part = strtok(cursor, ",")) != NULL) { trim(part); strcpy(f->params[f->parameter_count++], part); cursor = NULL; } }
    }
}

Flow execute_range(int start, int end, Env *env) {
    int i = start;
    while (i < end) {
        char *text = source[i].text; int child_start, child_end; Value value;
        if (!*text) { i++; continue; }
        if (keyword(text, "def") || keyword(text, "func") || keyword(text, "f") || keyword(text, "function")) { i = end_of_block(i + 1, end, source[i].indent); continue; }
        if (keyword(text, "iferror")) {
            value = evaluate(text + 7, source[i].number, env);
            if (value.type == V_STR) report_error(source[i].number, value.s);
            else report_error(source[i].number, "iferror message must be a string");
            free_value(&value);
            return (Flow){1, void_value()};
        }
        if (keyword(text, "return")) {
            char *return_text = text + 6;
            while (isspace((unsigned char)*return_text)) return_text++;
            value = *return_text ? evaluate(return_text, source[i].number, env) : void_value();
            return (Flow){1, value};
        }
        if (keyword(text, "log")) { value = evaluate(text + 3, source[i].number, env); if (value.type == V_STR) printf("%s\n", value.s); else if (value.type == V_FLOAT) printf("%g\n", value.f); else if (value.type == V_BOOL) printf("%s\n", value.b ? "true" : "false"); else printf("%ld\n", value.i); free_value(&value); i++; continue; }
        if (keyword(text, "if") || keyword(text, "while") || keyword(text, "loop")) {
            int is_if = keyword(text, "if"), offset = is_if ? 2 : (keyword(text, "while") ? 5 : 4), condition_true; value = evaluate(text + offset, source[i].number, env); condition_true = truth(value); free_value(&value); child_start = i + 1; child_end = end_of_block(child_start, end, source[i].indent);
            if (is_if) {
                int branch_taken = condition_true;
                if (condition_true) {
                    Flow flow = execute_range(child_start, child_end, env);
                    if (flow.returned) return flow;
                }
                i = child_end;
                while (i < end && keyword(source[i].text, "else")) {
                    int else_end = end_of_block(i + 1, end, source[i].indent);
                    int else_if = keyword(source[i].text, "else if");
                    int else_condition = !branch_taken;
                    if (else_if && else_condition) {
                        Value else_value = evaluate(source[i].text + 7, source[i].number, env);
                        else_condition = truth(else_value);
                        free_value(&else_value);
                    }
                    if (else_condition) {
                        Flow flow = execute_range(i + 1, else_end, env);
                        if (flow.returned) return flow;
                        branch_taken = 1;
                    }
                    i = else_end;
                    if (!else_if) break;
                }
                continue;
            }
            while (1) { value = evaluate(text + offset, source[i].number, env); if (!truth(value)) { free_value(&value); break; } free_value(&value); { Flow flow = execute_range(child_start, child_end, env); if (flow.returned) return flow; } }
            i = child_end; continue;
        }
        if (keyword(text, "repeat")) { int count, n = 0; value = evaluate(text + 6, source[i].number, env); count = (int)value.i; free_value(&value); child_start = i + 1; child_end = end_of_block(child_start, end, source[i].indent); while (n++ < count) { Flow flow = execute_range(child_start, child_end, env); if (flow.returned) return flow; } i = child_end; continue; }
        {
            char type[16], name[64], expression_text[MAX_TEXT];
            if (sscanf(text, "%15s %63[^= ] = %1023[^\n]", type, name, expression_text) == 3 && (!strcmp(type, "int") || !strcmp(type, "float") || !strcmp(type, "str") || !strcmp(type, "bool") || !strcmp(type, "TF"))) { value = evaluate(expression_text, source[i].number, env); if (!convert_input(&value, type) || !matches_type(type, value)) report_error(source[i].number, "input does not match variable type"); else { if (!strcmp(type, "float") && value.type == V_INT) value = float_value(value.f); assign(env, name, value); } free_value(&value); i++; continue; }
            if (sscanf(text, "%63[^= ] = %1023[^\n]", name, expression_text) == 2) { value = evaluate(expression_text, source[i].number, env); if (!lookup(env, name)) report_error(source[i].number, "variable for assignment not found"); else assign(env, name, value); free_value(&value); i++; continue; }
        }
        report_error(source[i].number, "unknown statement"); i++;
    }
    return (Flow){0, void_value()};
}

int main(int argc, char **argv) {
    FILE *file; Env global; int length;
    system("chcp 65001 > nul");
    if (argc < 2) { fprintf(stderr, "[Error] line 0: give a .sian file\n"); return 1; }
    length = (int)strlen(argv[1]); if (length < 5 || strcmp(argv[1] + length - 5, ".sian") != 0) { fprintf(stderr, "[Error] line 0: file must end with .sian\n"); return 1; }
    file = fopen(argv[1], "r"); if (!file) { fprintf(stderr, "[Error] line 0: cannot open file %s\n", argv[1]); return 1; }
    load_source(file); fclose(file); register_functions(); global.count = 0; global.parent = NULL; execute_range(0, source_count, &global); return has_error ? 1 : 0;
}

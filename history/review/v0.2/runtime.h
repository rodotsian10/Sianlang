#ifndef SIAN_RUNTIME_H
#define SIAN_RUNTIME_H

typedef struct { size_t refs, length; char text[]; } String;
typedef struct {
    ValueType type;
    union { int64_t integer; double decimal; String *string; int boolean; } as;
} Value;
typedef struct { const char *name; ValueType type; Value value; } Variable;
typedef struct Env { Variable *vars; size_t count, capacity; struct Env *parent; } Env;
typedef enum { FLOW_NORMAL, FLOW_RETURN, FLOW_BREAK, FLOW_CONTINUE, FLOW_ERROR } FlowKind;
typedef struct { FlowKind kind; Value value; } Flow;
typedef struct {
    Statement *program;
    Env global;
    unsigned int call_depth, eval_depth, block_depth;
} Runtime;
static size_t live_strings;

static Value nothing(void) { Value v = {0}; return v; }
static Value integer_value(int64_t n) { Value v = {.type = V_INT}; v.as.integer = n; return v; }
static Value boolean_value(int b) { Value v = {.type = V_BOOL}; v.as.boolean = b != 0; return v; }
static Value decimal_value(double d, Location at) {
    if (!isfinite(d)) { error_at(at, "floating-point result is outside the finite range"); return nothing(); }
    Value v = {.type = V_FLOAT}; v.as.decimal = d; return v;
}
static Value text_value(const char *text, size_t length, Location at) {
    if (length > TEXT_LIMIT) { error_at(at, "string exceeds 16 MiB limit"); return nothing(); }
    String *s = resize(NULL, sizeof(*s) + length + 1);
    s->refs = 1; s->length = length;
    memcpy(s->text, text, length); s->text[length] = '\0';
    live_strings++;
    Value v = {.type = V_STR}; v.as.string = s; return v;
}
static Value retain(Value v) { if (v.type == V_STR) v.as.string->refs++; return v; }
static void release(Value v) {
    if (v.type == V_STR && --v.as.string->refs == 0) { free(v.as.string); live_strings--; }
}
static const char *type_label(ValueType type) {
    static const char *labels[] = {"void", "int", "float", "str", "bool"};
    return labels[type];
}
static int numeric(Value v) { return v.type == V_INT || v.type == V_FLOAT; }
static double number(Value v) { return v.type == V_INT ? (double)v.as.integer : v.as.decimal; }
static int truth_value(Value v, Location at) {
    switch (v.type) {
        case V_INT: return v.as.integer != 0;
        case V_FLOAT: return v.as.decimal != 0;
        case V_STR: return v.as.string->length != 0;
        case V_BOOL: return v.as.boolean;
        default: error_at(at, "a function without a return value cannot be used as a condition"); return 0;
    }
}
static Variable *find_local(Env *env, const char *name) {
    for (size_t i = 0; i < env->count; i++) if (!strcmp(env->vars[i].name, name)) return &env->vars[i];
    return NULL;
}
static Variable *find_variable(Env *env, const char *name) {
    for (; env; env = env->parent) {
        Variable *v = find_local(env, name);
        if (v) return v;
    }
    return NULL;
}
static void define_variable(Env *env, const char *name, ValueType type, Value value) {
    Variable *v = find_local(env, name);
    if (!v) {
        if (env->count == env->capacity) {
            env->capacity = env->capacity ? env->capacity * 2 : 16;
            env->vars = resize(env->vars, env->capacity * sizeof(*env->vars));
        }
        v = &env->vars[env->count++];
        *v = (Variable){name, type, nothing()};
    }
    release(v->value); v->value = retain(value);
}
static void free_env(Env *env) {
    for (size_t i = 0; i < env->count; i++) release(env->vars[i].value);
    free(env->vars);
}

static const char *skip_space(const char *s) { while (isspace((unsigned char)*s)) s++; return s; }
static int decimal_text(const char *s) {
    s = skip_space(s);
    if (*s == '+' || *s == '-') s++;
    int digits = 0;
    while (ascii_digit((unsigned char)*s)) { s++; digits = 1; }
    if (*s == '.') { s++; while (ascii_digit((unsigned char)*s)) { s++; digits = 1; } }
    if (!digits) return 0;
    if (*s == 'e' || *s == 'E') {
        s++; if (*s == '+' || *s == '-') s++;
        if (!ascii_digit((unsigned char)*s)) return 0;
        while (ascii_digit((unsigned char)*s)) s++;
    }
    return !*skip_space(s);
}
/* Returns an owned value; the caller still owns its input value. */
static Value convert_value(Value value, ValueType target, Location at) {
    if (value.type == target) return retain(value);
    if (value.type == V_VOID) {
        error_at(at, "cannot convert a function without a return value"); return nothing();
    }
    if (target == V_STR) {
        char buffer[96];
        if (value.type == V_INT) snprintf(buffer, sizeof(buffer), "%" PRId64, value.as.integer);
        else if (value.type == V_FLOAT) snprintf(buffer, sizeof(buffer), "%.15g", value.as.decimal);
        else snprintf(buffer, sizeof(buffer), "%s", value.as.boolean ? "true" : "false");
        return text_value(buffer, strlen(buffer), at);
    }
    if (value.type == V_STR) {
        const char *s = skip_space(value.as.string->text);
        char *end;
        errno = 0;
        if (target == V_INT) {
            int64_t n = strtoll(s, &end, 10);
            if (end != s && !*skip_space(end) && errno != ERANGE) return integer_value(n);
        } else if (target == V_FLOAT) {
            double d = strtod(s, &end);
            if (decimal_text(s) && end != s && !*skip_space(end) && errno != ERANGE && isfinite(d)) return decimal_value(d, at);
        } else if (target == V_BOOL) {
            if (!strncmp(s, "true", 4) && !*skip_space(s + 4)) return boolean_value(1);
            if (!strncmp(s, "false", 5) && !*skip_space(s + 5)) return boolean_value(0);
        }
        error_at(at, "cannot convert input/string to %s: invalid text or out of range", type_label(target));
        return nothing();
    }
    if (target == V_BOOL) return boolean_value(truth_value(value, at));
    if (value.type == V_BOOL) {
        if (target == V_INT) return integer_value(value.as.boolean);
        if (target == V_FLOAT) return decimal_value(value.as.boolean, at);
    }
    if (target == V_FLOAT && value.type == V_INT) return decimal_value((double)value.as.integer, at);
    if (target == V_INT && value.type == V_FLOAT) {
        double d = value.as.decimal;
        if (d >= -0x1p63 && d < 0x1p63) return integer_value((int64_t)d);
        error_at(at, "float is outside int range"); return nothing();
    }
    error_at(at, "cannot convert %s to %s", type_label(value.type), type_label(target));
    return nothing();
}

static Value input_value(Value *args, size_t argc, Location at) {
    if (argc > 1 || (argc == 1 && args[0].type != V_STR)) {
        error_at(at, "input expects zero arguments or one str prompt"); return nothing();
    }
    if (argc) { fputs(args[0].as.string->text, stdout); fflush(stdout); }
    size_t length = 0, capacity = 256;
    char *buffer = resize(NULL, capacity);
    int ch;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (ch == 0) { error_at(at, "input contains a NUL byte"); break; }
        if (length == TEXT_LIMIT) { error_at(at, "input exceeds 16 MiB limit"); break; }
        if (length + 1 == capacity) { capacity *= 2; buffer = resize(buffer, capacity); }
        buffer[length++] = (char)ch;
    }
    if (ferror(stdin)) error_at(at, "failed to read input");
    else if (ch == EOF && !length) error_at(at, "end of input: no line available");
    if (length && buffer[length - 1] == '\r') length--;
    Value result = has_error ? nothing() : text_value(buffer, length, at);
    free(buffer); return result;
}

/* Exact mixed comparison near the int64/double boundary, without rounding the integer. */
static int compare_int_float(int64_t i, double d) {
    if (d >= 0x1p63) return -1;
    if (d < -0x1p63) return 1;
    int64_t whole = (int64_t)d;
    if (i != whole) return i < whole ? -1 : 1;
    double truncated = (double)whole;
    return truncated < d ? -1 : truncated > d ? 1 : 0;
}
static int compare_numeric(Value a, Value b) {
    if (a.type == V_INT && b.type == V_INT) return a.as.integer < b.as.integer ? -1 : a.as.integer > b.as.integer ? 1 : 0;
    if (a.type == V_INT) return compare_int_float(a.as.integer, b.as.decimal);
    if (b.type == V_INT) return -compare_int_float(b.as.integer, a.as.decimal);
    return a.as.decimal < b.as.decimal ? -1 : a.as.decimal > b.as.decimal ? 1 : 0;
}
static Value compare_values(Operator op, Value a, Value b, Location at) {
    if (a.type == V_VOID || b.type == V_VOID) {
        error_at(at, "cannot compare a function without a return value"); return nothing();
    }
    int cmp;
    if (numeric(a) && numeric(b)) cmp = compare_numeric(a, b);
    else if (a.type == V_STR && b.type == V_STR) cmp = strcmp(a.as.string->text, b.as.string->text);
    else if (a.type == V_BOOL && b.type == V_BOOL && (op == OP_EQ || op == OP_NE)) cmp = a.as.boolean - b.as.boolean;
    else if ((op == OP_EQ || op == OP_NE) && a.type != b.type) return boolean_value(op == OP_NE);
    else { error_at(at, "cannot order %s and %s", type_label(a.type), type_label(b.type)); return nothing(); }
    return boolean_value(op == OP_EQ ? cmp == 0 : op == OP_NE ? cmp != 0 : op == OP_LT ? cmp < 0 :
                         op == OP_LE ? cmp <= 0 : op == OP_GT ? cmp > 0 : cmp >= 0);
}
static int checked_integer(Operator op, int64_t a, int64_t b, int64_t *result) {
    if (op == OP_ADD) {
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
        *result = a + b;
    } else if (op == OP_SUB) {
        if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return 0;
        *result = a - b;
    } else {
        if (a > 0) {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) return 0;
        } else if (a < 0) {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) return 0;
        }
        *result = a * b;
    }
    return 1;
}
static Value arithmetic(Operator op, Value a, Value b, Location at) {
    if (op >= OP_EQ && op <= OP_GE) return compare_values(op, a, b, at);
    if (op == OP_ADD && a.type == V_STR && b.type == V_STR) {
        size_t length = a.as.string->length + b.as.string->length;
        if (length > TEXT_LIMIT) { error_at(at, "string exceeds 16 MiB limit"); return nothing(); }
        char *joined = resize(NULL, length + 1);
        memcpy(joined, a.as.string->text, a.as.string->length);
        memcpy(joined + a.as.string->length, b.as.string->text, b.as.string->length);
        Value result = text_value(joined, length, at); free(joined); return result;
    }
    if (!numeric(a) || !numeric(b)) {
        error_at(at, "arithmetic requires numbers (got %s and %s); use str(...) for text conversion", type_label(a.type), type_label(b.type));
        return nothing();
    }
    if ((op == OP_DIV || op == OP_MOD) && number(b) == 0) {
        error_at(at, "cannot divide or take remainder by zero"); return nothing();
    }
    if (op == OP_MOD && a.type == V_INT && b.type == V_INT) {
        int64_t i = a.as.integer, j = b.as.integer;
        int64_t result = i == INT64_MIN && j == -1 ? 0 : i % j;
        if (result && ((result < 0) != (j < 0))) result += j;
        return integer_value(result);
    }
    if (a.type == V_INT && b.type == V_INT && op != OP_DIV) {
        int64_t result;
        if (!checked_integer(op, a.as.integer, b.as.integer, &result)) {
            error_at(at, "integer overflow"); return nothing();
        }
        return integer_value(result);
    }
    double x = number(a), y = number(b), result;
    switch (op) {
        case OP_ADD: result = x + y; break;
        case OP_SUB: result = x - y; break;
        case OP_MUL: result = x * y; break;
        case OP_DIV: result = x / y; break;
        default:
            result = fmod(x, y);
            if (result && ((result < 0) != (y < 0))) result += y;
            break;
    }
    return decimal_value(result, at);
}

static Value evaluate(Runtime *runtime, Env *env, Expr *expr);
static Flow execute(Runtime *runtime, Env *env, Statement *statement);

static Value call_function(Runtime *runtime, Env *env, Expr *expr) {
    int is_input = !strcmp(expr->text, "input");
    ValueType conversion = type_named(expr->text);
    Statement *function = NULL;
    if (!is_input && !conversion) {
        for (Statement *s = runtime->program; s; s = s->next)
            if (s->kind == S_FUNCTION && !strcmp(s->name, expr->text)) { function = s; break; }
        if (!function) { error_at(expr->at, "function '%s' not found", expr->text); return nothing(); }
        if (expr->argc != function->parameter_count) {
            error_at(expr->at, "function '%s' expects %zu arguments, got %zu", expr->text, function->parameter_count, expr->argc);
            return nothing();
        }
    } else if ((conversion && expr->argc != 1) || (is_input && expr->argc > 1)) {
        error_at(expr->at, "wrong number of arguments to '%s'", expr->text); return nothing();
    }
    if (runtime->call_depth >= DEPTH_LIMIT) {
        error_at(expr->at, "call depth exceeds %u", DEPTH_LIMIT); return nothing();
    }
    Value *args = resize(NULL, expr->argc * sizeof(Value));
    size_t count = 0;
    for (ExprList *item = expr->args; item && !has_error; item = item->next) {
        args[count] = evaluate(runtime, env, item->value);
        if (!has_error && args[count].type == V_VOID) error_at(item->value->at, "argument has no return value");
        count++;
    }
    Value result = nothing();
    if (!has_error) {
        if (is_input) result = input_value(args, count, expr->at);
        else if (conversion) result = convert_value(args[0], conversion, expr->at);
        else {
            Env local = {.parent = &runtime->global};
            size_t i = 0;
            for (NameList *param = function->params; param; param = param->next) {
                define_variable(&local, param->name, args[i].type, args[i]); i++;
            }
            runtime->call_depth++;
            Flow flow = execute(runtime, &local, function->body);
            runtime->call_depth--;
            if (flow.kind == FLOW_RETURN && !has_error) result = flow.value;
            else release(flow.value);
            free_env(&local);
            if (has_error) fprintf(stderr, "  called from %s at line %d, column %d\n", expr->text, expr->at.line, expr->at.column);
        }
    }
    for (size_t i = 0; i < count; i++) release(args[i]);
    free(args);
    return result;
}

static Value evaluate_inner(Runtime *runtime, Env *env, Expr *expr) {
    switch (expr->kind) {
        case E_INT: return integer_value(expr->integer);
        case E_FLOAT: return decimal_value(expr->decimal, expr->at);
        case E_BOOL: return boolean_value(expr->integer != 0);
        case E_STRING: return text_value(expr->text, strlen(expr->text), expr->at);
        case E_NAME: {
            Variable *v = find_variable(env, expr->text);
            if (!v) { error_at(expr->at, "variable '%s' not found", expr->text); return nothing(); }
            return retain(v->value);
        }
        case E_CALL: return call_function(runtime, env, expr);
        case E_UNARY: {
            Value right = evaluate(runtime, env, expr->right), result = nothing();
            if (!has_error) {
                if (expr->op == OP_NOT) result = boolean_value(!truth_value(right, expr->at));
                else if (!numeric(right)) error_at(expr->at, "unary sign requires a number, got %s", type_label(right.type));
                else if (expr->op == OP_ADD) result = retain(right);
                else if (right.type == V_FLOAT) result = decimal_value(-right.as.decimal, expr->at);
                else if (right.as.integer == INT64_MIN) error_at(expr->at, "integer overflow in negation");
                else result = integer_value(-right.as.integer);
            }
            release(right); return result;
        }
        case E_BINARY: {
            Value left = evaluate(runtime, env, expr->left), right = nothing(), result = nothing();
            if (!has_error && (expr->op == OP_AND || expr->op == OP_OR)) {
                int first = truth_value(left, expr->at);
                if (!has_error && ((expr->op == OP_AND && !first) || (expr->op == OP_OR && first))) result = boolean_value(first);
                else if (!has_error) {
                    right = evaluate(runtime, env, expr->right);
                    if (!has_error) result = boolean_value(truth_value(right, expr->at));
                }
            } else if (!has_error) {
                right = evaluate(runtime, env, expr->right);
                if (!has_error) result = arithmetic(expr->op, left, right, expr->at);
            }
            release(left); release(right); return result;
        }
    }
    return nothing();
}
static Value evaluate(Runtime *runtime, Env *env, Expr *expr) {
    if (has_error || !expr) return nothing();
    if (runtime->eval_depth >= DEPTH_LIMIT) {
        error_at(expr->at, "evaluation nesting exceeds %u", DEPTH_LIMIT); return nothing();
    }
    runtime->eval_depth++;
    Value value = evaluate_inner(runtime, env, expr);
    runtime->eval_depth--;
    return value;
}

static Value assignment_value(Value v, ValueType target, Expr *expr, const char *name) {
    if (v.type == target) return retain(v);
    int direct_input = expr->kind == E_CALL && !strcmp(expr->text, "input");
    if (direct_input || (target == V_FLOAT && v.type == V_INT)) return convert_value(v, target, expr->at);
    error_at(expr->at, "variable '%s' expects %s, got %s", name, type_label(target), type_label(v.type));
    return nothing();
}
static void log_value(Value value, Location at) {
    switch (value.type) {
        case V_STR: puts(value.as.string->text); break;
        case V_INT: printf("%" PRId64 "\n", value.as.integer); break;
        case V_FLOAT: printf("%.15g\n", value.as.decimal); break;
        case V_BOOL: puts(value.as.boolean ? "true" : "false"); break;
        default: error_at(at, "cannot log a function without a return value"); break;
    }
}

static Flow execute_inner(Runtime *runtime, Env *env, Statement *statement) {
    for (Statement *s = statement; s && !has_error; s = s->next) {
        if (s->kind == S_FUNCTION) continue;
        if (s->kind == S_BREAK) return (Flow){FLOW_BREAK, nothing()};
        if (s->kind == S_CONTINUE) return (Flow){FLOW_CONTINUE, nothing()};
        if (s->kind == S_IF) {
            /* Iterate else-if chains rather than consuming the C stack for each branch. */
            Statement *branch = s;
            while (branch && branch->kind == S_IF && !has_error) {
                Value condition = evaluate(runtime, env, branch->expr);
                int take_branch = !has_error && truth_value(condition, branch->at);
                release(condition);
                if (has_error) break;
                if (take_branch) { branch = branch->body; break; }
                if (branch->alternate_is_if) branch = branch->otherwise;
                else { branch = branch->otherwise; break; }
            }
            if (!has_error && branch) {
                Flow flow = execute(runtime, env, branch);
                if (flow.kind != FLOW_NORMAL) return flow;
            }
            continue;
        }
        if (s->kind == S_WHILE || s->kind == S_REPEAT) {
            int64_t remaining = 0;
            if (s->kind == S_REPEAT) {
                Value count = evaluate(runtime, env, s->expr);
                if (!has_error && (count.type != V_INT || count.as.integer < 0))
                    error_at(s->at, "repeat count must be a nonnegative int");
                if (!has_error) remaining = count.as.integer;
                release(count);
            }
            while (!has_error) {
                if (s->kind == S_REPEAT) { if (!remaining) break; remaining--; }
                else {
                    Value condition = evaluate(runtime, env, s->expr);
                    int active = !has_error && truth_value(condition, s->at);
                    release(condition);
                    if (!active || has_error) break;
                }
                Flow flow = execute(runtime, env, s->body);
                if (flow.kind == FLOW_RETURN || flow.kind == FLOW_ERROR) return flow;
                if (flow.kind == FLOW_BREAK) break;
            }
            continue;
        }
        /* Resolve assignment target before running its RHS, but re-resolve after calls
           because declarations inside a called function can grow an environment. */
        ValueType target = s->type;
        if (s->kind == S_ASSIGN) {
            Variable *v = find_variable(env, s->name);
            if (!v) { error_at(s->at, "variable '%s' for assignment not found", s->name); break; }
            target = v->type;
        } else if (s->kind == S_DECLARE) {
            Variable *v = find_local(env, s->name);
            if (v && v->type != s->type) {
                error_at(s->at, "variable '%s' is already declared as %s", s->name, type_label(v->type)); break;
            }
        }
        Value value = evaluate(runtime, env, s->expr);
        if (has_error) { release(value); break; }
        if (s->kind == S_RETURN) return (Flow){FLOW_RETURN, value};
        if (s->kind == S_LOG) log_value(value, s->at);
        else if (s->kind == S_ERROR) {
            if (value.type != V_STR) error_at(s->at, "iferror message must be str");
            else error_at(s->at, "%s", value.as.string->text);
        } else if (s->kind == S_DECLARE || s->kind == S_ASSIGN) {
            Value converted = assignment_value(value, target, s->expr, s->name);
            if (!has_error) {
                if (s->kind == S_DECLARE) define_variable(env, s->name, target, converted);
                else {
                    Variable *v = find_variable(env, s->name);
                    release(v->value); v->value = retain(converted);
                }
            }
            release(converted);
        }
        release(value);
    }
    return (Flow){has_error ? FLOW_ERROR : FLOW_NORMAL, nothing()};
}
static Flow execute(Runtime *runtime, Env *env, Statement *statement) {
    if (runtime->block_depth >= DEPTH_LIMIT * 2) {
        error_at(statement->at, "execution nesting is too deep"); return (Flow){FLOW_ERROR, nothing()};
    }
    runtime->block_depth++;
    Flow flow = execute_inner(runtime, env, statement);
    runtime->block_depth--;
    return flow;
}
static void run_program(Statement *program) {
    Runtime runtime = {.program = program};
    Flow flow = execute(&runtime, &runtime.global, program);
    release(flow.value);
    free_env(&runtime.global);
    if (live_strings) {
        fprintf(stderr, "[Internal error] %zu string allocations remain\n", live_strings);
        has_error = 1;
    }
}
#endif

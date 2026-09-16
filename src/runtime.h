#ifndef SIAN_RUNTIME_H
#define SIAN_RUNTIME_H

#include "values.h"
#include "printing.h"
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
    if (target == V_STR) return display_value(value, 0, at);
    if (value.type == V_VOID) {
        if (target == V_BOOL) return boolean_value(0);
        error_at(at, "cannot convert None to %s", type_label(target)); return nothing();
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
    int cmp;
    if (numeric(a) && numeric(b)) cmp = compare_numeric(a, b);
    else if (a.type == V_STR && b.type == V_STR) cmp = strcmp(a.as.string->text, b.as.string->text);
    else if (a.type == V_BOOL && b.type == V_BOOL && (op == OP_EQ || op == OP_NE)) cmp = a.as.boolean - b.as.boolean;
    else if ((op == OP_EQ || op == OP_NE) && a.type == V_VOID && b.type == V_VOID) cmp = 0;
    else if ((op == OP_EQ || op == OP_NE) && a.type == V_FUNCTION && b.type == V_FUNCTION) cmp = a.as.function != b.as.function;
    else if ((op == OP_EQ || op == OP_NE) && (a.type == V_TUPLE || a.type == V_LIST || a.type == V_DICT || a.type == V_FILE) && a.type == b.type) cmp = a.as.list != b.as.list;
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

#include "calls.h"

static Value evaluate_inner(Runtime *runtime, Env *env, Expr *expr) {
    switch (expr->kind) {
        case E_NONE: return nothing();
        case E_INT: return integer_value(expr->integer);
        case E_FLOAT: return decimal_value(expr->decimal, expr->at);
        case E_BOOL: return boolean_value(expr->integer != 0);
        case E_STRING: return text_value(expr->text, strlen(expr->text), expr->at);
        case E_NAME: {
            Variable *v = find_variable(env, expr->text);
            if (!v && (builtin_named(expr->text) || !strcmp(expr->text, "log.f"))) return builtin_value(runtime, expr->text);
            if (!v) { error_at(expr->at, "variable '%s' not found", expr->text); return nothing(); }
            return retain(v->value);
        }
        case E_CALL: return call_function(runtime, env, expr);
        case E_TUPLE: {
            Value *items = resize(NULL, expr->argc * sizeof(Value));
            size_t count = 0;
            for (ExprList *item = expr->args; item && !has_error; item = item->next) items[count++] = evaluate(runtime, env, item->value);
            Value result = has_error ? nothing() : tuple_value(runtime, items, count);
            for (size_t i = 0; i < count; i++) release(items[i]);
            free(items); return result;
        }
        case E_LIST: {
            Value *items = resize(NULL, expr->argc * sizeof(Value));
            size_t count = 0;
            for (ExprList *item = expr->args; item && !has_error; item = item->next) items[count++] = evaluate(runtime, env, item->value);
            Value result = has_error ? nothing() : list_value(runtime, items, count);
            for (size_t i = 0; i < count; i++) release(items[i]);
            free(items); return result;
        }
        case E_DICT: {
            Value *keys = resize(NULL, expr->argc * sizeof(Value));
            Value *values = resize(NULL, expr->argc * sizeof(Value));
            size_t count = 0;
            for (ExprList *item = expr->args; item && !has_error; item = item->next) {
                keys[count] = evaluate(runtime, env, item->value);
                item = item->next;
                if (!item) { error_at(expr->at, "dictionary entry needs a value"); break; }
                values[count++] = evaluate(runtime, env, item->value);
            }
            Value result = has_error ? nothing() : dict_value(runtime, keys, values, count);
            for (size_t i = 0; i < count; i++) { release(keys[i]); release(values[i]); }
            free(keys); free(values); return result;
        }
        case E_MEMBER: {
            Value obj = evaluate(runtime, env, expr->left);
            if (has_error) return nothing();
            Value res = nothing();
            if (obj.type == V_DICT) {
                int found = 0;
                for (size_t i = 0; i < obj.as.dict->count; i++) {
                    if (obj.as.dict->items[i].key.type == V_STR && !strcmp(obj.as.dict->items[i].key.as.string->text, expr->text)) {
                        res = retain(obj.as.dict->items[i].value);
                        found = 1; break;
                    }
                }
                if (!found) error_at(expr->at, "dictionary key '%s' not found", expr->text);
            } else {
                error_at(expr->at, "member access requires a dictionary");
            }
            release(obj);
            return res;
        }
        case E_INDEX: {
            Value value = evaluate(runtime, env, expr->left);
            Value index = has_error ? nothing() : evaluate(runtime, env, expr->right);
            Value result = nothing();
            if (!has_error) {
                if (value.type == V_STR) {
                    /* UTF-8 character indexing */
                    if (index.type != V_INT) error_at(expr->at, "string index must be int");
                    else {
                        int64_t char_count = (int64_t)character_count(value.as.string->text, value.as.string->length);
                        int64_t i = index.as.integer;
                        if (i < 0) i += char_count;
                        if (i < 0 || i >= char_count) error_at(expr->at, "string index out of range");
                        else {
                            size_t byte_off = utf8_char_offset(value.as.string->text, value.as.string->length, i);
                            size_t char_bytes = utf8_char_bytes(value.as.string->text, byte_off);
                            result = text_value(value.as.string->text + byte_off, char_bytes, expr->at);
                        }
                    }
                } else if (value.type == V_DICT) {
                    for (size_t i = 0; i < value.as.dict->count; i++) {
                        Value equal = compare_values(OP_EQ, value.as.dict->items[i].key, index, expr->at);
                        int match = !has_error && equal.type == V_BOOL && equal.as.boolean;
                        release(equal);
                        if (match) { result = retain(value.as.dict->items[i].value); break; }
                    }
                    if (!has_error && result.type == V_VOID) error_at(expr->at, "dictionary key not found");
                } else if ((value.type != V_TUPLE && value.type != V_LIST) || index.type != V_INT) error_at(expr->at, "indexing requires a tuple or list and an int index");
                else {
                    int64_t i = index.as.integer, count = (int64_t)(value.type == V_TUPLE ? value.as.tuple->count : value.as.list->count);
                    if (i < 0) i += count;
                    if (i < 0 || i >= count) error_at(expr->at, "variadic argument index out of range");
                    else result = retain(value.type == V_TUPLE ? value.as.tuple->items[i] : value.as.list->items[i]);
                }
            }
            release(value); release(index); return result;
        }
        case E_FORMAT: {
            TextBuffer text = {.at = expr->at};
            for (ExprList *part = expr->args; part && !has_error; part = part->next) {
                Value value = evaluate(runtime, env, part->value);
                Value formatted = has_error ? nothing() : format_field(value, part->format, part->value->at);
                if (!has_error) append_text(&text, formatted.as.string->text, formatted.as.string->length);
                release(value); release(formatted);
            }
            Value result = has_error ? nothing() : text_value(text.text ? text.text : "", text.count, expr->at);
            free(text.text); return result;
        }
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
    if (target == V_ANY) return retain(v);
    if (v.type == target) return retain(v);
    int direct_input = expr->kind == E_CALL && expr->left->kind == E_NAME && !strcmp(expr->left->text, "input");
    if (direct_input || (target == V_FLOAT && v.type == V_INT)) return convert_value(v, target, expr->at);
    error_at(expr->at, "variable '%s' expects %s, got %s", name, type_label(target), type_label(v.type));
    return nothing();
}

/* Get the value at an expr path without raising an error on missing keys.
   Returns nothing() (V_VOID) if the path doesn't exist. */
static Value get_value_soft(Env *env, Expr *expr) {
    if (expr->kind == E_NAME) {
        Variable *v = find_variable(env, expr->text);
        return v ? retain(v->value) : nothing();
    }
    if (expr->kind == E_MEMBER) {
        Value parent = get_value_soft(env, expr->left);
        if (parent.type != V_DICT) { release(parent); return nothing(); }
        Value res = nothing();
        for (size_t i = 0; i < parent.as.dict->count; i++) {
            if (parent.as.dict->items[i].key.type == V_STR &&
                !strcmp(parent.as.dict->items[i].key.as.string->text, expr->text)) {
                res = retain(parent.as.dict->items[i].value); break;
            }
        }
        release(parent); return res;
    }
    return nothing();
}

static void set_member_value(Runtime *rt, Env *env, Expr *expr, Value val, Location at) {
    if (expr->kind == E_NAME) {
        Variable *v = find_variable(env, expr->text);
        if (v) { release(v->value); v->value = retain(val); }
        else error_at(at, "variable '%s' not found", expr->text);
    } else if (expr->kind == E_MEMBER) {
        /* Use soft-get so missing intermediate dicts don't error */
        Value obj = get_value_soft(env, expr->left);
        if (obj.type != V_DICT) {
            release(obj);
            /* Parent doesn't exist or isn't a dict — create a new empty dict and assign it */
            Value new_dict = dict_value(rt, NULL, NULL, 0);
            set_member_value(rt, env, expr->left, new_dict, at);
            release(new_dict);
            if (has_error) return;
            obj = get_value_soft(env, expr->left);
        }
        if (obj.type == V_DICT) {
            Dict *dict = obj.as.dict;
            int found = 0;
            for (size_t i = 0; i < dict->count; i++) {
                if (dict->items[i].key.type == V_STR && !strcmp(dict->items[i].key.as.string->text, expr->text)) {
                    release(dict->items[i].value);
                    dict->items[i].value = retain(val);
                    found = 1; break;
                }
            }
            if (!found) {
                if (dict->count == dict->capacity) {
                    dict->capacity = dict->capacity ? dict->capacity * 2 : 8;
                    dict->items = resize(dict->items, dict->capacity * sizeof(DictEntry));
                }
                dict->items[dict->count].key = text_value(expr->text, strlen(expr->text), at);
                dict->items[dict->count].value = retain(val);
                dict->count++;
            }
        } else {
            error_at(at, "member assignment requires a dictionary");
        }
        release(obj);
    } else if (expr->kind == E_INDEX) {
        Value obj = evaluate(rt, env, expr->left);
        Value key = has_error ? nothing() : evaluate(rt, env, expr->right);
        if (has_error) { release(obj); release(key); return; }
        if (obj.type == V_LIST) {
            if (key.type != V_INT) error_at(at, "list index must be int");
            else {
                int64_t idx = key.as.integer;
                if (idx < 0) idx += (int64_t)obj.as.list->count;
                if (idx < 0 || idx >= (int64_t)obj.as.list->count) error_at(at, "list index out of range");
                else { release(obj.as.list->items[idx]); obj.as.list->items[idx] = retain(val); }
            }
        } else if (obj.type == V_DICT) {
            Dict *dict = obj.as.dict;
            int found = 0;
            for (size_t i = 0; i < dict->count; i++) {
                Value eq = compare_values(OP_EQ, dict->items[i].key, key, at);
                if (!has_error && eq.as.boolean) {
                    release(dict->items[i].value); dict->items[i].value = retain(val); found = 1;
                }
                release(eq); if (found || has_error) break;
            }
            if (!found && !has_error) {
                if (dict->count == dict->capacity) {
                    dict->capacity = dict->capacity ? dict->capacity * 2 : 8;
                    dict->items = resize(dict->items, dict->capacity * sizeof(DictEntry));
                }
                dict->items[dict->count].key = retain(key); dict->items[dict->count].value = retain(val); dict->count++;
            }
        } else {
            error_at(at, "index assignment requires list or dict");
        }
        release(obj); release(key);
    } else {
        error_at(at, "invalid assignment target");
    }
}

static Flow execute_inner(Runtime *runtime, Env *env, Statement *statement) {
    for (Statement *s = statement; s && !has_error; s = s->next) {
        collect(runtime, 0);
        if (s->kind == S_FUNCTION) {
            Variable *previous = find_local(env, s->name);
            if (previous && previous->type != V_ANY && previous->type != V_FUNCTION) {
                error_at(s->at, "function '%s' conflicts with a typed variable", s->name); break;
            }
            Value function = create_function(runtime, env, s);
            if (!has_error) define_variable(env, s->name, V_ANY, function);
            release(function); continue;
        }
        if (s->kind == S_TRY) {
            Flow flow = execute(runtime, env, s->body);
            if (flow.kind == FLOW_ERROR) {
                Value message = text_value(error_message, strlen(error_message), s->at);
                has_error = 0; error_message[0] = '\0'; error_trace[0] = '\0';
                if (s->name) {
                    Variable *previous = find_local(env, s->name);
                    if (previous && previous->type != V_ANY && previous->type != V_STR)
                        error_at(s->at, "catch name '%s' conflicts with a typed variable", s->name);
                    else define_variable(env, s->name, V_ANY, message);
                }
                release(message); release(flow.value);
                flow = has_error ? (Flow){FLOW_ERROR, nothing()} : execute(runtime, env, s->otherwise);
            }
            if (flow.kind != FLOW_NORMAL) return flow;
            continue;
        }
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
            int broken = 0;
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
                if (flow.kind == FLOW_BREAK) { broken = 1; break; }
            }
            if (!has_error && !broken && s->otherwise) {
                Flow flow = execute(runtime, env, s->otherwise);
                if (flow.kind != FLOW_NORMAL) return flow;
            }
            continue;
        }
        if (s->kind == S_FOR) {
            Value iterable = evaluate(runtime, env, s->expr);
            size_t count = 0;
            if (!has_error && iterable.type == V_TUPLE) count = iterable.as.tuple->count;
            else if (!has_error && iterable.type == V_LIST) count = iterable.as.list->count;
            else if (!has_error && iterable.type == V_DICT) count = iterable.as.dict->count;
            else if (!has_error && iterable.type == V_STR) { /* UTF-8 character iteration */ }
            else if (!has_error) error_at(s->at, "for requires a tuple, list, dict, str, or range");
            int broken = 0;
            if (!has_error && iterable.type == V_STR) {
                /* Iterate over UTF-8 characters */
                const char *text = iterable.as.string->text;
                size_t bytes = iterable.as.string->length;
                size_t pos = 0;
                while (pos < bytes && !has_error) {
                    size_t char_bytes = utf8_char_bytes(text, pos);
                    Value item = text_value(text + pos, char_bytes, s->at);
                    pos += char_bytes;
                    Variable *variable = find_local(env, s->name);
                    if (variable) { release(variable->value); variable->value = retain(item); }
                    else define_variable(env, s->name, V_ANY, item);
                    release(item);
                    if (has_error) break;
                    Flow flow = execute(runtime, env, s->body);
                    if (flow.kind == FLOW_RETURN || flow.kind == FLOW_ERROR) { release(iterable); return flow; }
                    if (flow.kind == FLOW_BREAK) { broken = 1; break; }
                }
            } else {
                for (size_t i = 0; i < count && !has_error; i++) {
                    Value item = iterable.type == V_TUPLE ? retain(iterable.as.tuple->items[i]) : iterable.type == V_LIST ? retain(iterable.as.list->items[i]) : retain(iterable.as.dict->items[i].key);
                    Variable *variable = find_local(env, s->name);
                    if (variable) { release(variable->value); variable->value = retain(item); }
                    else define_variable(env, s->name, V_ANY, item);
                    release(item);
                    if (has_error) break;
                    Flow flow = execute(runtime, env, s->body);
                    if (flow.kind == FLOW_RETURN || flow.kind == FLOW_ERROR) { release(iterable); return flow; }
                    if (flow.kind == FLOW_BREAK) { broken = 1; break; }
                }
            }
            release(iterable);
            if (!has_error && !broken && s->otherwise) {
                Flow flow = execute(runtime, env, s->otherwise);
                if (flow.kind != FLOW_NORMAL) return flow;
            }
            continue;
        }
        /* ── Fjson blocks ── */
        if (s->kind == S_FJSON) {
            Value filename = evaluate(runtime, env, s->expr);
            if (has_error || filename.type != V_STR) { error_at(s->at, "Fjson filename must be str"); release(filename); break; }
            const char *path = filename.as.string->text;
            FILE *fp = fopen(path, "rb");
            Value j_val;
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long len = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                size_t flen = (len > 0) ? (size_t)len : 0;
                char *buf = malloc(flen + 1);
                fread(buf, 1, flen, fp);
                buf[flen] = '\0';
                fclose(fp);
                const char *p = buf;
                j_val = parse_json_val(runtime, &p);
                free(buf);
                if (j_val.type == V_VOID) j_val = dict_value(runtime, NULL, NULL, 0);
            } else {
                j_val = dict_value(runtime, NULL, NULL, 0);
            }
            Env *local = new_env(runtime, env);
            define_variable(local, "j", V_ANY, j_val);
            release(j_val);
            Flow flow = execute(runtime, local, s->body);
            if (flow.kind != FLOW_ERROR && !has_error) {
                Variable *j_var = find_local(local, "j");
                if (j_var) {
                    char temp_path[1024];
                    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
                    FILE *out = fopen(temp_path, "wb");
                    if (out) {
                        stringify_json(out, j_var->value, 0);
                        fclose(out);
                        remove(path);
                        rename(temp_path, path);
                    } else {
                        error_at(s->at, "failed to save Fjson file");
                    }
                }
            }
            local->object.refs--;
            release(filename);
            if (flow.kind != FLOW_NORMAL) return flow;
            continue;
        }
        if (s->kind == S_FJSON_REPLACE) {
            Value rhs = evaluate(runtime, env, s->expr2);
            if (has_error) break;
            Value final_val;
            if (s->type != V_VOID) {
                /* compound assign: soft-get current value (0 if missing), apply op */
                Operator op = (Operator)(s->type - 1);
                Value cur = get_value_soft(env, s->expr);
                if (cur.type == V_VOID) cur = integer_value(0); /* default to 0 */
                final_val = arithmetic(op, cur, rhs, s->at);
                release(cur);
            } else {
                final_val = retain(rhs);
            }
            release(rhs);
            if (!has_error) set_member_value(runtime, env, s->expr, final_val, s->at);
            release(final_val);
            if (has_error) break;
            continue;
        }
        if (s->kind == S_FJSON_ADD) {
            Value val = evaluate(runtime, env, s->expr2);
            if (has_error) break;
            Value target = get_value_soft(env, s->expr);
            if (target.type == V_LIST) {
                /* append to existing list */
                List *list = target.as.list;
                if (list->count == list->capacity) {
                    list->capacity = list->capacity ? list->capacity * 2 : 8;
                    list->items = resize(list->items, list->capacity * sizeof(Value));
                }
                list->items[list->count++] = retain(val);
                release(target);
            } else {
                /* target doesn't exist or isn't a list: create/set as new key */
                release(target);
                set_member_value(runtime, env, s->expr, val, s->at);
            }
            release(val);
            if (has_error) break;
            continue;
        }
        if (s->kind == S_FJSON_DELETE) {
            if (s->expr->kind == E_MEMBER) {
                Value obj = get_value_soft(env, s->expr->left);
                if (obj.type == V_DICT) {
                    Dict *dict = obj.as.dict;
                    int found = -1;
                    for (size_t i = 0; i < dict->count; i++) {
                        if (dict->items[i].key.type == V_STR && !strcmp(dict->items[i].key.as.string->text, s->expr->text)) { found = (int)i; break; }
                    }
                    if (found >= 0) {
                        release(dict->items[found].key); release(dict->items[found].value);
                        for (size_t i = (size_t)found; i < dict->count - 1; i++) dict->items[i] = dict->items[i + 1];
                        dict->count--;
                    }
                }
                release(obj);
            } else if (s->expr->kind == E_INDEX) {
                Value obj = get_value_soft(env, s->expr->left);
                Value key = evaluate(runtime, env, s->expr->right);
                if (!has_error && obj.type == V_DICT) {
                    Dict *dict = obj.as.dict;
                    int found = -1;
                    for (size_t i = 0; i < dict->count; i++) {
                        Value eq = compare_values(OP_EQ, dict->items[i].key, key, s->at);
                        if (!has_error && eq.as.boolean) found = (int)i;
                        release(eq); if (found >= 0 || has_error) break;
                    }
                    if (found >= 0) {
                        release(dict->items[found].key); release(dict->items[found].value);
                        for (size_t i = (size_t)found; i < dict->count - 1; i++) dict->items[i] = dict->items[i + 1];
                        dict->count--;
                    }
                } else if (!has_error && obj.type == V_LIST) {
                    if (key.type == V_INT) {
                        int64_t idx = key.as.integer;
                        if (idx < 0) idx += (int64_t)obj.as.list->count;
                        if (idx >= 0 && idx < (int64_t)obj.as.list->count) {
                            release(obj.as.list->items[idx]);
                            for (size_t i = (size_t)idx; i < obj.as.list->count - 1; i++) obj.as.list->items[i] = obj.as.list->items[i + 1];
                            obj.as.list->count--;
                        }
                    }
                }
                release(obj); release(key);
            }
            if (has_error) break;
            continue;
        }
        /* ── Index assignment: collection[key] = value ── */
        if (s->kind == S_INDEX_ASSIGN) {
            Value val = evaluate(runtime, env, s->expr2);
            if (!has_error) set_member_value(runtime, env, s->expr, val, s->at);
            release(val);
            if (has_error) break;
            continue;
        }
        /* ── Variable resolve for S_ASSIGN ── */
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
        if (s->kind == S_DECLARE || s->kind == S_ASSIGN) {
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
    runtime.global = new_env(&runtime, NULL);
    Flow flow = execute(&runtime, runtime.global, program);
    release(flow.value);
    runtime.global->object.refs--;
    collect(&runtime, 1);
    if (live_strings || runtime.object_count) {
        fprintf(stderr, "[Internal error] %zu strings, %zu objects remain\n", live_strings, runtime.object_count);
        has_error = 1;
    }
}
#endif

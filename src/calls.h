#ifndef SIAN_CALLS_H
#define SIAN_CALLS_H
typedef struct { Value *values; const char **names; size_t count, capacity; } Arguments;
static void add_argument(Arguments *args, Value value, const char *name, Location at) {
    if (args->count == ARG_LIMIT) { error_at(at, "too many function arguments (maximum 256 after expansion)"); return; }
    if (args->count == args->capacity) {
        args->capacity = args->capacity ? args->capacity * 2 : 8;
        args->values = resize(args->values, args->capacity * sizeof(Value));
        args->names = resize(args->names, args->capacity * sizeof(char *));
    }
    args->values[args->count] = retain(value); args->names[args->count++] = name;
}
static void free_arguments(Arguments *args) {
    for (size_t i = 0; i < args->count; i++) release(args->values[i]);
    free(args->values); free(args->names);
}
static Value builtin_value(Runtime *rt, const char *name) {
    static const char *names[] = {"log", "log.f", "input", "len", "int", "float", "str", "bool", "TF"};
    Closure *fn = new_object(rt, sizeof(*fn), G_FUNCTION);
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++) if (!strcmp(name, names[i])) { fn->builtin = names[i]; break; }
    Value result = {.type = V_FUNCTION}; result.as.function = fn; return result;
}
static Value print_arguments(Arguments *args, Location at) {
    const char *sep = " ", *end = "\n"; int flush = 0;
    for (size_t i = 0; i < args->count && !has_error; i++) {
        const char *name = args->names[i]; Value value = args->values[i];
        if (!name) continue;
        if (!strcmp(name, "sep") || !strcmp(name, "end")) {
            if (value.type != V_STR && value.type != V_VOID) error_at(at, "%s must be str or None", name);
            else if (!strcmp(name, "sep")) sep = value.type == V_VOID ? " " : value.as.string->text;
            else end = value.type == V_VOID ? "\n" : value.as.string->text;
        } else if (!strcmp(name, "flush")) flush = truth_value(value, at);
        else if (!strcmp(name, "file")) {
            if (value.type != V_VOID) error_at(at, "log supports console output only; file must be None");
        } else error_at(at, "log got unexpected named argument '%s'", name);
    }
    int first = 1;
    for (size_t i = 0; i < args->count && !has_error; i++) if (!args->names[i]) {
        Value text = display_value(args->values[i], 0, at);
        if (!has_error) {
            if (!first && fputs(sep, stdout) == EOF) error_at(at, "console write failed");
            if (!has_error && fputs(text.as.string->text, stdout) == EOF) error_at(at, "console write failed");
            first = 0;
        }
        release(text);
    }
    if (!has_error && fputs(end, stdout) == EOF) error_at(at, "console write failed");
    if (!has_error && flush && fflush(stdout) == EOF) error_at(at, "console flush failed");
    return nothing();
}
static Value call_builtin(const char *name, Arguments *args, Location at) {
    if (!strcmp(name, "log") || !strcmp(name, "log.f")) return print_arguments(args, at);
    for (size_t i = 0; i < args->count; i++) if (args->names[i]) {
        error_at(at, "'%s' does not accept named arguments", name); return nothing();
    }
    if (!strcmp(name, "input")) return input_value(args->values, args->count, at);
    if (args->count != 1) { error_at(at, "wrong number of arguments to '%s' (expected 1)", name); return nothing(); }
    if (!strcmp(name, "len")) {
        Value value = args->values[0];
        if (value.type == V_TUPLE) return integer_value((int64_t)value.as.tuple->count);
        if (value.type == V_STR) return integer_value((int64_t)character_count(value.as.string->text, value.as.string->length));
        error_at(at, "len requires variadic arguments or str"); return nothing();
    }
    return convert_value(args->values[0], type_named(name), at);
}
static Value create_function(Runtime *rt, Env *env, Statement *s) {
    Closure *fn = new_object(rt, sizeof(*fn), G_FUNCTION);
    fn->definition = s; fn->environment = env; env->object.refs++;
    fn->defaults = resize(NULL, s->parameter_count * sizeof(Value));
    for (size_t i = 0; i < s->parameter_count; i++) fn->defaults[i] = nothing();
    size_t i = 0;
    for (NameList *param = s->params; param && !has_error; param = param->next, i++)
        if (param->default_value) fn->defaults[i] = evaluate(rt, env, param->default_value);
    Value result = {.type = V_FUNCTION}; result.as.function = fn; return result;
}
static Value invoke_function(Runtime *rt, Closure *fn, Arguments *args, Location at) {
    if (fn->builtin) return call_builtin(fn->builtin, args, at);
    Statement *definition = fn->definition;
    size_t count = definition->parameter_count, fixed = count;
    NameList **params = resize(NULL, count * sizeof(NameList *));
    size_t index = 0;
    for (NameList *p = definition->params; p; p = p->next) params[index++] = p;
    int variadic = count && params[count - 1]->rest;
    if (variadic) fixed--;
    Value *bound = resize(NULL, count * sizeof(Value));
    unsigned char *present = resize(NULL, count);
    memset(present, 0, count);
    for (size_t i = 0; i < count; i++) bound[i] = nothing();
    Arguments excess = {0}; size_t position = 0;
    /* All positional arguments (including #expansion) bind before keywords. */
    for (size_t i = 0; i < args->count && !has_error; i++) if (!args->names[i]) {
        if (position < fixed) { bound[position] = retain(args->values[i]); present[position++] = 1; }
        else if (variadic) add_argument(&excess, args->values[i], NULL, at);
        else error_at(at, "function '%s' expects at most %zu positional arguments", definition->name, fixed);
    }
    for (size_t i = 0; i < args->count && !has_error; i++) if (args->names[i]) {
        size_t p = 0;
        while (p < fixed && strcmp(params[p]->name, args->names[i])) p++;
        if (p == fixed) error_at(at, "function '%s' got unexpected named argument '%s'", definition->name, args->names[i]);
        else if (present[p]) error_at(at, "multiple values for argument '%s'", params[p]->name);
        else { bound[p] = retain(args->values[i]); present[p] = 1; }
    }
    for (size_t i = 0; i < fixed && !has_error; i++) if (!present[i]) {
        if (params[i]->default_value) bound[i] = retain(fn->defaults[i]);
        else error_at(at, "function '%s' missing required argument '%s'", definition->name, params[i]->name);
    }
    if (variadic) bound[count - 1] = tuple_value(rt, excess.values, excess.count);
    Value result = nothing();
    if (!has_error) {
        if (rt->call_depth >= DEPTH_LIMIT) error_at(at, "call depth exceeds %u", DEPTH_LIMIT);
        else {
            Env *local = new_env(rt, fn->environment);
            for (size_t i = 0; i < count; i++) define_variable(local, params[i]->name, V_ANY, bound[i]);
            rt->call_depth++;
            Flow flow = execute(rt, local, definition->body);
            rt->call_depth--;
            if (flow.kind == FLOW_RETURN && !has_error) result = flow.value;
            else release(flow.value);
            local->object.refs--;
        }
    }
    if (has_error) trace_error(definition->name, at);
    for (size_t i = 0; i < count; i++) release(bound[i]);
    free(bound); free(present); free(params); free_arguments(&excess);
    return result;
}
static Expr *runtime_template(Arena *arena, Value value, Location at) {
    Parser parser = {0}; parser.arena = arena;
    Expr literal = {.kind = E_STRING, .at = at, .text = value.as.string->text, .height = 1};
    if (setjmp(parser.failure)) return NULL;
    return parse_template(&parser, &literal);
}
static Value dynamic_format(Runtime *rt, Env *env, Value value, Location at) {
    Arena arena = {0};
    unsigned int saved_depth = format_parse_depth;
    Expr *template = runtime_template(&arena, value, at);
    Value result = has_error ? nothing() : evaluate(rt, env, template);
    format_parse_depth = saved_depth;
    arena_free(&arena);
    return result;
}
static Value call_function(Runtime *rt, Env *env, Expr *expr) {
    Value callable = evaluate(rt, env, expr->left);
    if (!has_error && callable.type != V_FUNCTION) error_at(expr->at, "%s value is not callable", type_label(callable.type));
    Arguments args = {0};
    int formatting = !has_error && callable.as.function->builtin && !strcmp(callable.as.function->builtin, "log.f");
    for (ExprList *item = expr->args; item && !has_error; item = item->next) {
        Value value = evaluate(rt, env, item->value);
        if (!has_error && formatting && !item->name && !item->spread && value.type == V_STR && item->value->kind != E_FORMAT) {
            Value rendered = dynamic_format(rt, env, value, item->value->at);
            release(value); value = rendered;
        }
        if (!has_error) {
            if (!item->spread) add_argument(&args, value, item->name, item->value->at);
            else if (value.type != V_TUPLE) error_at(item->value->at, "#expansion requires variadic arguments");
            else for (size_t i = 0; i < value.as.tuple->count && !has_error; i++) {
                Value element = retain(value.as.tuple->items[i]);
                if (formatting && element.type == V_STR) {
                    Value rendered = dynamic_format(rt, env, element, item->value->at);
                    release(element); element = rendered;
                }
                if (!has_error) add_argument(&args, element, NULL, item->value->at);
                release(element);
            }
        }
        release(value);
    }
    Value result = has_error ? nothing() : invoke_function(rt, callable.as.function, &args, expr->at);
    release(callable); free_arguments(&args);
    return result;
}
#endif

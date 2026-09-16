#ifndef SIAN_VALUES_H
#define SIAN_VALUES_H

typedef struct { size_t refs, length; char text[]; } String;
typedef enum { G_ENV, G_FUNCTION, G_TUPLE, G_LIST, G_DICT, G_FILE } ObjectKind;
typedef struct Object { ObjectKind kind; size_t refs, trial; int marked; struct Object *next; } Object;
typedef struct Env Env;
typedef struct Closure Closure;
typedef struct Tuple Tuple;
typedef struct List List;
typedef struct Dict Dict;
typedef struct FileObj FileObj;
typedef struct {
    ValueType type;
    union { int64_t integer; double decimal; String *string; int boolean;
        Closure *function; Tuple *tuple; List *list; Dict *dict; FileObj *file; } as;
} Value;
struct FileObj { Object object; FILE *fp; int closed; };
typedef struct { const char *name; ValueType type; Value value; } Variable;
struct Env { Object object; Variable *vars; size_t count, capacity; Env *parent; };
struct Closure { Object object; Statement *definition; Env *environment; Value *defaults;
    const char *builtin; Value receiver; };
struct Tuple { Object object; size_t count; Value *items; };
struct List { Object object; size_t count, capacity; Value *items; };
typedef struct { Value key, value; } DictEntry;
struct Dict { Object object; size_t count, capacity; DictEntry *items; };
typedef enum { FLOW_NORMAL, FLOW_RETURN, FLOW_BREAK, FLOW_CONTINUE, FLOW_ERROR } FlowKind;
typedef struct { FlowKind kind; Value value; } Flow;
typedef struct {
    Statement *program;
    Env *global;
    Object *objects;
    size_t allocations, object_count;
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
    memcpy(s->text, text, length); s->text[length] = '\0'; live_strings++;
    Value v = {.type = V_STR}; v.as.string = s; return v;
}
static Object *value_object(Value v) {
    if (v.type == V_FUNCTION) return &v.as.function->object;
    if (v.type == V_TUPLE) return &v.as.tuple->object;
    if (v.type == V_LIST) return &v.as.list->object;
    if (v.type == V_DICT) return &v.as.dict->object;
    if (v.type == V_FILE) return &v.as.file->object;
    return NULL;
}
static Value retain(Value v) {
    if (v.type == V_STR) v.as.string->refs++;
    Object *o = value_object(v); if (o) o->refs++;
    return v;
}
static void release(Value v) {
    if (v.type == V_STR && --v.as.string->refs == 0) { free(v.as.string); live_strings--; }
    Object *o = value_object(v); if (o) o->refs--;
}
static void *new_object(Runtime *rt, size_t size, ObjectKind kind) {
    Object *o = resize(NULL, size); memset(o, 0, size);
    o->kind = kind; o->refs = 1; o->next = rt->objects; rt->objects = o;
    rt->allocations++; rt->object_count++;
    return o;
}
static Env *new_env(Runtime *rt, Env *parent) {
    Env *env = new_object(rt, sizeof(*env), G_ENV); env->parent = parent;
    if (parent) parent->object.refs++;
    return env;
}
static Value tuple_value(Runtime *rt, Value *values, size_t count) {
    Tuple *tuple = new_object(rt, sizeof(*tuple), G_TUPLE);
    tuple->count = count; tuple->items = resize(NULL, count * sizeof(Value));
    for (size_t i = 0; i < count; i++) tuple->items[i] = retain(values[i]);
    Value value = {.type = V_TUPLE}; value.as.tuple = tuple; return value;
}
static Value list_value(Runtime *rt, Value *values, size_t count) {
    List *list = new_object(rt, sizeof(*list), G_LIST);
    list->count = count; list->capacity = count; list->items = resize(NULL, count * sizeof(Value));
    for (size_t i = 0; i < count; i++) list->items[i] = retain(values[i]);
    Value value = {.type = V_LIST}; value.as.list = list; return value;
}
static Value dict_value(Runtime *rt, Value *keys, Value *values, size_t count) {
    Dict *dict = new_object(rt, sizeof(*dict), G_DICT);
    dict->count = count; dict->capacity = count; dict->items = resize(NULL, count * sizeof(DictEntry));
    for (size_t i = 0; i < count; i++) { dict->items[i].key = retain(keys[i]); dict->items[i].value = retain(values[i]); }
    Value value = {.type = V_DICT}; value.as.dict = dict; return value;
}
static const char *type_label(ValueType type) {
    static const char *labels[] = {"None", "int", "float", "str", "bool", "function", "tuple", "list", "dict", "var"};
    return labels[type];
}
static int numeric(Value v) { return v.type == V_INT || v.type == V_FLOAT; }
static double number(Value v) { return v.type == V_INT ? (double)v.as.integer : v.as.decimal; }
static int truth_value(Value v, Location at) {
    (void)at;
    switch (v.type) {
        case V_INT: return v.as.integer != 0;
        case V_FLOAT: return v.as.decimal != 0;
        case V_STR: return v.as.string->length != 0;
        case V_BOOL: return v.as.boolean;
        case V_VOID: return 0;
        case V_TUPLE: return v.as.tuple->count != 0;
        case V_LIST: return v.as.list->count != 0;
        case V_DICT: return v.as.dict->count != 0;
        default: return 1;
    }
}
static Variable *find_local(Env *env, const char *name) {
    for (size_t i = 0; i < env->count; i++) if (!strcmp(env->vars[i].name, name)) return &env->vars[i];
    return NULL;
}
static Variable *find_variable(Env *env, const char *name) {
    for (; env; env = env->parent) {
        Variable *v = find_local(env, name); if (v) return v;
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
        v = &env->vars[env->count++]; *v = (Variable){name, type, nothing()};
    }
    Value copy = retain(value); release(v->value); v->value = copy;
}

typedef void (*VisitObject)(Object *, void *);
static void visit_value(Value value, VisitObject visit, void *context) {
    Object *object = value_object(value); if (object) visit(object, context);
}
static void object_edges(Object *o, VisitObject visit, void *context) {
    if (o->kind == G_ENV) {
        Env *env = (Env *)o;
        if (env->parent) visit(&env->parent->object, context);
        for (size_t i = 0; i < env->count; i++) visit_value(env->vars[i].value, visit, context);
    } else if (o->kind == G_FUNCTION) {
        Closure *fn = (Closure *)o;
        if (fn->environment) visit(&fn->environment->object, context);
        if (fn->definition) for (size_t i = 0; i < fn->definition->parameter_count; i++) visit_value(fn->defaults[i], visit, context);
        visit_value(fn->receiver, visit, context);
    } else if (o->kind == G_TUPLE) {
        Tuple *tuple = (Tuple *)o;
        for (size_t i = 0; i < tuple->count; i++) visit_value(tuple->items[i], visit, context);
    } else if (o->kind == G_LIST) {
        List *list = (List *)o;
        for (size_t i = 0; i < list->count; i++) visit_value(list->items[i], visit, context);
    } else if (o->kind == G_DICT) {
        Dict *dict = (Dict *)o;
        for (size_t i = 0; i < dict->count; i++) {
            visit_value(dict->items[i].key, visit, context);
            visit_value(dict->items[i].value, visit, context);
        }
    }
}
static void subtract_edge(Object *o, void *context) { (void)context; o->trial--; }
typedef struct { Object **items; size_t count; } MarkQueue;
static void mark_edge(Object *o, void *context) {
    MarkQueue *queue = context;
    if (!o->marked) { o->marked = 1; queue->items[queue->count++] = o; }
}
static void release_live_edge(Object *o, void *context) { (void)context; if (o->marked) o->refs--; }
static void release_string(Value v) { if (v.type == V_STR) release(v); }
static void collect(Runtime *rt, int force) {
    if (!force && rt->allocations < 256) return;
    /* refs - heap-internal edges leaves roots owned by C evaluation frames.
       Reachability from these roots preserves escaped closures and collects cycles. */
    for (Object *o = rt->objects; o; o = o->next) { o->trial = o->refs; o->marked = 0; }
    for (Object *o = rt->objects; o; o = o->next) object_edges(o, subtract_edge, NULL);
    MarkQueue queue = {resize(NULL, rt->object_count * sizeof(Object *)), 0};
    for (Object *o = rt->objects; o; o = o->next) if (o->trial) mark_edge(o, &queue);
    for (size_t i = 0; i < queue.count; i++) object_edges(queue.items[i], mark_edge, &queue);
    free(queue.items);
    for (Object *o = rt->objects; o; o = o->next) if (!o->marked) object_edges(o, release_live_edge, NULL);
    Object **cursor = &rt->objects;
    while (*cursor) {
        Object *o = *cursor;
        if (o->marked) { cursor = &o->next; continue; }
        *cursor = o->next;
        if (o->kind == G_ENV) {
            Env *env = (Env *)o;
            for (size_t i = 0; i < env->count; i++) release_string(env->vars[i].value);
            free(env->vars);
        } else if (o->kind == G_FUNCTION) {
            Closure *fn = (Closure *)o;
            if (fn->definition) for (size_t i = 0; i < fn->definition->parameter_count; i++) release_string(fn->defaults[i]);
            release_string(fn->receiver); free(fn->defaults);
        } else if (o->kind == G_TUPLE) {
            Tuple *tuple = (Tuple *)o;
            for (size_t i = 0; i < tuple->count; i++) release_string(tuple->items[i]);
            free(tuple->items);
        } else if (o->kind == G_LIST) {
            List *list = (List *)o;
            for (size_t i = 0; i < list->count; i++) release_string(list->items[i]);
            free(list->items);
        } else if (o->kind == G_DICT) {
            Dict *dict = (Dict *)o;
            for (size_t i = 0; i < dict->count; i++) {
                release_string(dict->items[i].key);
                release_string(dict->items[i].value);
            }
            free(dict->items);
        } else if (o->kind == G_FILE) {
            FileObj *f = (FileObj *)o;
            if (f->fp && !f->closed) { fclose(f->fp); f->closed = 1; }
        }
        free(o); rt->object_count--;
    }
    rt->allocations = 0;
}
#endif

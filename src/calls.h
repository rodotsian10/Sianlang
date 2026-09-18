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
    static const char *names[] = {"log", "log.f", "input", "len", "range", "time.now",
        "int", "float", "str", "bool", "TF",
        "abs", "min", "max", "round",
        "random.int", "random.float", "random.choice", "open", "game.start", "game.close", "game.delta_time", "scene.change", "key.down", "rodot.create", "rodot.load", "draw.rect", "draw.circle", "draw.line", "draw.text", "collision"};
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

/* ── UTF-8 helper: get byte offset of the i-th character ── */
static size_t utf8_char_offset(const char *text, size_t bytes, int64_t char_index) {
    size_t pos = 0, char_num = 0;
    while (pos < bytes) {
        if (((unsigned char)text[pos] & 0xc0) != 0x80) {
            if ((int64_t)char_num == char_index) return pos;
            char_num++;
        }
        pos++;
    }
    return pos; /* end of string */
}
static size_t utf8_char_bytes(const char *text, size_t start) {
    unsigned char ch = (unsigned char)text[start];
    if (ch < 0x80) return 1;
    if (ch < 0xE0) return 2;
    if (ch < 0xF0) return 3;
    return 4;
}

/* ── Method dispatch for list / dict / str / file ── */
static Value call_method(Runtime *rt, Value receiver, const char *method, Arguments *args, Location at) {
    /* list.append(value) */
    if (receiver.type == V_LIST && !strcmp(method, "append")) {
        if (args->count != 1 || args->names[0]) { error_at(at, "append expects exactly 1 positional argument"); return nothing(); }
        List *list = receiver.as.list;
        if (list->count == list->capacity) {
            list->capacity = list->capacity ? list->capacity * 2 : 8;
            list->items = resize(list->items, list->capacity * sizeof(Value));
        }
        list->items[list->count++] = retain(args->values[0]);
        return nothing();
    }
    /* list.pop() */
    if (receiver.type == V_LIST && !strcmp(method, "pop")) {
        if (args->count != 0) { error_at(at, "pop expects no arguments"); return nothing(); }
        List *list = receiver.as.list;
        if (list->count == 0) { error_at(at, "pop from empty list"); return nothing(); }
        Value last = list->items[--list->count]; /* transfer ownership to caller */
        return last;
    }
    /* dict.keys() */
    if (receiver.type == V_DICT && !strcmp(method, "keys")) {
        if (args->count != 0) { error_at(at, "keys expects no arguments"); return nothing(); }
        Dict *dict = receiver.as.dict;
        Value *items = resize(NULL, dict->count * sizeof(Value));
        for (size_t i = 0; i < dict->count; i++) items[i] = retain(dict->items[i].key);
        Value result = list_value(rt, items, dict->count);
        for (size_t i = 0; i < dict->count; i++) release(items[i]);
        free(items); return result;
    }
    /* dict.values() */
    if (receiver.type == V_DICT && !strcmp(method, "values")) {
        if (args->count != 0) { error_at(at, "values expects no arguments"); return nothing(); }
        Dict *dict = receiver.as.dict;
        Value *items = resize(NULL, dict->count * sizeof(Value));
        for (size_t i = 0; i < dict->count; i++) items[i] = retain(dict->items[i].value);
        Value result = list_value(rt, items, dict->count);
        for (size_t i = 0; i < dict->count; i++) release(items[i]);
        free(items); return result;
    }
    /* dict.items() — returns list of (key, value) tuples */
    if (receiver.type == V_DICT && !strcmp(method, "items")) {
        if (args->count != 0) { error_at(at, "items expects no arguments"); return nothing(); }
        Dict *dict = receiver.as.dict;
        Value *pairs = resize(NULL, dict->count * sizeof(Value));
        for (size_t i = 0; i < dict->count && !has_error; i++) {
            Value pair[2] = { retain(dict->items[i].key), retain(dict->items[i].value) };
            pairs[i] = tuple_value(rt, pair, 2);
            release(pair[0]); release(pair[1]);
        }
        Value result = has_error ? nothing() : list_value(rt, pairs, dict->count);
        for (size_t i = 0; i < dict->count; i++) release(pairs[i]);
        free(pairs); return result;
    }
    /* file.read() */
    if (receiver.type == V_FILE && !strcmp(method, "read")) {
        if (args->count != 0) { error_at(at, "read expects no arguments"); return nothing(); }
        FileObj *f = receiver.as.file;
        if (f->closed || !f->fp) { error_at(at, "I/O operation on closed file"); return nothing(); }
        size_t count = 0, capacity = 4096;
        char *buf = resize(NULL, capacity);
        int ch;
        while ((ch = fgetc(f->fp)) != EOF) {
            if (count == TEXT_LIMIT) { free(buf); error_at(at, "file content exceeds 16 MiB limit"); return nothing(); }
            if (count + 1 >= capacity) { capacity *= 2; buf = resize(buf, capacity); }
            buf[count++] = (char)ch;
        }
        if (ferror(f->fp)) { free(buf); error_at(at, "file read error"); return nothing(); }
        Value result = text_value(buf, count, at);
        free(buf); return result;
    }
    /* file.readline() */
    if (receiver.type == V_FILE && !strcmp(method, "readline")) {
        if (args->count != 0) { error_at(at, "readline expects no arguments"); return nothing(); }
        FileObj *f = receiver.as.file;
        if (f->closed || !f->fp) { error_at(at, "I/O operation on closed file"); return nothing(); }
        size_t count = 0, capacity = 256;
        char *buf = resize(NULL, capacity);
        int ch;
        while ((ch = fgetc(f->fp)) != EOF && ch != '\n') {
            if (count == TEXT_LIMIT) { free(buf); error_at(at, "line exceeds 16 MiB limit"); return nothing(); }
            if (count + 1 >= capacity) { capacity *= 2; buf = resize(buf, capacity); }
            buf[count++] = (char)ch;
        }
        if (ch == '\n') { if (count + 1 >= capacity) buf = resize(buf, count + 2); buf[count++] = '\n'; }
        if (ferror(f->fp)) { free(buf); error_at(at, "file read error"); return nothing(); }
        Value result = text_value(buf, count, at);
        free(buf); return result;
    }
    /* file.write(str) */
    if (receiver.type == V_FILE && !strcmp(method, "write")) {
        if (args->count != 1 || args->names[0]) { error_at(at, "write expects exactly 1 positional str argument"); return nothing(); }
        if (args->values[0].type != V_STR) { error_at(at, "write argument must be str"); return nothing(); }
        FileObj *f = receiver.as.file;
        if (f->closed || !f->fp) { error_at(at, "I/O operation on closed file"); return nothing(); }
        String *s = args->values[0].as.string;
        if (fwrite(s->text, 1, s->length, f->fp) != s->length) { error_at(at, "file write error"); return nothing(); }
        return nothing();
    }
    /* file.close() */
    if (receiver.type == V_FILE && !strcmp(method, "close")) {
        if (args->count != 0) { error_at(at, "close expects no arguments"); return nothing(); }
        FileObj *f = receiver.as.file;
        if (!f->closed && f->fp) { fclose(f->fp); f->fp = NULL; f->closed = 1; }
        return nothing();
    }
    /* str.split([sep]) */
    if (receiver.type == V_STR && !strcmp(method, "split")) {
        if (args->count > 1 || (args->count == 1 && args->names[0])) { error_at(at, "split expects 0 or 1 positional argument"); return nothing(); }
        const char *text = receiver.as.string->text;
        size_t len = receiver.as.string->length;
        if (args->count == 1) {
            if (args->values[0].type != V_STR) { error_at(at, "split separator must be str"); return nothing(); }
            const char *sep = args->values[0].as.string->text;
            size_t sep_len = args->values[0].as.string->length;
            if (sep_len == 0) { error_at(at, "empty separator"); return nothing(); }
            size_t count = 0, capacity = 8;
            Value *items = malloc(capacity * sizeof(Value));
            const char *scan = text;
            while (scan < text + len) {
                const char *match = strstr(scan, sep);
                if (!match) match = text + len;
                if (count == capacity) { capacity *= 2; items = realloc(items, capacity * sizeof(Value)); }
                items[count++] = text_value(scan, (size_t)(match - scan), at);
                scan = match + sep_len;
            }
            if (scan == text + len && (len >= sep_len && memcmp(text + len - sep_len, sep, sep_len) == 0)) {
                if (count == capacity) { capacity *= 2; items = realloc(items, capacity * sizeof(Value)); }
                items[count++] = text_value("", 0, at);
            }
            Value res = list_value(rt, items, count);
            for(size_t i=0; i<count; i++) release(items[i]);
            free(items); return res;
        } else {
            size_t count = 0, capacity = 8;
            Value *items = malloc(capacity * sizeof(Value));
            const char *scan = text;
            while (scan < text + len) {
                while (scan < text + len && (*scan == ' ' || *scan == '\t' || *scan == '\n' || *scan == '\r')) scan++;
                if (scan == text + len) break;
                const char *start = scan;
                while (scan < text + len && !(*scan == ' ' || *scan == '\t' || *scan == '\n' || *scan == '\r')) scan++;
                if (count == capacity) { capacity *= 2; items = realloc(items, capacity * sizeof(Value)); }
                items[count++] = text_value(start, (size_t)(scan - start), at);
            }
            Value res = list_value(rt, items, count);
            for(size_t i=0; i<count; i++) release(items[i]);
            free(items); return res;
        }
    }
    /* str.join(list) */
    if (receiver.type == V_STR && !strcmp(method, "join")) {
        if (args->count != 1 || args->values[0].type != V_LIST) { error_at(at, "join expects 1 list argument"); return nothing(); }
        List *list = args->values[0].as.list;
        const char *sep = receiver.as.string->text;
        size_t sep_len = receiver.as.string->length;
        size_t total = 0;
        for (size_t i = 0; i < list->count; i++) {
            if (list->items[i].type != V_STR) { error_at(at, "join expects a list of str"); return nothing(); }
            total += list->items[i].as.string->length;
            if (i < list->count - 1) total += sep_len;
        }
        if (total > TEXT_LIMIT) { error_at(at, "string exceeds limit"); return nothing(); }
        char *buf = malloc(total + 1);
        size_t out = 0;
        for (size_t i = 0; i < list->count; i++) {
            size_t len = list->items[i].as.string->length;
            memcpy(buf + out, list->items[i].as.string->text, len);
            out += len;
            if (i < list->count - 1 && sep_len > 0) {
                memcpy(buf + out, sep, sep_len);
                out += sep_len;
            }
        }
        buf[out] = '\0';
        Value res = text_value(buf, total, at);
        free(buf); return res;
    }
    /* str.trim() */
    if (receiver.type == V_STR && !strcmp(method, "trim")) {
        if (args->count != 0) { error_at(at, "trim expects no arguments"); return nothing(); }
        const char *start = receiver.as.string->text;
        const char *end = start + receiver.as.string->length;
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
        while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\n' || *(end - 1) == '\r')) end--;
        return text_value(start, (size_t)(end - start), at);
    }
    /* str.contains(str) */
    if (receiver.type == V_STR && !strcmp(method, "contains")) {
        if (args->count != 1 || args->values[0].type != V_STR) { error_at(at, "contains expects 1 str argument"); return nothing(); }
        const char *text = receiver.as.string->text;
        const char *sub = args->values[0].as.string->text;
        return boolean_value(strstr(text, sub) != NULL);
    }
    /* str.replace(old, new) */
    if (receiver.type == V_STR && !strcmp(method, "replace")) {
        if (args->count != 2 || args->values[0].type != V_STR || args->values[1].type != V_STR) { error_at(at, "replace expects 2 str arguments (old, new)"); return nothing(); }
        const char *text = receiver.as.string->text;
        const char *old = args->values[0].as.string->text;
        const char *new_str = args->values[1].as.string->text;
        size_t old_len = args->values[0].as.string->length;
        size_t new_len = args->values[1].as.string->length;
        if (old_len == 0) { error_at(at, "empty target string"); return nothing(); }
        
        size_t occurrences = 0;
        const char *scan = text;
        while ((scan = strstr(scan, old)) != NULL) { occurrences++; scan += old_len; }
        
        size_t final_len = receiver.as.string->length + occurrences * (new_len - old_len);
        if (final_len > TEXT_LIMIT) { error_at(at, "string exceeds limit"); return nothing(); }
        
        char *buf = malloc(final_len + 1);
        size_t out = 0;
        scan = text;
        const char *match;
        while ((match = strstr(scan, old)) != NULL) {
            size_t copy_len = (size_t)(match - scan);
            memcpy(buf + out, scan, copy_len); out += copy_len;
            memcpy(buf + out, new_str, new_len); out += new_len;
            scan = match + old_len;
        }
        size_t rem = (size_t)((text + receiver.as.string->length) - scan);
        memcpy(buf + out, scan, rem); out += rem;
        buf[out] = '\0';
        Value res = text_value(buf, final_len, at);
        free(buf); return res;
    }
    error_at(at, "'%s' object has no method '%s'", type_label(receiver.type), method);
    return nothing();
}

static Value call_builtin(Runtime *rt, const char *name, Arguments *args, Location at) {
    if (!strcmp(name, "log") || !strcmp(name, "log.f")) return print_arguments(args, at);
    for (size_t i = 0; i < args->count; i++) if (args->names[i]) {
        error_at(at, "'%s' does not accept named arguments", name); return nothing();
    }
    if (!strcmp(name, "input")) return input_value(args->values, args->count, at);
    if (!strcmp(name, "rodot.create")) return rodot_create(args->values, args->count, at);
    if (!strcmp(name, "rodot.load")) return rodot_load(rt, args->values, args->count, at);
    if (!strcmp(name, "collision")) {
        if (args->count != 2 || !rodot_field(args->values[0], "_rodot_path") ||
            !rodot_field(args->values[1], "_rodot_path")) {
            error_at(at, "collision expects two loaded rodot sprites"); return nothing();
        }
        double box[2][4] = {{0}};
        for (int i = 0; i < 2; i++) {
            Value *data = rodot_field(args->values[i], "data");
            Value *collision_data = data ? rodot_field(*data, "collision") : NULL;
            Value *enabled = collision_data ? rodot_field(*collision_data, "enabled") : NULL;
            if (!enabled || enabled->type != V_BOOL || !enabled->as.boolean) return boolean_value(0);
            Value *x = rodot_field(*data, "x"), *y = rodot_field(*data, "y");
            Value *scale = rodot_field(*data, "scale");
            Value *cx = rodot_field(*collision_data, "x"), *cy = rodot_field(*collision_data, "y");
            Value *width = rodot_field(*collision_data, "width"), *height = rodot_field(*collision_data, "height");
            if (!x || !y || !scale || !cx || !cy || !width || !height ||
                !numeric(*x) || !numeric(*y) || !numeric(*scale) || !numeric(*cx) || !numeric(*cy) ||
                !numeric(*width) || !numeric(*height) || number(*scale) <= 0 || number(*width) < 0 || number(*height) < 0) {
                error_at(at, "invalid rodot collision rectangle"); return nothing();
            }
            box[i][0] = number(*x) + number(*cx) * number(*scale);
            box[i][1] = number(*y) + number(*cy) * number(*scale);
            box[i][2] = box[i][0] + number(*width) * number(*scale);
            box[i][3] = box[i][1] + number(*height) * number(*scale);
        }
        return boolean_value(box[0][0] < box[1][2] && box[0][2] > box[1][0] &&
            box[0][1] < box[1][3] && box[0][3] > box[1][1]);
    }
    if (!strncmp(name, "draw.", 5)) {
        if (!rt->game_active) { error_at(at, "draw requires an active game"); return nothing(); }
        int kind = !strcmp(name, "draw.rect") ? 1 : !strcmp(name, "draw.circle") ? 2 :
            !strcmp(name, "draw.line") ? 3 : !strcmp(name, "draw.text") ? 4 : 0;
        size_t base = kind == 2 || kind == 4 ? 3 : 4;
        if (!kind || (args->count != base && args->count != base + 1)) {
            error_at(at, "%s expects %zu coordinates/text arguments and optional color", name, base);
            return nothing();
        }
        if (kind == 4 && args->values[0].type != V_STR) {
            error_at(at, "draw.text requires a str first argument"); return nothing();
        }
        for (size_t i = kind == 4 ? 1 : 0; i < base; i++)
            if (!numeric(args->values[i]) || !isfinite(number(args->values[i])) || fabs(number(args->values[i])) > 1000000) {
                error_at(at, "draw coordinates must be finite numbers within one million"); return nothing();
            }
        uint32_t color = 0xffffff;
        if (args->count == base + 1) {
            Value c = args->values[base];
            if (c.type != V_STR || c.as.string->length != 7 || c.as.string->text[0] != '#') {
                error_at(at, "draw color must be '#RRGGBB'"); return nothing();
            }
            char *end = NULL;
            unsigned long parsed = strtoul(c.as.string->text + 1, &end, 16);
            if (!end || *end || parsed > 0xffffff) {
                error_at(at, "draw color must be '#RRGGBB'"); return nothing();
            }
            color = (uint32_t)parsed;
        }
        if (rt->draw_count == 4096) { error_at(at, "too many draw commands in one frame"); return nothing(); }
        if (rt->draw_count == rt->draw_capacity) {
            rt->draw_capacity = rt->draw_capacity ? rt->draw_capacity * 2 : 16;
            rt->draws = resize(rt->draws, rt->draw_capacity * sizeof(DrawCommand));
        }
        DrawCommand *command = &rt->draws[rt->draw_count++];
        *command = (DrawCommand){.kind = kind, .color = color, .label = nothing()};
        if (kind == 4) {
            command->label = retain(args->values[0]);
            command->x = (int)number(args->values[1]); command->y = (int)number(args->values[2]);
        } else {
            command->x = (int)number(args->values[0]); command->y = (int)number(args->values[1]);
            command->a = (int)number(args->values[2]);
            if (kind != 2) command->b = (int)number(args->values[3]);
        }
        return nothing();
    }
    if (!strcmp(name, "game.start")) {
        if ((args->count != 1 && args->count != 3 && args->count != 4) || args->values[0].type != V_STR) {
            error_at(at, "game.start expects scene name [, width, height [, title]]"); return nothing();
        }
        int width = 800, height = 600;
        if (args->count >= 3) {
            if (args->values[1].type != V_INT || args->values[2].type != V_INT ||
                args->values[1].as.integer < 64 || args->values[1].as.integer > 4096 ||
                args->values[2].as.integer < 64 || args->values[2].as.integer > 4096) {
                error_at(at, "game window width and height must be ints from 64 to 4096"); return nothing();
            }
            width = (int)args->values[1].as.integer; height = (int)args->values[2].as.integer;
        }
        if (args->count == 4 && args->values[3].type != V_STR) {
            error_at(at, "game window title must be str"); return nothing();
        }
        const char *title = args->count == 4 ? args->values[3].as.string->text : "SianLang Game";
        game_run(rt, args->values[0].as.string->text, width, height, title, at);
        return nothing();
    }
    if (!strcmp(name, "game.close")) {
        if (args->count || !rt->game_active) { error_at(at, "game.close requires an active game and no arguments"); return nothing(); }
        rt->game_exit = 1;
        return nothing();
    }
    if (!strcmp(name, "scene.change")) {
        if (args->count != 1 || args->values[0].type != V_STR || !rt->game_active) {
            error_at(at, "scene.change requires an active game and a scene name"); return nothing();
        }
        const char *target = args->values[0].as.string->text;
        Statement *scene = rt->program;
        while (scene && (scene->kind != S_SCENE || strcmp(scene->name, target))) scene = scene->next;
        if (!scene) { error_at(at, "scene '%s' not found", target); return nothing(); }
        rt->next_scene = scene->name;
        return nothing();
    }
    if (!strcmp(name, "game.delta_time")) {
        if (args->count) { error_at(at, "game.delta_time expects no arguments"); return nothing(); }
        return decimal_value(rt->delta_time, at);
    }
    if (!strcmp(name, "key.down")) {
        if (args->count != 1 || args->values[0].type != V_STR) {
            error_at(at, "key.down expects one key name"); return nothing();
        }
#ifdef _WIN32
        const char *key = args->values[0].as.string->text;
        int code = 0;
        if (!strcmp(key, "left")) code = VK_LEFT;
        else if (!strcmp(key, "right")) code = VK_RIGHT;
        else if (!strcmp(key, "up")) code = VK_UP;
        else if (!strcmp(key, "down")) code = VK_DOWN;
        else if (!strcmp(key, "enter")) code = VK_RETURN;
        else if (!strcmp(key, "space")) code = VK_SPACE;
        else if (strlen(key) == 1 && ascii_letter((unsigned char)key[0])) code = toupper((unsigned char)key[0]);
        if (!code) { error_at(at, "unknown key '%s'", key); return nothing(); }
        return boolean_value(rt->game_active && GetForegroundWindow() == rt->window && (GetAsyncKeyState(code) & 0x8000) != 0);
#else
        error_at(at, "key.down is available on Windows only"); return nothing();
#endif
    }
    if (!strcmp(name, "range")) {
        if (args->count < 1 || args->count > 3) { error_at(at, "range expects 1 to 3 int arguments"); return nothing(); }
        int64_t start = 0, stop = 0, step = 1;
        for (size_t i = 0; i < args->count; i++) if (args->values[i].type != V_INT) { error_at(at, "range arguments must be int"); return nothing(); }
        if (args->count == 1) stop = args->values[0].as.integer;
        else { start = args->values[0].as.integer; stop = args->values[1].as.integer; }
        if (args->count == 3) step = args->values[2].as.integer;
        if (!step) { error_at(at, "range step cannot be zero"); return nothing(); }
        size_t count = 0; for (int64_t i = start; step > 0 ? i < stop : i > stop; ) { count++; if ((step > 0 && i > INT64_MAX - step) || (step < 0 && i < INT64_MIN - step)) break; i += step; }
        Value *items = resize(NULL, count * sizeof(Value)); int64_t current = start;
        for (size_t i = 0; i < count; i++) { items[i] = integer_value(current); current += step; }
        Value result = tuple_value(rt, items, count); free(items); return result;
    }
    if (!strcmp(name, "time.now")) {
        if (args->count != 0) { error_at(at, "time.now expects no arguments"); return nothing(); }
        return integer_value((int64_t)time(NULL));
    }
    /* ── Math builtins ── */
    if (!strcmp(name, "abs")) {
        if (args->count != 1) { error_at(at, "abs expects 1 argument"); return nothing(); }
        Value v = args->values[0];
        if (v.type == V_INT) {
            if (v.as.integer == INT64_MIN) { error_at(at, "abs result is outside int range"); return nothing(); }
            return integer_value(v.as.integer < 0 ? -v.as.integer : v.as.integer);
        }
        if (v.type == V_FLOAT) return decimal_value(fabs(v.as.decimal), at);
        error_at(at, "abs requires int or float"); return nothing();
    }
    if (!strcmp(name, "min") || !strcmp(name, "max")) {
        int is_max = !strcmp(name, "max");
        if (args->count == 0) { error_at(at, "%s requires at least 1 argument", name); return nothing(); }
        /* allow min(list) / max(list) with a single list argument */
        if (args->count == 1 && args->values[0].type == V_LIST) {
            List *list = args->values[0].as.list;
            if (list->count == 0) { error_at(at, "%s of empty list", name); return nothing(); }
            Value best = list->items[0];
            for (size_t i = 1; i < list->count && !has_error; i++) {
                Value cmp = compare_values(is_max ? OP_GT : OP_LT, list->items[i], best, at);
                if (!has_error && cmp.as.boolean) best = list->items[i];
                release(cmp);
            }
            return retain(best);
        }
        if (args->count < 2) { error_at(at, "%s requires at least 2 arguments or a list", name); return nothing(); }
        Value best = args->values[0];
        for (size_t i = 1; i < args->count && !has_error; i++) {
            Value cmp = compare_values(is_max ? OP_GT : OP_LT, args->values[i], best, at);
            if (!has_error && cmp.as.boolean) best = args->values[i];
            release(cmp);
        }
        return retain(best);
    }
    if (!strcmp(name, "round")) {
        if (args->count < 1 || args->count > 2) { error_at(at, "round expects 1 or 2 arguments"); return nothing(); }
        int64_t places = 0;
        if (args->count == 2) {
            if (args->values[1].type != V_INT) { error_at(at, "round's second argument must be int"); return nothing(); }
            places = args->values[1].as.integer;
        }
        Value v = args->values[0];
        double d;
        if (v.type == V_INT) d = (double)v.as.integer;
        else if (v.type == V_FLOAT) d = v.as.decimal;
        else { error_at(at, "round requires a number"); return nothing(); }
        double factor = pow(10.0, (double)places);
        double rounded = round(d * factor) / factor;
        if (places <= 0 && v.type == V_INT) return integer_value((int64_t)rounded);
        if (places == 0) return decimal_value(round(d), at);
        return decimal_value(rounded, at);
    }
    /* ── Random builtins ── */
    if (!strcmp(name, "random.int")) {
        if (args->count != 2 || args->values[0].type != V_INT || args->values[1].type != V_INT)
            { error_at(at, "random.int expects two int arguments (min, max)"); return nothing(); }
        int64_t lo = args->values[0].as.integer, hi = args->values[1].as.integer;
        if (lo > hi) { error_at(at, "random.int: min must be <= max"); return nothing(); }
        uint64_t range = (uint64_t)hi - (uint64_t)lo + 1;
        uint64_t offset = (uint64_t)rand() % range;
        int64_t r = (int64_t)((uint64_t)lo + offset);
        return integer_value(r);
    }
    if (!strcmp(name, "random.float")) {
        if (args->count != 0) { error_at(at, "random.float expects no arguments"); return nothing(); }
        return decimal_value((double)rand() / ((double)RAND_MAX + 1.0), at);
    }
    if (!strcmp(name, "random.choice")) {
        if (args->count != 1 || args->values[0].type != V_LIST)
            { error_at(at, "random.choice expects a list argument"); return nothing(); }
        List *list = args->values[0].as.list;
        if (list->count == 0) { error_at(at, "random.choice from empty list"); return nothing(); }
        size_t i = (size_t)rand() % list->count;
        return retain(list->items[i]);
    }
    /* ── File I/O ── */
    if (!strcmp(name, "open")) {
        if (args->count != 2 || args->values[0].type != V_STR || args->values[1].type != V_STR)
            { error_at(at, "open expects two str arguments (path, mode)"); return nothing(); }
        const char *path = args->values[0].as.string->text;
        const char *mode = args->values[1].as.string->text;
        /* validate mode */
        if (strcmp(mode, "r") && strcmp(mode, "w") && strcmp(mode, "a") &&
            strcmp(mode, "rb") && strcmp(mode, "wb") && strcmp(mode, "ab"))
            { error_at(at, "open mode must be 'r', 'w', or 'a'"); return nothing(); }
#ifdef _WIN32
        /* Use wide-char path on Windows so Unicode file names work */
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
        wchar_t *wpath = resize(NULL, (size_t)wlen * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
        int wmlen = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
        wchar_t *wmode = resize(NULL, (size_t)wmlen * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmlen);
        FILE *fp = _wfopen(wpath, wmode);
        free(wpath); free(wmode);
#else
        FILE *fp = fopen(path, mode);
#endif
        if (!fp) {
            error_at(at, "cannot open file '%s': %s", path, strerror(errno));
            return nothing();
        }
        FileObj *f = new_object(rt, sizeof(*f), G_FILE);
        f->fp = fp; f->closed = 0;
        Value result = {.type = V_FILE}; result.as.file = f; return result;
    }
    if (args->count != 1) { error_at(at, "wrong number of arguments to '%s' (expected 1)", name); return nothing(); }
    if (!strcmp(name, "len")) {
        Value value = args->values[0];
        if (value.type == V_TUPLE) return integer_value((int64_t)value.as.tuple->count);
        if (value.type == V_LIST) return integer_value((int64_t)value.as.list->count);
        if (value.type == V_DICT) return integer_value((int64_t)value.as.dict->count);
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
    if (fn->builtin) return call_builtin(rt, fn->builtin, args, at);
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
    /* ── Method call: receiver.method(args) ── */
    if (expr->left->kind == E_MEMBER) {
        Value receiver = evaluate(rt, env, expr->left->left);
        Arguments args = {0};
        for (ExprList *item = expr->args; item && !has_error; item = item->next) {
            Value value = evaluate(rt, env, item->value);
            if (!has_error) {
                if (!item->spread) add_argument(&args, value, item->name, item->value->at);
                else if (value.type != V_TUPLE) error_at(item->value->at, "#expansion requires variadic arguments");
                else for (size_t i = 0; i < value.as.tuple->count && !has_error; i++)
                    add_argument(&args, value.as.tuple->items[i], NULL, item->value->at);
            }
            release(value);
        }
        Value result = has_error ? nothing() : call_method(rt, receiver, expr->left->text, &args, expr->at);
        release(receiver); free_arguments(&args);
        return result;
    }
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

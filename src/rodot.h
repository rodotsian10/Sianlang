#ifndef SIAN_RODOT_H
#define SIAN_RODOT_H

static const unsigned char rodot_png_magic[8] = {137, 80, 78, 71, 13, 10, 26, 10};
static const char rodot_magic[8] = {'R','O','D','O','T','0','0','1'};

static Value *rodot_field(Value object, const char *name) {
    if (object.type != V_DICT) return NULL;
    for (size_t i = 0; i < object.as.dict->count; i++) {
        DictEntry *entry = &object.as.dict->items[i];
        if (entry->key.type == V_STR && !strcmp(entry->key.as.string->text, name)) return &entry->value;
    }
    return NULL;
}
static int rodot_property(const char *name) {
    static const char *names[] = {"visible", "x", "y", "scale", "rotation", "opacity",
        "layer", "speed", "collision", "user"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++)
        if (!strcmp(name, names[i])) return 1;
    return 0;
}

static FILE *rodot_open(const char *path, const char *mode) {
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!size) return NULL;
    wchar_t *wide = resize(NULL, (size_t)size * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, size);
    FILE *file = !strcmp(mode, "rb") ? _wfopen(wide, L"rb") : _wfopen(wide, L"wb");
    free(wide); return file;
#else
    return fopen(path, mode);
#endif
}

static uint32_t rodot_u32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static void rodot_json_space(const char **cursor) {
    while (**cursor == ' ' || **cursor == '\r' || **cursor == '\n' || **cursor == '\t') (*cursor)++;
}
static int rodot_json_string(const char **cursor) {
    if (*(*cursor)++ != '"') return 0;
    while (**cursor && **cursor != '"') {
        unsigned char ch = (unsigned char)**cursor;
        if (ch < 32) return 0;
        if (ch == '\\') {
            (*cursor)++;
            if (!strchr("\\\"nrt", **cursor) || !**cursor) return 0;
        }
        (*cursor)++;
    }
    if (**cursor != '"') return 0;
    (*cursor)++; return 1;
}
static int rodot_json_value(const char **cursor, int depth) {
    if (depth > 64) return 0;
    rodot_json_space(cursor);
    if (**cursor == '"') return rodot_json_string(cursor);
    if (**cursor == '{') {
        (*cursor)++; rodot_json_space(cursor);
        if (**cursor == '}') { (*cursor)++; return 1; }
        do {
            if (!rodot_json_string(cursor)) return 0;
            rodot_json_space(cursor);
            if (**cursor != ':') return 0;
            (*cursor)++;
            if (!rodot_json_value(cursor, depth + 1)) return 0;
            rodot_json_space(cursor);
            if (**cursor == '}') { (*cursor)++; return 1; }
            if (**cursor != ',') return 0;
            (*cursor)++; rodot_json_space(cursor);
        } while (**cursor);
        return 0;
    }
    if (**cursor == '[') {
        (*cursor)++; rodot_json_space(cursor);
        if (**cursor == ']') { (*cursor)++; return 1; }
        do {
            if (!rodot_json_value(cursor, depth + 1)) return 0;
            rodot_json_space(cursor);
            if (**cursor == ']') { (*cursor)++; return 1; }
            if (**cursor != ',') return 0;
            (*cursor)++;
        } while (**cursor);
        return 0;
    }
    const char *start = *cursor;
    if (**cursor == '-' || ascii_digit((unsigned char)**cursor)) {
        if (**cursor == '-') (*cursor)++;
        if (!ascii_digit((unsigned char)**cursor)) return 0;
        if (**cursor == '0') (*cursor)++;
        else while (ascii_digit((unsigned char)**cursor)) (*cursor)++;
        int decimal = 0;
        if (**cursor == '.') {
            decimal = 1; (*cursor)++;
            if (!ascii_digit((unsigned char)**cursor)) return 0;
            while (ascii_digit((unsigned char)**cursor)) (*cursor)++;
        }
        if (**cursor == 'e' || **cursor == 'E') {
            decimal = 1; (*cursor)++;
            if (**cursor == '+' || **cursor == '-') (*cursor)++;
            if (!ascii_digit((unsigned char)**cursor)) return 0;
            while (ascii_digit((unsigned char)**cursor)) (*cursor)++;
        }
        errno = 0;
        if (decimal) { double number = strtod(start, NULL); return errno != ERANGE && isfinite(number); }
        strtoll(start, NULL, 10); return errno != ERANGE;
    }
    if (!strncmp(*cursor, "true", 4)) { *cursor += 4; return 1; }
    if (!strncmp(*cursor, "false", 5)) { *cursor += 5; return 1; }
    if (!strncmp(*cursor, "null", 4)) { *cursor += 4; return 1; }
    return 0;
}

static int rodot_png_size(FILE *file, uint32_t *width, uint32_t *height) {
    unsigned char header[24];
    if (fseek(file, 0, SEEK_SET) || fread(header, 1, sizeof(header), file) != sizeof(header)) return 0;
    if (memcmp(header, rodot_png_magic, 8) || memcmp(header + 12, "IHDR", 4)) return 0;
    *width = rodot_u32(header + 16); *height = rodot_u32(header + 20);
    return *width && *height && *width <= 16384 && *height <= 16384;
}

static Value rodot_create(Value *args, size_t count, Location at) {
    if (count != 2 || args[0].type != V_STR || args[1].type != V_STR) {
        error_at(at, "rodot.create expects PNG path and .rodot path"); return nothing();
    }
    const char *input = args[0].as.string->text, *output = args[1].as.string->text;
    size_t out_length = strlen(output);
    if (out_length < 7 || strcmp(output + out_length - 6, ".rodot")) {
        error_at(at, "rodot output must end with .rodot"); return nothing();
    }
    FILE *existing = rodot_open(output, "rb");
    if (existing) {
        fclose(existing);
        error_at(at, "rodot '%s' already exists; choose a new output path", output);
        return nothing();
    }
    FILE *in = rodot_open(input, "rb");
    if (!in) { error_at(at, "cannot open PNG '%s'", input); return nothing(); }
    uint32_t width = 0, height = 0;
    if (!rodot_png_size(in, &width, &height) || fseek(in, 0, SEEK_END)) {
        fclose(in); error_at(at, "invalid PNG image"); return nothing();
    }
    long length = ftell(in);
    if (length < 24 || length > (long)TEXT_LIMIT || fseek(in, 0, SEEK_SET)) {
        fclose(in); error_at(at, "PNG is too large or unreadable"); return nothing();
    }
    unsigned char *bytes = resize(NULL, (size_t)length);
    if (fread(bytes, 1, (size_t)length, in) != (size_t)length) {
        free(bytes); fclose(in); error_at(at, "failed to read PNG"); return nothing();
    }
    fclose(in);
    const char *base = strrchr(output, '/');
    const char *slash = strrchr(output, '\\');
    if (!base || (slash && slash > base)) base = slash;
    base = base ? base + 1 : output;
    char name[256]; size_t n = 0;
    while (base[n] && base[n] != '.' && n < sizeof(name) - 1) {
        unsigned char c = (unsigned char)base[n];
        name[n] = (char)(ascii_letter(c) || ascii_digit(c) ? c : '_'); n++;
    }
    name[n] = '\0'; if (!n) strcpy(name, "sprite");
    char json[2048];
    int json_length = snprintf(json, sizeof(json),
        "{\"name\":\"%s\",\"meta\":{\"width\":%" PRIu32 ",\"height\":%" PRIu32 "},"
        "\"data\":{\"visible\":true,\"x\":0,\"y\":0,\"scale\":1.0,"
        "\"rotation\":0,\"opacity\":1.0,\"layer\":0,\"speed\":5,"
        "\"collision\":{\"enabled\":true,\"x\":0,\"y\":0,"
        "\"width\":%" PRIu32 ",\"height\":%" PRIu32 "},\"user\":{}}}",
        name, width, height, width, height);
    if (json_length < 0 || (size_t)json_length >= sizeof(json)) {
        free(bytes); error_at(at, "rodot metadata is too large"); return nothing();
    }
    FILE *out = rodot_open(output, "wb");
    if (!out) { free(bytes); error_at(at, "cannot create rodot '%s'", output); return nothing(); }
    uint32_t size = (uint32_t)json_length;
    unsigned char tail[4] = {(unsigned char)size, (unsigned char)(size >> 8),
        (unsigned char)(size >> 16), (unsigned char)(size >> 24)};
    int ok = fwrite(bytes, 1, (size_t)length, out) == (size_t)length &&
        fwrite(json, 1, size, out) == size && fwrite(tail, 1, 4, out) == 4 &&
        fwrite(rodot_magic, 1, 8, out) == 8;
    if (fclose(out)) ok = 0;
    free(bytes);
    if (!ok) error_at(at, "failed to write rodot '%s'", output);
    return nothing();
}

static Value rodot_load(Runtime *rt, Value *args, size_t count, Location at) {
    if (count != 1 || args[0].type != V_STR) {
        error_at(at, "rodot.load expects one .rodot path"); return nothing();
    }
    const char *path = args[0].as.string->text;
    if (rt->game_active && rt->loading_rodot_name) {
        for (size_t i = 0; i < rt->saved_rodot_count; i++) {
            SavedRodot *saved = &rt->saved_rodots[i];
            Value *saved_path = rodot_field(saved->sprite, "_rodot_path");
            if (!strcmp(saved->name, rt->loading_rodot_name) && saved_path &&
                saved_path->type == V_STR && !strcmp(saved_path->as.string->text, path))
                return retain(saved->sprite);
        }
    }
    FILE *file = rodot_open(path, "rb");
    if (!file) { error_at(at, "cannot open rodot '%s'", path); return nothing(); }
    uint32_t width = 0, height = 0;
    if (!rodot_png_size(file, &width, &height) || fseek(file, 0, SEEK_END)) {
        fclose(file); error_at(at, "invalid rodot PNG data"); return nothing();
    }
    long end = ftell(file);
    unsigned char tail[12];
    if (end < 36 || fseek(file, end - 12, SEEK_SET) || fread(tail, 1, 12, file) != 12 ||
        memcmp(tail + 4, rodot_magic, 8)) {
        fclose(file); error_at(at, "invalid rodot metadata trailer"); return nothing();
    }
    uint32_t size = (uint32_t)tail[0] | ((uint32_t)tail[1] << 8) |
        ((uint32_t)tail[2] << 16) | ((uint32_t)tail[3] << 24);
    if (!size || size > TEXT_LIMIT || (long)size > end - 36 || fseek(file, end - 12 - (long)size, SEEK_SET)) {
        fclose(file); error_at(at, "invalid rodot metadata length"); return nothing();
    }
    char *json = resize(NULL, (size_t)size + 1);
    int ok = fread(json, 1, size, file) == size;
    fclose(file); json[size] = '\0';
    if (!ok) { free(json); error_at(at, "failed to read rodot metadata"); return nothing(); }
    if (memchr(json, 0, size)) {
        free(json); error_at(at, "rodot metadata contains a NUL byte"); return nothing();
    }
    const char *valid = json;
    int valid_json = rodot_json_value(&valid, 0);
    rodot_json_space(&valid);
    if (!valid_json || *valid) {
        free(json); error_at(at, "invalid rodot JSON metadata"); return nothing();
    }
    const char *cursor = json;
    Value sprite = parse_json_val(rt, &cursor);
    free(json);
    Value *data = rodot_field(sprite, "data"), *meta = rodot_field(sprite, "meta");
    Value *stored_width = meta ? rodot_field(*meta, "width") : NULL;
    Value *stored_height = meta ? rodot_field(*meta, "height") : NULL;
    if (sprite.type != V_DICT || !data || data->type != V_DICT ||
        !stored_width || stored_width->type != V_INT || stored_width->as.integer != width ||
        !stored_height || stored_height->type != V_INT || stored_height->as.integer != height) {
        release(sprite); error_at(at, "rodot metadata does not match PNG dimensions"); return nothing();
    }
    Dict *dict = sprite.as.dict;
    if (dict->count == dict->capacity) {
        dict->capacity = dict->capacity ? dict->capacity * 2 : 8;
        dict->items = resize(dict->items, dict->capacity * sizeof(DictEntry));
    }
    dict->items[dict->count].key = text_value("_rodot_path", 11, at);
    dict->items[dict->count].value = retain(args[0]);
    dict->count++;
    return sprite;
}

static void rodot_remove_temp(const char *path) {
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (size) {
        wchar_t *wide = resize(NULL, (size_t)size * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, size);
        _wremove(wide); free(wide);
    }
#else
    remove(path);
#endif
}
static int rodot_replace_file(const char *temp, const char *target) {
#ifdef _WIN32
    int a = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, temp, -1, NULL, 0);
    int b = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, target, -1, NULL, 0);
    if (!a || !b) return 0;
    wchar_t *wide_temp = resize(NULL, (size_t)a * sizeof(wchar_t));
    wchar_t *wide_target = resize(NULL, (size_t)b * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, temp, -1, wide_temp, a);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, target, -1, wide_target, b);
    int ok = MoveFileExW(wide_temp, wide_target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    free(wide_temp); free(wide_target); return ok;
#else
    return rename(temp, target) == 0;
#endif
}
static int rodot_json_compatible(Value value, Object **parents, int depth) {
    if (depth > 64) return 0;
    if (value.type == V_VOID || value.type == V_INT || value.type == V_BOOL) return 1;
    if (value.type == V_FLOAT) return isfinite(value.as.decimal);
    if (value.type == V_STR) {
        for (size_t i = 0; i < value.as.string->length; i++) {
            unsigned char ch = (unsigned char)value.as.string->text[i];
            if (ch < 32 && ch != '\n' && ch != '\r' && ch != '\t') return 0;
        }
        return 1;
    }
    if (value.type != V_DICT && value.type != V_LIST && value.type != V_TUPLE) return 0;
    Object *object = value_object(value);
    for (int i = 0; i < depth; i++) if (parents[i] == object) return 0;
    parents[depth] = object;
    if (value.type == V_DICT) {
        for (size_t i = 0; i < value.as.dict->count; i++) {
            DictEntry *entry = &value.as.dict->items[i];
            if (entry->key.type != V_STR || !rodot_json_compatible(entry->key, parents, depth + 1) ||
                !rodot_json_compatible(entry->value, parents, depth + 1)) return 0;
        }
    } else {
        size_t count = value.type == V_LIST ? value.as.list->count : value.as.tuple->count;
        Value *items = value.type == V_LIST ? value.as.list->items : value.as.tuple->items;
        for (size_t i = 0; i < count; i++)
            if (!rodot_json_compatible(items[i], parents, depth + 1)) return 0;
    }
    return 1;
}
static Value rodot_write_file(Runtime *rt, Value *args, size_t count, Location at) {
    (void)rt;
    if ((count != 1 && count != 2) || args[0].type != V_DICT ||
        (count == 2 && args[1].type != V_STR)) {
        error_at(at, "Frodot requires a loaded sprite"); return nothing();
    }
    Value sprite = args[0];
    Value *original_path = rodot_field(sprite, "_rodot_path");
    Value *name = rodot_field(sprite, "name"), *meta = rodot_field(sprite, "meta");
    Value *data = rodot_field(sprite, "data");
    if (!original_path || original_path->type != V_STR || !name || name->type != V_STR ||
        !meta || meta->type != V_DICT || !data || data->type != V_DICT) {
        error_at(at, "Frodot requires an intact loaded sprite"); return nothing();
    }
    Object *parents[65] = {0};
    if (!rodot_json_compatible(*name, parents, 0) || !rodot_json_compatible(*meta, parents, 0) ||
        !rodot_json_compatible(*data, parents, 0)) {
        error_at(at, "rodot data must be acyclic JSON values with string keys"); return nothing();
    }
    const char *source = original_path->as.string->text;
    const char *target = count == 2 ? args[1].as.string->text : source;
    size_t target_length = strlen(target);
    if (target_length < 7 || strcmp(target + target_length - 6, ".rodot")) {
        error_at(at, "rodot output must end with .rodot"); return nothing();
    }
    FILE *in = rodot_open(source, "rb");
    if (!in) { error_at(at, "cannot reopen rodot image"); return nothing(); }
    uint32_t width = 0, height = 0;
    Value *stored_width = rodot_field(*meta, "width"), *stored_height = rodot_field(*meta, "height");
    if (!rodot_png_size(in, &width, &height) || !stored_width || stored_width->type != V_INT ||
        stored_width->as.integer != width || !stored_height || stored_height->type != V_INT ||
        stored_height->as.integer != height || fseek(in, 0, SEEK_END)) {
        fclose(in); error_at(at, "rodot image dimensions changed"); return nothing();
    }
    long end = ftell(in);
    unsigned char tail[12];
    if (end < 36 || fseek(in, end - 12, SEEK_SET) || fread(tail, 1, 12, in) != 12 ||
        memcmp(tail + 4, rodot_magic, 8)) {
        fclose(in); error_at(at, "invalid rodot image trailer"); return nothing();
    }
    uint32_t old_json_length = (uint32_t)tail[0] | ((uint32_t)tail[1] << 8) |
        ((uint32_t)tail[2] << 16) | ((uint32_t)tail[3] << 24);
    long image_length = end - 12 - (long)old_json_length;
    if (image_length < 24 || fseek(in, 0, SEEK_SET)) {
        fclose(in); error_at(at, "invalid rodot image length"); return nothing();
    }
    char temp[4096];
#ifdef _WIN32
    int temp_length = snprintf(temp, sizeof(temp), "%s.part.%lu", target, (unsigned long)GetCurrentProcessId());
#else
    int temp_length = snprintf(temp, sizeof(temp), "%s.part", target);
#endif
    if (temp_length < 0 || (size_t)temp_length >= sizeof(temp)) {
        fclose(in); error_at(at, "rodot path is too long"); return nothing();
    }
    FILE *out = rodot_open(temp, "wb");
    if (!out) { fclose(in); error_at(at, "cannot create rodot temporary file"); return nothing(); }
    unsigned char buffer[8192]; int ok = 1;
    while (image_length > 0 && ok) {
        size_t chunk = image_length > (long)sizeof(buffer) ? sizeof(buffer) : (size_t)image_length;
        if (fread(buffer, 1, chunk, in) != chunk || fwrite(buffer, 1, chunk, out) != chunk) ok = 0;
        image_length -= (long)chunk;
    }
    fclose(in);
    long start_json = ftell(out);
    if (ok && start_json >= 0) {
        fputs("{\"name\":", out); stringify_json(out, *name, 0);
        fputs(",\"meta\":", out); stringify_json(out, *meta, 0);
        fputs(",\"data\":", out); stringify_json(out, *data, 0);
        fputc('}', out);
    }
    long end_json = ftell(out);
    if (!ok || start_json < 0 || end_json < start_json || end_json - start_json > (long)TEXT_LIMIT) ok = 0;
    if (ok) {
        uint32_t length = (uint32_t)(end_json - start_json);
        unsigned char bytes[4] = {(unsigned char)length, (unsigned char)(length >> 8),
            (unsigned char)(length >> 16), (unsigned char)(length >> 24)};
        if (fwrite(bytes, 1, 4, out) != 4 || fwrite(rodot_magic, 1, 8, out) != 8) ok = 0;
    }
    if (fclose(out)) ok = 0;
    if (ok) ok = rodot_replace_file(temp, target);
    if (!ok) { rodot_remove_temp(temp); error_at(at, "failed to save rodot; original file was preserved"); }
    return nothing();
}
#endif

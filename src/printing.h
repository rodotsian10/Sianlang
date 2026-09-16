#ifndef SIAN_PRINTING_H
#define SIAN_PRINTING_H
typedef struct { char *text; size_t count, capacity; Location at; } TextBuffer;
static void append_text(TextBuffer *b, const char *s, size_t count) {
    if (has_error) return;
    if (count > TEXT_LIMIT - b->count) { error_at(b->at, "formatted text exceeds 16 MiB limit"); return; }
    size_t need = b->count + count + 1;
    if (need > b->capacity) {
        size_t capacity = b->capacity ? b->capacity : 64;
        while (capacity < need) capacity *= 2;
        b->text = resize(b->text, capacity); b->capacity = capacity;
    }
    memcpy(b->text + b->count, s, count); b->count += count; b->text[b->count] = '\0';
}
static void append_cstr(TextBuffer *b, const char *s) { append_text(b, s, strlen(s)); }
static void double_text(double value, char *buffer, size_t capacity) {
    int precision = 1;
    char candidate[128];
    for (; precision < 17; precision++) {
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        if (strtod(candidate, NULL) == value) break;
    }
    snprintf(candidate, sizeof(candidate), "%.*e", precision - 1, value);
    char *exponent_text = strchr(candidate, 'e');
    int exponent = exponent_text ? atoi(exponent_text + 1) : 0;
    if (exponent >= -4 && exponent < 16) {
        int places = precision - exponent - 1;
        if (places < 0) places = 0;
        snprintf(buffer, capacity, "%.*f", places, value);
        if (!strchr(buffer, '.')) {
            size_t length = strlen(buffer);
            snprintf(buffer + length, capacity - length, ".0");
        }
    } else {
        snprintf(buffer, capacity, "%.*e", precision - 1, value);
        /* Windows CRT versions may emit three exponent digits. Python uses at least two. */
        char *e = strchr(buffer, 'e');
        if (e && strlen(e + 2) > 2 && e[2] == '0') memmove(e + 2, e + 3, strlen(e + 3) + 1);
    }
}
static void render_value(TextBuffer *b, Value value, int repr, unsigned int depth) {
    if (depth >= DEPTH_LIMIT) { error_at(b->at, "value display nesting is too deep"); return; }
    char buffer[128];
    switch (value.type) {
        case V_VOID: append_cstr(b, "None"); break;
        case V_INT: snprintf(buffer, sizeof(buffer), "%" PRId64, value.as.integer); append_cstr(b, buffer); break;
        case V_FLOAT: double_text(value.as.decimal, buffer, sizeof(buffer)); append_cstr(b, buffer); break;
        case V_BOOL: append_cstr(b, value.as.boolean ? "True" : "False"); break;
        case V_STR:
            if (!repr) append_text(b, value.as.string->text, value.as.string->length);
            else {
                const char *s = value.as.string->text;
                char quote = strchr(s, '\'') && !strchr(s, '"') ? '"' : '\'';
                append_text(b, &quote, 1);
                for (size_t i = 0; i < value.as.string->length; i++) {
                    unsigned char ch = (unsigned char)s[i];
                    if (ch == (unsigned char)quote || ch == '\\') append_cstr(b, "\\");
                    if (ch == '\n') append_cstr(b, "\\n");
                    else if (ch == '\r') append_cstr(b, "\\r");
                    else if (ch == '\t') append_cstr(b, "\\t");
                    else if (ch < 32 || ch == 127) { snprintf(buffer, sizeof(buffer), "\\x%02x", ch); append_cstr(b, buffer); }
                    else append_text(b, s + i, 1);
                }
                append_text(b, &quote, 1);
            }
            break;
        case V_FUNCTION:
            append_cstr(b, "<function ");
            append_cstr(b, value.as.function->builtin ? value.as.function->builtin : value.as.function->definition->name);
            append_cstr(b, ">"); break;
        case V_TUPLE:
            append_cstr(b, "(");
            for (size_t i = 0; i < value.as.tuple->count; i++) {
                if (i) append_cstr(b, ", ");
                render_value(b, value.as.tuple->items[i], 1, depth + 1);
            }
            if (value.as.tuple->count == 1) append_cstr(b, ",");
            append_cstr(b, ")"); break;
        case V_LIST:
            append_cstr(b, "[");
            for (size_t i = 0; i < value.as.list->count; i++) {
                if (i) append_cstr(b, ", ");
                render_value(b, value.as.list->items[i], 1, depth + 1);
            }
            append_cstr(b, "]"); break;
        case V_DICT:
            append_cstr(b, "{");
            for (size_t i = 0; i < value.as.dict->count; i++) {
                if (i) append_cstr(b, ", ");
                render_value(b, value.as.dict->items[i].key, 1, depth + 1);
                append_cstr(b, ": ");
                render_value(b, value.as.dict->items[i].value, 1, depth + 1);
            }
            append_cstr(b, "}"); break;
        default: error_at(b->at, "cannot display this value"); break;
    }
}
static Value display_value(Value value, int repr, Location at) {
    TextBuffer buffer = {.at = at}; render_value(&buffer, value, repr, 0);
    Value result = has_error ? nothing() : text_value(buffer.text ? buffer.text : "", buffer.count, at);
    free(buffer.text); return result;
}
static size_t character_count(const char *text, size_t bytes) {
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) if (((unsigned char)text[i] & 0xc0) != 0x80) count++;
    return count;
}
static Value format_field(Value value, const char *spec, Location at) {
    if (!spec || !*spec) return display_value(value, 0, at);
    int repr = 0, converted = 0;
    if (*spec == '!') {
        spec++;
        if (*spec != 's' && *spec != 'r') { error_at(at, "format conversion must be !s or !r"); return nothing(); }
        repr = *spec++ == 'r'; converted = 1;
    }
    Value working = converted ? display_value(value, repr, at) : retain(value);
    if (!*spec) return working;
    if (*spec++ != ':') { release(working); error_at(at, "invalid format specification"); return nothing(); }
    char align = numeric(working) ? '>' : '<', fill = ' ', sign = 0;
    int zero = 0, width = 0, precision = -1;
    if (spec[0] && spec[1] && strchr("<>^", spec[1])) { fill = *spec++; align = *spec++; }
    else if (*spec && strchr("<>^", *spec)) align = *spec++;
    if (*spec == '+' || *spec == '-' || *spec == ' ') sign = *spec++;
    if (*spec == '0') { zero = 1; spec++; }
    while (ascii_digit((unsigned char)*spec)) {
        if (width > 100000) { error_at(at, "format width is too large"); break; }
        width = width * 10 + (*spec++ - '0');
    }
    if (*spec == '.') {
        precision = 0; spec++;
        if (!ascii_digit((unsigned char)*spec)) error_at(at, "format precision requires digits");
        while (ascii_digit((unsigned char)*spec) && !has_error) {
            if (precision > 1000) { error_at(at, "format precision is too large"); break; }
            precision = precision * 10 + (*spec++ - '0');
        }
    }
    char kind = *spec ? *spec++ : 0;
    if (*spec) error_at(at, "unsupported format specification");
    TextBuffer text = {.at = at};
    if (!has_error && kind && strchr("fFeEgG%", kind)) {
        if (!numeric(working)) error_at(at, "numeric format requires a number");
        else {
            double d = number(working);
            if (kind == '%') d *= 100;
            char pattern[16];
            snprintf(pattern, sizeof(pattern), "%%%s.*%c", sign == '+' ? "+" : sign == ' ' ? " " : "", kind == '%' ? 'f' : kind);
            int length = snprintf(NULL, 0, pattern, precision < 0 ? 6 : precision, d);
            char *result = resize(NULL, (size_t)(length < 0 ? 0 : length) + 1);
            if (length < 0) error_at(at, "cannot format number");
            else { snprintf(result, (size_t)length + 1, pattern, precision < 0 ? 6 : precision, d); append_cstr(&text, result); }
            free(result);
            if (kind == '%') append_cstr(&text, "%");
        }
    } else if (!has_error && kind && strchr("dxbXo", kind)) {
        if (working.type != V_INT || precision >= 0) error_at(at, "integer format requires an int and no precision");
        else {
            int64_t n = working.as.integer;
            uint64_t magnitude = n < 0 ? (uint64_t)(-(n + 1)) + 1 : (uint64_t)n;
            unsigned int base = kind == 'b' ? 2u : kind == 'o' ? 8u : kind == 'd' ? 10u : 16u;
            const char *digits = kind == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char reverse[65]; size_t count = 0;
            do { reverse[count++] = digits[magnitude % base]; magnitude /= base; } while (magnitude);
            if (n < 0) append_cstr(&text, "-"); else if (sign == '+' || sign == ' ') append_text(&text, &sign, 1);
            while (count) append_text(&text, &reverse[--count], 1);
        }
    } else if (!has_error) {
        if (kind && kind != 's') error_at(at, "unsupported format type");
        else if (kind == 's' && working.type != V_STR) error_at(at, "'s' format requires str");
        else if (precision >= 0 && working.type != V_STR) error_at(at, "use f, e or g for numeric precision");
        else {
            if ((sign || zero) && !numeric(working)) error_at(at, "sign and zero padding require a number");
            if (numeric(working) && (sign == '+' || sign == ' ') && copysign(1.0, number(working)) > 0) append_text(&text, &sign, 1);
            render_value(&text, working, 0, 0);
            if (precision >= 0 && !has_error) {
                size_t chars = 0, bytes = 0;
                while (bytes < text.count) {
                    if (((unsigned char)text.text[bytes] & 0xc0) != 0x80 && chars++ == (size_t)precision) break;
                    bytes++;
                }
                text.count = bytes; text.text[bytes] = '\0';
            }
        }
    }
    TextBuffer result = {.at = at};
    size_t chars = character_count(text.text ? text.text : "", text.count);
    size_t padding = width > 0 && (size_t)width > chars ? (size_t)width - chars : 0;
    if (!has_error) {
        size_t left = align == '>' ? padding : align == '^' ? padding / 2 : 0;
        size_t skip = 0;
        if (zero && numeric(working)) {
            fill = '0'; left = padding;
            if (text.count && strchr("-+ ", text.text[0])) { append_text(&result, text.text, 1); skip = 1; }
        }
        for (size_t i = 0; i < left; i++) append_text(&result, &fill, 1);
        append_text(&result, text.text ? text.text + skip : "", text.count - skip);
        for (size_t i = left; i < padding; i++) append_text(&result, &fill, 1);
    }
    release(working); free(text.text);
    Value output = has_error ? nothing() : text_value(result.text ? result.text : "", result.count, at);
    free(result.text); return output;
}
#endif

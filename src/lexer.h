#ifndef SIAN_LEXER_H
#define SIAN_LEXER_H

typedef enum {
    T_EOF, T_NEWLINE, T_INDENT, T_DEDENT, T_NAME, T_NUMBER, T_STRING,
    T_LPAREN, T_RPAREN, T_COMMA, T_PLUS, T_MINUS, T_STAR, T_SLASH,
    T_PERCENT, T_BANG, T_EQUAL, T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
    T_DOT, T_HASH, T_LBRACKET, T_RBRACKET
} TokenKind;
typedef struct { TokenKind kind; char *text; Location at; } Token;
typedef struct { Token *items; size_t count, capacity; } TokenList;

static void emit(TokenList *list, TokenKind kind, char *text, Location at) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 256;
        list->items = resize(list->items, list->capacity * sizeof(Token));
    }
    list->items[list->count++] = (Token){kind, text, at};
}

static void lex(const char *text, TokenList *list, Arena *arena) {
    size_t pos = 0;
    int line = 1, column = 1, line_start = 1, code_on_line = 0;
    int block_comment = 0, indent = 0;
    int prefix_done = 0;
    Location comment_start = {0, 0};
    int indents[SYNTAX_LIMIT + 1] = {0};
    size_t depth = 0;
    if (strlen(text) >= 3 && memcmp(text, "\xef\xbb\xbf", 3) == 0) pos = 3;
    while (text[pos] && !has_error) {
        /* Only the physical line's leading whitespace sets its indentation.
           Spaces after an inline block comment cannot create a new block. */
        if (!prefix_done) {
            while (text[pos] == ' ' || text[pos] == '\t') {
                indent += text[pos] == '\t' ? 4 : 1;
                pos++; column++;
            }
            prefix_done = 1;
            if (!text[pos]) break;
        }
        unsigned char ch = (unsigned char)text[pos];
        Location at = {line, column};
        if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && text[pos + 1] == '\n') pos++;
            pos++; line++; column = 1;
            if (code_on_line) emit(list, T_NEWLINE, NULL, at);
            code_on_line = 0; line_start = 1; indent = 0; prefix_done = 0;
            continue;
        }
        if (block_comment) {
            if (ch == '^' && text[pos + 1] == '^') {
                block_comment = 0; pos += 2; column += 2;
            } else { pos++; column++; }
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            pos++; column++;
            continue;
        }
        if (ch == '|' && text[pos + 1] == '|') {
            while (text[pos] && text[pos] != '\n' && text[pos] != '\r') { pos++; column++; }
            continue;
        }
        if (ch == '^' && text[pos + 1] == '^') {
            block_comment = 1; comment_start = at; pos += 2; column += 2;
            continue;
        }
        if (line_start) {
            if (indent > indents[depth]) {
                if (depth == SYNTAX_LIMIT) {
                    error_at(at, "block nesting exceeds %u", SYNTAX_LIMIT); break;
                }
                indents[++depth] = indent;
                emit(list, T_INDENT, NULL, at);
            } else {
                while (indent < indents[depth]) { depth--; emit(list, T_DEDENT, NULL, at); }
                if (indent != indents[depth]) {
                    error_at(at, "indentation does not match an enclosing block"); break;
                }
            }
            line_start = 0;
        }
        code_on_line = 1;
        if (ascii_letter(ch)) {
            size_t begin = pos;
            while (ascii_letter((unsigned char)text[pos]) || ascii_digit((unsigned char)text[pos])) { pos++; column++; }
            if (pos - begin > NAME_LIMIT) {
                error_at(at, "name exceeds %u bytes", NAME_LIMIT); break;
            }
            emit(list, T_NAME, arena_text(arena, text + begin, pos - begin), at);
            continue;
        }
        if (ascii_digit(ch) || (ch == '.' && ascii_digit((unsigned char)text[pos + 1]))) {
            size_t begin = pos;
            while (ascii_digit((unsigned char)text[pos])) { pos++; column++; }
            if (text[pos] == '.') {
                pos++; column++;
                while (ascii_digit((unsigned char)text[pos])) { pos++; column++; }
            }
            if (text[pos] == 'e' || text[pos] == 'E') {
                pos++; column++;
                if (text[pos] == '+' || text[pos] == '-') { pos++; column++; }
                if (!ascii_digit((unsigned char)text[pos])) {
                    error_at(at, "exponent needs digits"); break;
                }
                while (ascii_digit((unsigned char)text[pos])) { pos++; column++; }
            }
            emit(list, T_NUMBER, arena_text(arena, text + begin, pos - begin), at);
            continue;
        }
        if (ch == '"') {
            size_t begin = ++pos, end = begin;
            column++;
            while (text[end] && text[end] != '"' && text[end] != '\n' && text[end] != '\r') {
                if (text[end] == '\\' && text[end + 1] && text[end + 1] != '\n' && text[end + 1] != '\r') end++;
                end++;
            }
            if (text[end] != '"') { error_at(at, "need a closing quote"); break; }
            char *decoded = arena_alloc(arena, end - begin + 1);
            size_t out = 0;
            while (pos < end && !has_error) {
                char value = text[pos++]; column++;
                if (value == '\\') {
                    value = text[pos++]; column++;
                    switch (value) {
                        case 'n': value = '\n'; break;
                        case 'r': value = '\r'; break;
                        case 't': value = '\t'; break;
                        case '\\': case '"': break;
                        default: error_at(at, "unknown string escape \\%c", value); break;
                    }
                }
                decoded[out++] = value;
            }
            pos++; column++;
            emit(list, T_STRING, decoded, at);
            continue;
        }
        TokenKind kind;
        switch (ch) {
            case '(': kind = T_LPAREN; break;
            case ')': kind = T_RPAREN; break;
            case ',': kind = T_COMMA; break;
            case '+': kind = T_PLUS; break;
            case '-': kind = T_MINUS; break;
            case '*': kind = T_STAR; break;
            case '/': kind = T_SLASH; break;
            case '%': kind = T_PERCENT; break;
            case '!': kind = T_BANG; break;
            case '=': kind = T_EQUAL; break;
            case '<': kind = T_LT; break;
            case '>': kind = T_GT; break;
            case '.': kind = T_DOT; break;
            case '#': kind = T_HASH; break;
            case '[': kind = T_LBRACKET; break;
            case ']': kind = T_RBRACKET; break;
            default: error_at(at, "unexpected character (byte 0x%02x)", (unsigned int)ch); return;
        }
        pos++; column++;
        if (text[pos] == '=') {
            TokenKind pair = kind == T_BANG ? T_NE : kind == T_EQUAL ? T_EQ : kind == T_LT ? T_LE : kind == T_GT ? T_GE : kind;
            if (pair != kind) { kind = pair; pos++; column++; }
        }
        emit(list, kind, NULL, at);
    }
    if (block_comment) error_at(comment_start, "need a closing ^^ for block comment");
    if (code_on_line) emit(list, T_NEWLINE, NULL, (Location){line, column});
    while (depth) { depth--; emit(list, T_DEDENT, NULL, (Location){line, column}); }
    emit(list, T_EOF, NULL, (Location){line, column});
}
#endif

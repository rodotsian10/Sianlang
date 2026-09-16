#ifndef SIAN_PARSER_H
#define SIAN_PARSER_H

typedef enum { V_VOID, V_INT, V_FLOAT, V_STR, V_BOOL, V_FUNCTION, V_TUPLE, V_LIST, V_DICT, V_ANY, V_FILE } ValueType;
typedef enum { E_INT, E_FLOAT, E_STRING, E_BOOL, E_NAME, E_CALL, E_UNARY, E_BINARY,
    E_NONE, E_INDEX, E_MEMBER, E_FORMAT, E_TUPLE, E_LIST, E_DICT } ExprKind;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NOT,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_AND, OP_OR } Operator;
typedef struct Expr Expr;
typedef struct ExprList { Expr *value; char *name, *format; int spread; struct ExprList *next; } ExprList;
struct Expr {
    ExprKind kind;
    Location at;
    unsigned int height;
    Operator op;
    char *text;
    int64_t integer;
    double decimal;
    Expr *left, *right;
    ExprList *args;
    size_t argc;
};
typedef enum { S_DECLARE, S_ASSIGN, S_INDEX_ASSIGN, S_RETURN, S_EXPR, S_TRY,
    S_IF, S_WHILE, S_REPEAT, S_FOR, S_FUNCTION, S_BREAK, S_CONTINUE } StatementKind;
typedef struct NameList { char *name; Expr *default_value; int rest; struct NameList *next; } NameList;
typedef struct Statement Statement;
struct Statement {
    StatementKind kind;
    Location at;
    char *name;
    ValueType type;
    Expr *expr, *expr2;
    Statement *body, *otherwise, *next;
    NameList *params;
    size_t parameter_count;
    int alternate_is_if;
};
typedef struct {
    TokenList *tokens;
    Arena *arena;
    size_t pos;
    unsigned int expr_depth, block_depth, loop_depth;
    jmp_buf failure;
} Parser;

static Token *peek(Parser *p) { return &p->tokens->items[p->pos]; }
static int word(Token *t, const char *name) { return t->kind == T_NAME && strcmp(t->text, name) == 0; }
static int accept_token(Parser *p, TokenKind kind) {
    if (peek(p)->kind != kind) return 0;
    p->pos++; return 1;
}
static void syntax_error(Parser *p, Location at, const char *message) {
    error_at(at, "%s", message);
    longjmp(p->failure, 1);
}
static Token *expect(Parser *p, TokenKind kind, const char *message) {
    Token *token = peek(p);
    if (!accept_token(p, kind)) syntax_error(p, token->at, message);
    return token;
}
static ValueType type_named(const char *name) {
    if (!strcmp(name, "int")) return V_INT;
    if (!strcmp(name, "float")) return V_FLOAT;
    if (!strcmp(name, "str")) return V_STR;
    if (!strcmp(name, "bool") || !strcmp(name, "TF")) return V_BOOL;
    if (!strcmp(name, "var")) return V_ANY;
    return V_VOID;
}
static int reserved(const char *name) {
    static const char *names[] = {"if", "else", "while", "loop", "repeat", "for", "in", "def", "func", "f", "function",
        "return", "log", "input", "range", "time", "true", "false", "None", "and", "or", "break", "continue", "try", "catch"};
    if (type_named(name)) return 1;
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++)
        if (!strcmp(name, names[i])) return 1;
    return 0;
}
static char *parse_name(Parser *p) {
    Token *token = expect(p, T_NAME, "expected a variable or function name");
    if (reserved(token->text)) syntax_error(p, token->at, "reserved word cannot be used as a name");
    return token->text;
}
static Expr *new_expr(Parser *p, ExprKind kind, Location at) {
    Expr *expr = arena_alloc(p->arena, sizeof(*expr));
    expr->kind = kind; expr->at = at; expr->height = 1;
    return expr;
}
static void expression_height(Parser *p, Expr *expr, unsigned int child_height) {
    if (child_height >= SYNTAX_LIMIT) syntax_error(p, expr->at, "expression nesting is too deep");
    if (expr->height <= child_height) expr->height = child_height + 1;
}
static Expr *parse_expression(Parser *p, int min_precedence);
static Expr *parse_template(Parser *p, Expr *literal);
static int builtin_named(const char *name) {
    return !strcmp(name, "input") || !strcmp(name, "log") || !strcmp(name, "len") || !strcmp(name, "range") || !strcmp(name, "time") || !strcmp(name, "time.now") ||
        !strcmp(name, "abs") || !strcmp(name, "min") || !strcmp(name, "max") || !strcmp(name, "round") ||
        !strcmp(name, "random") || !strcmp(name, "random.int") || !strcmp(name, "random.float") || !strcmp(name, "random.choice") ||
        !strcmp(name, "open") ||
        (type_named(name) && strcmp(name, "var"));
}

static void parse_arguments(Parser *p, Expr *call, TokenKind end, int parenthesized) {
    ExprList **tail = &call->args;
    int named_seen = 0;
    if (peek(p)->kind != end) {
        do {
            if (peek(p)->kind == end) break;
            if (call->argc == ARG_LIMIT) syntax_error(p, peek(p)->at, "too many function arguments (maximum 256)");
            ExprList *item = arena_alloc(p->arena, sizeof(*item));
            item->spread = accept_token(p, T_HASH);
            if (!item->spread && peek(p)->kind == T_NAME && p->tokens->items[p->pos + 1].kind == T_EQUAL) {
                item->name = peek(p)->text; p->pos += 2; named_seen = 1;
                for (ExprList *prev = call->args; prev; prev = prev->next)
                    if (prev->name && !strcmp(prev->name, item->name)) syntax_error(p, peek(p)->at, "duplicate named argument");
            } else if (named_seen && !item->spread) syntax_error(p, peek(p)->at, "positional argument follows named argument");
            item->value = parse_expression(p, 0);
            if (call->left->kind == E_NAME && !strcmp(call->left->text, "log.f") && !item->name && item->value->kind == E_STRING)
                item->value = parse_template(p, item->value);
            expression_height(p, call, item->value->height);
            *tail = item; tail = &item->next; call->argc++;
        } while (accept_token(p, T_COMMA));
    }
    if (parenthesized) expect(p, end, "need a closing parenthesis for the call");
}

static Expr *parse_atom(Parser *p) {
    Token *token = peek(p);
    if (accept_token(p, T_MINUS) || accept_token(p, T_PLUS) || accept_token(p, T_BANG)) {
        /* Permit the smallest int64 literal without overflowing its positive magnitude. */
        if (token->kind == T_MINUS && peek(p)->kind == T_NUMBER &&
            !strcmp(peek(p)->text, "9223372036854775808")) {
            p->pos++;
            Expr *expr = new_expr(p, E_INT, token->at); expr->integer = INT64_MIN; return expr;
        }
        Expr *expr = new_expr(p, E_UNARY, token->at);
        expr->op = token->kind == T_MINUS ? OP_SUB : token->kind == T_PLUS ? OP_ADD : OP_NOT;
        expr->right = parse_expression(p, 6);
        expression_height(p, expr, expr->right->height);
        return expr;
    }
    if (accept_token(p, T_LPAREN)) {
        Expr *expr = new_expr(p, E_TUPLE, token->at); ExprList **tail = &expr->args;
        if (!accept_token(p, T_RPAREN)) {
            do { ExprList *item = arena_alloc(p->arena, sizeof(*item)); item->value = parse_expression(p, 0); *tail = item; tail = &item->next; expr->argc++; }
            while (accept_token(p, T_COMMA) && peek(p)->kind != T_RPAREN);
            expect(p, T_RPAREN, "need a closing parenthesis");
        }
        if (expr->argc == 1) { Expr *single = expr->args->value; return single; }
        return expr;
    }
    if (accept_token(p, T_STRING)) {
        Expr *expr = new_expr(p, E_STRING, token->at); expr->text = token->text; return expr;
    }
    if (accept_token(p, T_NUMBER)) {
        char *end;
        errno = 0;
        int decimal = strpbrk(token->text, ".eE") != NULL;
        Expr *expr = new_expr(p, decimal ? E_FLOAT : E_INT, token->at);
        if (decimal) expr->decimal = strtod(token->text, &end);
        else expr->integer = strtoll(token->text, &end, 10);
        if (errno == ERANGE || *end || (decimal && !isfinite(expr->decimal)))
            syntax_error(p, token->at, "number is outside the supported range");
        return expr;
    }
    if (accept_token(p, T_LBRACKET)) {
        Expr *expr = new_expr(p, E_LIST, token->at); ExprList **tail = &expr->args;
        if (!accept_token(p, T_RBRACKET)) {
            do { ExprList *item = arena_alloc(p->arena, sizeof(*item)); item->value = parse_expression(p, 0); *tail = item; tail = &item->next; expr->argc++; }
            while (accept_token(p, T_COMMA) && peek(p)->kind != T_RBRACKET);
            expect(p, T_RBRACKET, "need a closing bracket");
        }
        return expr;
    }
    if (accept_token(p, T_LBRACE)) {
        Expr *expr = new_expr(p, E_DICT, token->at); ExprList **tail = &expr->args;
        if (!accept_token(p, T_RBRACE)) {
            do {
                ExprList *key = arena_alloc(p->arena, sizeof(*key));
                key->value = parse_expression(p, 0); *tail = key; tail = &key->next;
                expect(p, T_COLON, "dictionary entry needs ':'");
                ExprList *value = arena_alloc(p->arena, sizeof(*value));
                value->value = parse_expression(p, 0); *tail = value; tail = &value->next; expr->argc++;
            } while (accept_token(p, T_COMMA) && peek(p)->kind != T_RBRACE);
            expect(p, T_RBRACE, "need a closing brace");
        }
        return expr;
    }
    if (accept_token(p, T_NAME)) {
        if (word(token, "None")) return new_expr(p, E_NONE, token->at);
        if (word(token, "true") || word(token, "false")) {
            Expr *expr = new_expr(p, E_BOOL, token->at); expr->integer = word(token, "true"); return expr;
        }
        int callable_builtin = builtin_named(token->text);
        if (reserved(token->text) && !callable_builtin)
            syntax_error(p, token->at, "expected an expression, found a reserved word");
        Expr *expr = new_expr(p, E_NAME, token->at); expr->text = token->text;
        return expr;
    }
    syntax_error(p, token->at, "expected an expression");
    return NULL;
}

static Expr *parse_prefix(Parser *p) {
    Expr *expr = parse_atom(p);
    for (;;) {
        if (accept_token(p, T_LPAREN)) {
            Expr *call = new_expr(p, E_CALL, expr->at); call->left = expr;
            expression_height(p, call, expr->height);
            parse_arguments(p, call, T_RPAREN, 1); expr = call;
        } else if (accept_token(p, T_LBRACKET)) {
            Expr *index = new_expr(p, E_INDEX, expr->at); index->left = expr;
            index->right = parse_expression(p, 0);
            expect(p, T_RBRACKET, "need a closing bracket");
            expression_height(p, index, expr->height); expression_height(p, index, index->right->height);
            expr = index;
        } else if (accept_token(p, T_DOT)) {
            Token *member = expect(p, T_NAME, "expected a member name after '.'");
            if (expr->kind == E_NAME && !strcmp(expr->text, "log") && !strcmp(member->text, "f")) {
                expr->text = arena_text(p->arena, "log.f", 5);
            } else if (expr->kind == E_NAME && !strcmp(expr->text, "time") && !strcmp(member->text, "now")) {
                expr->text = arena_text(p->arena, "time.now", 8);
            } else if (expr->kind == E_NAME && !strcmp(expr->text, "random") && !strcmp(member->text, "int")) {
                expr->text = arena_text(p->arena, "random.int", 10);
            } else if (expr->kind == E_NAME && !strcmp(expr->text, "random") && !strcmp(member->text, "float")) {
                expr->text = arena_text(p->arena, "random.float", 12);
            } else if (expr->kind == E_NAME && !strcmp(expr->text, "random") && !strcmp(member->text, "choice")) {
                expr->text = arena_text(p->arena, "random.choice", 13);
            } else {
                Expr *access = new_expr(p, E_MEMBER, expr->at); access->left = expr; access->text = member->text;
                expression_height(p, access, expr->height); expr = access;
            }
        } else break;
    }
    return expr;
}

static int operator_info(Token *token, Operator *op) {
    if (word(token, "or")) { *op = OP_OR; return 1; }
    if (word(token, "and")) { *op = OP_AND; return 2; }
    switch (token->kind) {
        case T_EQ: *op = OP_EQ; return 3;
        case T_NE: *op = OP_NE; return 3;
        case T_LT: *op = OP_LT; return 3;
        case T_LE: *op = OP_LE; return 3;
        case T_GT: *op = OP_GT; return 3;
        case T_GE: *op = OP_GE; return 3;
        case T_PLUS: *op = OP_ADD; return 4;
        case T_MINUS: *op = OP_SUB; return 4;
        case T_STAR: *op = OP_MUL; return 5;
        case T_SLASH: *op = OP_DIV; return 5;
        case T_PERCENT: *op = OP_MOD; return 5;
        default: return 0;
    }
}
static Expr *parse_expression(Parser *p, int min_precedence) {
    if (++p->expr_depth > SYNTAX_LIMIT) syntax_error(p, peek(p)->at, "expression nesting is too deep");
    Expr *left = parse_prefix(p);
    int compared = 0;
    for (;;) {
        Operator op = OP_ADD;
        Token *token = peek(p);
        int precedence = operator_info(token, &op);
        if (!precedence || precedence < min_precedence) break;
        if (precedence == 3 && compared) syntax_error(p, token->at, "chained comparisons are unsupported; join comparisons with 'and'");
        if (precedence == 3) compared = 1;
        p->pos++;
        Expr *expr = new_expr(p, E_BINARY, token->at);
        expr->op = op; expr->left = left;
        expr->right = parse_expression(p, precedence + 1);
        expression_height(p, expr, left->height);
        expression_height(p, expr, expr->right->height);
        left = expr;
    }
    p->expr_depth--;
    return left;
}

static Statement *parse_statement(Parser *p);
static Statement *parse_suite(Parser *p) {
    expect(p, T_NEWLINE, "unexpected text after statement");
    expect(p, T_INDENT, "expected an indented, nonempty block");
    p->block_depth++;
    Statement *head = NULL, **tail = &head;
    while (peek(p)->kind != T_DEDENT && peek(p)->kind != T_EOF) {
        *tail = parse_statement(p); tail = &(*tail)->next;
    }
    if (!head) syntax_error(p, peek(p)->at, "block must contain a statement");
    expect(p, T_DEDENT, "expected end of block");
    p->block_depth--;
    return head;
}
static Statement *new_statement(Parser *p, StatementKind kind, Location at) {
    Statement *statement = arena_alloc(p->arena, sizeof(*statement));
    statement->kind = kind; statement->at = at; return statement;
}
static Statement *parse_statement(Parser *p) {
    Token *token = peek(p);
    if (token->kind == T_INDENT) syntax_error(p, token->at, "unexpected indentation");
    if (word(token, "def") || word(token, "func") || word(token, "f") || word(token, "function")) {
        p->pos++;
        Statement *s = new_statement(p, S_FUNCTION, token->at);
        s->name = parse_name(p);
        expect(p, T_LPAREN, "expected '(' after function name");
        NameList **tail = &s->params;
        int default_seen = 0, rest_seen = 0;
        if (!accept_token(p, T_RPAREN)) {
            do {
                if (s->parameter_count == ARG_LIMIT) syntax_error(p, peek(p)->at, "too many parameters (maximum 256)");
                if (rest_seen) syntax_error(p, peek(p)->at, "#variadic parameter must be last");
                int rest = accept_token(p, T_HASH);
                char *name = parse_name(p);
                for (NameList *n = s->params; n; n = n->next)
                    if (!strcmp(name, n->name)) syntax_error(p, peek(p)->at, "duplicate parameter name");
                NameList *item = arena_alloc(p->arena, sizeof(*item)); item->name = name; item->rest = rest;
                if (accept_token(p, T_EQUAL)) {
                    if (rest) syntax_error(p, peek(p)->at, "#variadic parameter cannot have a default");
                    item->default_value = parse_expression(p, 0); default_seen = 1;
                } else if (default_seen && !rest) syntax_error(p, peek(p)->at, "required parameter follows a default parameter");
                rest_seen = rest;
                *tail = item; tail = &item->next; s->parameter_count++;
            } while (accept_token(p, T_COMMA) && peek(p)->kind != T_RPAREN);
            expect(p, T_RPAREN, "need a closing parenthesis for parameters");
        }
        unsigned int saved_loop = p->loop_depth;
        p->loop_depth = 0; s->body = parse_suite(p); p->loop_depth = saved_loop;
        return s;
    }
    if (word(token, "try")) {
        p->pos++;
        Statement *s = new_statement(p, S_TRY, token->at);
        s->body = parse_suite(p);
        if (!word(peek(p), "catch")) syntax_error(p, peek(p)->at, "try requires a catch block");
        p->pos++;
        if (peek(p)->kind == T_NAME) s->name = parse_name(p);
        s->otherwise = parse_suite(p);
        return s;
    }
    if (word(token, "if")) {
        p->pos++;
        Statement *head = new_statement(p, S_IF, token->at), *branch = head;
        for (;;) {
            branch->expr = parse_expression(p, 0);
            branch->body = parse_suite(p);
            if (!word(peek(p), "else")) break;
            p->pos++;
            if (word(peek(p), "if")) {
                Location at = peek(p)->at; p->pos++;
                branch->otherwise = new_statement(p, S_IF, at);
                branch->alternate_is_if = 1;
                branch = branch->otherwise;
            } else {
                branch->otherwise = parse_suite(p); break;
            }
        }
        return head;
    }
    if (word(token, "for")) {
        p->pos++;
        Statement *s = new_statement(p, S_FOR, token->at);
        s->name = parse_name(p);
        if (!word(peek(p), "in")) syntax_error(p, peek(p)->at, "for requires 'in'");
        p->pos++; s->expr = parse_expression(p, 0);
        p->loop_depth++; s->body = parse_suite(p); p->loop_depth--;
        if (word(peek(p), "else")) { p->pos++; s->otherwise = parse_suite(p); }
        return s;
    }
    if (word(token, "while") || word(token, "loop") || word(token, "repeat")) {
        p->pos++;
        Statement *s = new_statement(p, word(token, "repeat") ? S_REPEAT : S_WHILE, token->at);
        s->expr = parse_expression(p, 0);
        p->loop_depth++; s->body = parse_suite(p); p->loop_depth--;
        if (word(peek(p), "else")) { p->pos++; s->otherwise = parse_suite(p); }
        return s;
    }
    Statement *s;
    if (word(token, "log")) {
        p->pos++;
        Expr *callee = new_expr(p, E_NAME, token->at); callee->text = token->text;
        if (accept_token(p, T_DOT)) {
            if (!word(peek(p), "f")) syntax_error(p, peek(p)->at, "expected log.f");
            p->pos++; callee->text = arena_text(p->arena, "log.f", 5);
        }
        Expr *call = new_expr(p, E_CALL, token->at); call->left = callee;
        int command_group = 0;
        if (peek(p)->kind == T_LPAREN && peek(p)->at.column > token->at.column + (int)strlen(callee->text)) {
            size_t scan = p->pos; int depth = 0;
            do {
                if (p->tokens->items[scan].kind == T_LPAREN) depth++;
                if (p->tokens->items[scan].kind == T_RPAREN) depth--;
                scan++;
            } while (scan < p->tokens->count && depth && p->tokens->items[scan].kind != T_NEWLINE);
            Operator ignored;
            if (!depth && scan < p->tokens->count && operator_info(&p->tokens->items[scan], &ignored)) command_group = 1;
        }
        int parens = !command_group && accept_token(p, T_LPAREN);
        parse_arguments(p, call, parens ? T_RPAREN : T_NEWLINE, parens);
        s = new_statement(p, S_EXPR, token->at); s->expr = call;
    } else if (word(token, "return")) {
        p->pos++;
        s = new_statement(p, S_RETURN, token->at);
        if (peek(p)->kind != T_NEWLINE) s->expr = parse_expression(p, 0);
    } else if (word(token, "break") || word(token, "continue")) {
        if (!p->loop_depth) syntax_error(p, token->at, "break/continue requires an enclosing loop");
        p->pos++;
        s = new_statement(p, word(token, "break") ? S_BREAK : S_CONTINUE, token->at);
    } else if (token->kind == T_NAME && type_named(token->text) && p->tokens->items[p->pos + 1].kind != T_LPAREN) {
        p->pos++;
        s = new_statement(p, S_DECLARE, token->at); s->type = type_named(token->text);
        s->name = parse_name(p);
        expect(p, T_EQUAL, "expected '=' after variable name");
        s->expr = parse_expression(p, 0);
    } else if (token->kind == T_NAME && p->tokens->items[p->pos + 1].kind == T_LBRACKET) {
        /* Look ahead for name[...] = expr (index assignment) */
        size_t scan = p->pos + 2; int depth = 1;
        while (scan < p->tokens->count && depth > 0) {
            if (p->tokens->items[scan].kind == T_LBRACKET) depth++;
            if (p->tokens->items[scan].kind == T_RBRACKET) depth--;
            scan++;
        }
        if (depth == 0 && scan < p->tokens->count && p->tokens->items[scan].kind == T_EQUAL
            && (scan + 1 >= p->tokens->count || p->tokens->items[scan + 1].kind != T_EQUAL)) {
            /* index assignment: parse name[key] as expression then consume '=' and RHS */
            s = new_statement(p, S_INDEX_ASSIGN, token->at);
            s->expr = parse_expression(p, 0); /* evaluates name[key] → E_INDEX node */
            expect(p, T_EQUAL, "expected '=' in index assignment");
            s->expr2 = parse_expression(p, 0);
        } else {
            s = new_statement(p, S_EXPR, token->at);
            s->expr = parse_expression(p, 0);
            if (s->expr->kind != E_CALL) syntax_error(p, token->at, "expected a declaration, assignment, or function call");
        }
    } else if (token->kind == T_NAME && p->tokens->items[p->pos + 1].kind == T_EQUAL) {
        s = new_statement(p, S_ASSIGN, token->at); s->name = parse_name(p);
        p->pos++; s->expr = parse_expression(p, 0);
    } else {
        s = new_statement(p, S_EXPR, token->at);
        s->expr = parse_expression(p, 0);
        if (s->expr->kind != E_CALL) syntax_error(p, token->at, "expected a declaration, assignment, or function call");
    }
    expect(p, T_NEWLINE, "unexpected text after statement");
    return s;
}

static Expr *parse_fragment(Parser *outer, const char *text, size_t length, Location at) {
    TokenList tokens = {0};
    char *copy = arena_text(outer->arena, text, length);
    lex(copy, &tokens, outer->arena);
    for (size_t i = 0; i < tokens.count; i++) {
        tokens.items[i].at.line += at.line - 1;
        tokens.items[i].at.column += at.column;
    }
    Parser nested = {0}; nested.tokens = &tokens; nested.arena = outer->arena;
    Expr *result = NULL;
    if (!has_error && !setjmp(nested.failure)) {
        result = parse_expression(&nested, 0);
        expect(&nested, T_NEWLINE, "unexpected text in formatting expression");
        expect(&nested, T_EOF, "formatting expression must be on one line");
    }
    free(tokens.items);
    if (has_error) longjmp(outer->failure, 1);
    return result;
}

static unsigned int format_parse_depth;
static Expr *parse_template(Parser *p, Expr *literal) {
    if (++format_parse_depth > SYNTAX_LIMIT) syntax_error(p, literal->at, "formatting nesting is too deep");
    Expr *format = new_expr(p, E_FORMAT, literal->at);
    ExprList **tail = &format->args;
    const char *text = literal->text;
    size_t length = strlen(text), pos = 0, used = 0;
    char *plain = arena_alloc(p->arena, length + 1);
    while (pos < length) {
        if ((text[pos] == '{' || text[pos] == '}') && text[pos + 1] == text[pos]) {
            plain[used++] = text[pos]; pos += 2; continue;
        }
        if (text[pos] == '}') syntax_error(p, literal->at, "unmatched '}' in log.f; use '}}' for a literal brace");
        if (text[pos] != '{') { plain[used++] = text[pos++]; continue; }
        if (used) {
            ExprList *part = arena_alloc(p->arena, sizeof(*part));
            part->value = new_expr(p, E_STRING, literal->at);
            part->value->text = arena_text(p->arena, plain, used);
            *tail = part; tail = &part->next; used = 0;
        }
        size_t begin = ++pos, spec = length;
        int parens = 0, brackets = 0, quote = 0;
        for (; pos < length; pos++) {
            char ch = text[pos];
            if (quote) {
                if (ch == '\\' && pos + 1 < length) pos++;
                else if (ch == '"') quote = 0;
                continue;
            }
            if (ch == '"') { quote = 1; continue; }
            if (ch == '(') parens++;
            if (ch == ')') parens--;
            if (ch == '[') brackets++;
            if (ch == ']') brackets--;
            if (!parens && !brackets && ch == ':' && spec == length) spec = pos;
            if (!parens && !brackets && ch == '!' && (text[pos + 1] == 'r' || text[pos + 1] == 's') && spec == length) spec = pos;
            if (!parens && !brackets && ch == '}') break;
        }
        if (pos == length) syntax_error(p, literal->at, "need a closing '}' in log.f");
        size_t expression_end = spec == length ? pos : spec;
        ExprList *part = arena_alloc(p->arena, sizeof(*part));
        part->value = parse_fragment(p, text + begin, expression_end - begin,
            (Location){literal->at.line, literal->at.column + (int)begin});
        if (spec != length) part->format = arena_text(p->arena, text + spec, pos - spec);
        expression_height(p, format, part->value->height);
        *tail = part; tail = &part->next; pos++;
    }
    if (used || !format->args) {
        ExprList *part = arena_alloc(p->arena, sizeof(*part));
        part->value = new_expr(p, E_STRING, literal->at); part->value->text = arena_text(p->arena, plain, used);
        *tail = part;
    }
    format_parse_depth--;
    return format;
}

static Statement *parse_program(TokenList *tokens, Arena *arena) {
    Parser p = {0}; p.tokens = tokens; p.arena = arena;
    if (setjmp(p.failure)) return NULL;
    Statement *head = NULL, **tail = &head;
    while (peek(&p)->kind != T_EOF) {
        Statement *s = parse_statement(&p);
        if (s->kind == S_FUNCTION) {
            for (Statement *prev = head; prev; prev = prev->next)
                if (prev->kind == S_FUNCTION && !strcmp(prev->name, s->name))
                    syntax_error(&p, s->at, "duplicate function definition");
        }
        *tail = s; tail = &s->next;
    }
    return head;
}
#endif

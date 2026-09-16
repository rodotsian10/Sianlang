#ifndef SIAN_PARSER_H
#define SIAN_PARSER_H

typedef enum { V_VOID, V_INT, V_FLOAT, V_STR, V_BOOL } ValueType;
typedef enum { E_INT, E_FLOAT, E_STRING, E_BOOL, E_NAME, E_CALL, E_UNARY, E_BINARY } ExprKind;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NOT,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_AND, OP_OR } Operator;
typedef struct Expr Expr;
typedef struct ExprList { Expr *value; struct ExprList *next; } ExprList;
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
typedef enum { S_DECLARE, S_ASSIGN, S_LOG, S_ERROR, S_RETURN, S_EXPR,
    S_IF, S_WHILE, S_REPEAT, S_FUNCTION, S_BREAK, S_CONTINUE } StatementKind;
typedef struct NameList { char *name; struct NameList *next; } NameList;
typedef struct Statement Statement;
struct Statement {
    StatementKind kind;
    Location at;
    char *name;
    ValueType type;
    Expr *expr;
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
    return V_VOID;
}
static int reserved(const char *name) {
    static const char *names[] = {"if", "else", "while", "loop", "repeat", "def", "func", "f", "function",
        "return", "log", "input", "iferror", "true", "false", "and", "or", "break", "continue"};
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

static Expr *parse_prefix(Parser *p) {
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
        Expr *expr = parse_expression(p, 0);
        expect(p, T_RPAREN, "need a closing parenthesis");
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
    if (accept_token(p, T_NAME)) {
        if (word(token, "true") || word(token, "false")) {
            Expr *expr = new_expr(p, E_BOOL, token->at); expr->integer = word(token, "true"); return expr;
        }
        int callable_builtin = word(token, "input") || type_named(token->text);
        if (reserved(token->text) && !callable_builtin)
            syntax_error(p, token->at, "expected an expression, found a reserved word");
        Expr *expr = new_expr(p, E_NAME, token->at); expr->text = token->text;
        if (accept_token(p, T_LPAREN)) {
            expr->kind = E_CALL;
            ExprList **tail = &expr->args;
            if (!accept_token(p, T_RPAREN)) {
                do {
                    if (expr->argc == ARG_LIMIT) syntax_error(p, peek(p)->at, "too many function arguments (maximum 256)");
                    ExprList *item = arena_alloc(p->arena, sizeof(*item));
                    item->value = parse_expression(p, 0);
                    expression_height(p, expr, item->value->height);
                    *tail = item; tail = &item->next; expr->argc++;
                } while (accept_token(p, T_COMMA));
                expect(p, T_RPAREN, "need a closing parenthesis for the call");
            }
        } else if (callable_builtin) syntax_error(p, token->at, "builtin function needs parentheses");
        return expr;
    }
    syntax_error(p, token->at, "expected an expression");
    return NULL;
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
        if (p->block_depth) syntax_error(p, token->at, "functions must be defined at the top level");
        p->pos++;
        Statement *s = new_statement(p, S_FUNCTION, token->at);
        s->name = parse_name(p);
        expect(p, T_LPAREN, "expected '(' after function name");
        NameList **tail = &s->params;
        if (!accept_token(p, T_RPAREN)) {
            do {
                if (s->parameter_count == ARG_LIMIT) syntax_error(p, peek(p)->at, "too many parameters (maximum 256)");
                char *name = parse_name(p);
                for (NameList *n = s->params; n; n = n->next)
                    if (!strcmp(name, n->name)) syntax_error(p, peek(p)->at, "duplicate parameter name");
                NameList *item = arena_alloc(p->arena, sizeof(*item)); item->name = name;
                *tail = item; tail = &item->next; s->parameter_count++;
            } while (accept_token(p, T_COMMA));
            expect(p, T_RPAREN, "need a closing parenthesis for parameters");
        }
        s->body = parse_suite(p);
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
    if (word(token, "while") || word(token, "loop") || word(token, "repeat")) {
        p->pos++;
        Statement *s = new_statement(p, word(token, "repeat") ? S_REPEAT : S_WHILE, token->at);
        s->expr = parse_expression(p, 0);
        p->loop_depth++; s->body = parse_suite(p); p->loop_depth--;
        return s;
    }
    Statement *s;
    if (word(token, "log") || word(token, "iferror") || word(token, "return")) {
        p->pos++;
        s = new_statement(p, word(token, "log") ? S_LOG : word(token, "return") ? S_RETURN : S_ERROR, token->at);
        if (s->kind != S_RETURN || peek(p)->kind != T_NEWLINE) s->expr = parse_expression(p, 0);
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

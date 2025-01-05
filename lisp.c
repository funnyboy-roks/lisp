#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

#define DBG(...) do {                                     \
    fprintf(stderr, "[DBG] %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                         \
    fprintf(stderr, "\n");                                \
} while (0);

#ifdef DEBUG
#define ERROR(...) do {                                                                       \
    fprintf(stderr, "[ERROR] (%s:%d) %s:%ld:%ld: ", __FILE__, __LINE__, file_name, col, line); \
    fprintf(stderr, __VA_ARGS__);                                                             \
    fprintf(stderr, "\n");                                                                    \
    exit(1);                                                                                  \
} while (0);
#else // DEBUG
#define ERROR(...) do {                                            \
    fprintf(stderr, "[ERROR] %s:%ld:%ld: ", file_name, col, line); \
    fprintf(stderr, __VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                         \
    exit(1);                                                       \
} while (0);
#endif // DEBUG

#define PANIC(...) do {                                     \
    fprintf(stderr, "[PANIC] %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                           \
    fprintf(stderr, "\n");                                  \
    exit(1);                                                \
} while (0);

#define da_append(da, item)  do {                                                \
    if ((da)->count >= (da)->capacity) {                                         \
        (da)->capacity = (da)->capacity == 0 ? 16 : (da)->capacity*2;            \
        (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
        assert((da)->items != NULL && "Buy more RAM lol");                       \
    }                                                                            \
                                                                                 \
    (da)->items[(da)->count++] = (item);                                         \
} while(0);

typedef enum {
    TK_EOF = 0,

    // tokens
    TK_LPAREN,
    TK_RPAREN,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_EQUALS,

    // keywords
    TK_LET,
    TK_IF,
    TK_TRUE,
    TK_FALSE,
    TK_EVAL,
    TK_FUNCTION,

    // literals
    TK_INT,
    TK_IDENT,
    TK_STRING,

    __TK_LENGTH,
} TokenKind;

typedef struct {
    const char *data; 
    size_t length;
} StringSlice;

typedef union {
    int integer;
    const char *ident;
    StringSlice string;
} TokenValue;

typedef struct {
    TokenKind kind;
    TokenValue value;
} Token;

int fpeek(FILE *file)
{
    int out = getc(file);
    if (out != EOF) {
        ungetc(out, file);
    }
    return out;
}

size_t col = 1;
size_t line = 1;
const char *file_name;

int ftake(FILE *file)
{
    int c = fgetc(file);
    col += 1;
    if (c == '\n') {
        col = 1;
        line += 1;
    }
    return c;
}

int hex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    c = c & ~0b0100000; // force upper case
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

int take_int(FILE *file, char leading)
{
    int number = 0;
    int nc;
    if (leading == '0') {
        int kind = ftake(file);
        if (kind == EOF) return 0;
        if (kind == 'x') {
            int digit;
            while ((nc = fpeek(file)) != EOF && (digit = hex(nc)) != -1) {
                number *= 16;
                number += digit;
                ftake(file);
            }
            if (isalnum(nc)) {
                ERROR("Unexpected character '%c'", nc);
            }
            return number;
        } else if (kind == 'b') {
            while ((nc = fpeek(file)) != EOF && (nc == '0' || nc == '1')) {
                number *= 2;
                number += (nc == '1');
                ftake(file);
            }
            if (isalnum(nc)) {
                ERROR("Unexpected character '%c'", nc);
            }
            return number;
        } else {
            ungetc(kind, file);
        }
    }
    number = leading - '0';
    while ((nc = fpeek(file)) != EOF && isdigit(nc)) {
        number *= 10;
        number += nc - '0';
        ftake(file);
    }
    if (isalnum(nc)) {
        ERROR("Unexpected character '%c'", nc);
    }
    return number;
}

#define MAX_IDENT_LEN 255
const char *take_ident(FILE *file) {
    char buf[MAX_IDENT_LEN + 1] = {0};
    size_t n = 0;
    int nc;
    while ((nc = fpeek(file)) != EOF && (isalnum(nc) || nc == '_')) {
        if (n >= MAX_IDENT_LEN) ERROR("Ident must be at most %d characters.", MAX_IDENT_LEN);
        buf[n++] = nc;
        ftake(file);
    }
    return strdup(buf);
}

TokenKind keyword_from_ident(const char *ident) {
    if (!strcmp("let", ident)) return TK_LET;
    if (!strcmp("if", ident)) return TK_IF;
    if (!strcmp("true", ident)) return TK_TRUE;
    if (!strcmp("false", ident)) return TK_FALSE;
    if (!strcmp("eval", ident)) return TK_EVAL;
    if (!strcmp("function", ident)) return TK_FUNCTION;
    return TK_IDENT;
}

StringSlice take_string(FILE *file, char quote) {
    char nc;
    size_t cap = 256;
    char *string = malloc(cap);
    size_t len = 0;

    while((nc = ftake(file)) != EOF && nc != quote) {
        if (nc == '\\') {
            nc = ftake(file);
            if (nc == 'n') {
                nc = '\n';
            }
            if (nc == EOF) break;
        }
        if (len >= cap - 1) {
            string = realloc(string, cap *= 2);
        }
        string[len++] = nc;
    }

    if (nc == EOF) {
        ERROR("Expected string terminator, found EOF.");
    }
    string[len] = '\0';

    return (StringSlice) {
        .data = string,
        .length = len,
    };
}

Token next_token(FILE *file)
{
    for (;;) {
        int c = ftake(file);
        if (c == EOF) {
            return (Token) {
                .kind = TK_EOF,
            };
        }

        switch (c) {
            case '(': {
                return (Token) {
                    .kind = TK_LPAREN,
                };
            } break;
            case ')': {
                return (Token) {
                    .kind = TK_RPAREN,
                };
            } break;
            case '+': {
                return (Token) {
                    .kind = TK_PLUS,
                };
            } break;
            case '*': {
                return (Token) {
                    .kind = TK_STAR,
                };
            } break;
            case '=': {
                return (Token) {
                    .kind = TK_EQUALS,
                };
            } break;
            case '/': {
                return (Token) {
                    .kind = TK_SLASH,
                };
            } break;
            case '-': {
                int nc = fpeek(file);
                if (nc != EOF && isdigit(nc)) {
                    getc(file);
                    int n = take_int(file, nc);
                    return (Token) {
                        .kind = TK_INT,
                        .value = {
                            .integer = -n,
                        }
                    };
                }
                return (Token) {
                    .kind = TK_MINUS,
                };
            } break;
            case '\'':
            case '"': {
                StringSlice string = take_string(file, c);
                return (Token) {
                    .kind = TK_STRING,
                    .value = {
                        .string = string,
                    }
                };
            } break;
            case ';': {
                int nc;
                while((nc = ftake(file)) != EOF && nc != '\n');
                continue;
            } break;
        }

        if (isdigit(c)) {
            int number = take_int(file, c);
            return (Token) {
                .kind = TK_INT,
                .value = {
                    .integer = number,
                },
            };
        }

        if (isalpha(c) || c == '_') {
            ungetc(c, file);
            const char *ident = take_ident(file);
            return (Token) {
                .kind = keyword_from_ident(ident),
                .value = {
                    .ident = ident,
                },
            };
        }

        if (isspace(c)) {
            continue;
        }

        ERROR("Unexpected token '%c'", c);
    }
}

const char *tk_names[] = {
    [TK_EOF] = "EOF",

    [TK_LPAREN] = "LPAREN",
    [TK_RPAREN] = "RPAREN",
    [TK_PLUS] = "PLUS",
    [TK_MINUS] = "MINUS",
    [TK_STAR] = "STAR",
    [TK_SLASH] = "SLASH",
    [TK_EQUALS] = "EQUALS",

    [TK_LET] = "LET",
    [TK_IF] = "IF",
    [TK_TRUE] = "TRUE",
    [TK_FALSE] = "FALSE",
    [TK_EVAL] = "EVAL",
    [TK_FUNCTION] = "FUNCTION",

    [TK_INT] = "INT",
    [TK_IDENT] = "IDENT",
    [TK_STRING] = "STRING",
};

static_assert(sizeof(tk_names) / sizeof(*tk_names) == __TK_LENGTH, "Missing names for tokens");

#define SBUF_LEN 256
char sbuf[SBUF_LEN] = {0}; 
const char *token_string(Token tok)
{
    int n = snprintf(sbuf, SBUF_LEN, "%s", tk_names[tok.kind]);
    switch (tok.kind) {
    case TK_EOF:
        break;

    case TK_LPAREN:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '('");
        break;
    case TK_RPAREN:
        n += snprintf(sbuf + n, SBUF_LEN - n, " ')'");
        break;
    case TK_PLUS:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '+'");
        break;
    case TK_MINUS:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '-'");
        break;
    case TK_STAR:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '*'");
        break;
    case TK_SLASH:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '/'");
        break;
    case TK_EQUALS:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '='");
        break;

    case TK_LET:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'let'");
        break;
    case TK_IF:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'if'");
        break;
    case TK_TRUE:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'true'");
        break;
    case TK_FALSE:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'false'");
        break;
    case TK_FUNCTION:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'eval'");
        break;
    case TK_EVAL:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'eval'");
        break;

    case TK_INT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " %d", tok.value.integer);
        break;
    case TK_IDENT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '%s'", tok.value.ident);
        break;
    case TK_STRING:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '%s'", tok.value.string.data);
        break;

    case __TK_LENGTH:
        PANIC("unreachable");
    }
    return sbuf;
}

typedef struct AST AST;

typedef enum {
    EK_ATOM = 0,
    EK_FUNCTION_CALL,
    EK_FUNCTION_DEF,
    EK_IF,
    EK_DECLARE_VAR,
    EK_ASSIGN_VAR,
} ExpressionKind;

typedef struct {
    AST *items;
    size_t count;
    size_t capacity;
} ASTList;

typedef struct {
    Token op;
    ASTList args;
} FunctionCallValue;

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} ParamList;

typedef struct {
    const char *name;
    ParamList params;
    AST *body;
} FunctionDefValue;

typedef struct {
    AST *cond;
    AST *true_branch;
    AST *false_branch; // optional
} IfValue;

typedef struct {
    const char *name;
    AST *value; // optional for declare
} DeclareAssign;

typedef struct {
    Token atom;
    FunctionCallValue fn_call;
    FunctionDefValue fn_def;
    IfValue if_;
    DeclareAssign declare_assign;
} ASTValue;

typedef struct AST {
    ExpressionKind kind;
    ASTValue value;
} AST;

static Token peeked;
static bool have_peeked;
Token take_token(FILE *file) {
    if (have_peeked) {
        have_peeked = false;
        return peeked;
    }
    return next_token(file);
}

Token *peek_token(FILE *file) {
    if (have_peeked) return &peeked;
    peeked = next_token(file);
    have_peeked = true;
    return &peeked; 
}

Token expect_token(FILE *file, TokenKind kind) {
    Token tok = take_token(file);
    if (tok.kind != kind) ERROR("Expocted token %s, found %s.", tk_names[kind], tk_names[tok.kind]);
    return tok;
}

bool take_token_if(FILE *file, TokenKind kind, Token *out) {
    Token *peek = peek_token(file);
    if (peek->kind != kind) return false;
    Token t = take_token(file);
    if (out != NULL) *out = t;
    return true;
}

bool is_function_token(TokenKind kind) {
    switch (kind) {
    case TK_EOF:
    case TK_LPAREN:
    case TK_RPAREN:
    case TK_INT:
    case TK_STRING:
    case TK_IF:
    case TK_TRUE:
    case TK_FALSE:
    case TK_FUNCTION:
    case TK_LET:
    case TK_EQUALS:
        return false;
    case TK_PLUS:
    case TK_MINUS:
    case TK_STAR:
    case TK_SLASH:
    case TK_IDENT:
    case TK_EVAL:
        return true;
    case __TK_LENGTH:
        PANIC("unreachable");
    }
    PANIC("unreachable");
}

AST parse(FILE *file);

AST parse_cond(FILE *file) {
    if (take_token_if(file, TK_EOF, NULL)) {
        ERROR("expected condition, got EOF");
    }
    if (take_token_if(file, TK_RPAREN, NULL)) {
        ERROR("expected condition, got ')'");
    }

    AST cond = parse(file);
    AST *condp = malloc(sizeof(AST));
    *condp = cond;

    if (take_token_if(file, TK_EOF, NULL)) {
        ERROR("expected expression, got EOF");
    }
    if (take_token_if(file, TK_RPAREN, NULL)) {
        ERROR("expected expression, got ')'");
    }

    AST true_branch = parse(file);
    AST *true_branchp = malloc(sizeof(AST));
    *true_branchp = true_branch;

    if (take_token_if(file, TK_EOF, NULL)) {
        ERROR("expected expression or ')', got EOF");
    }
    AST *false_branchp = NULL;
    if (!take_token_if(file, TK_RPAREN, NULL)) {
        AST false_branch = parse(file);
        false_branchp = malloc(sizeof(AST));
        *false_branchp = false_branch;

        if (!take_token_if(file, TK_RPAREN, NULL)) {
            ERROR("IF may only contain a conditon, true branch, and optional false branch.");
        }

    }

    return (AST) {
        .kind = EK_IF,
        .value = {
            .if_ = {
                .cond = condp,
                .true_branch = true_branchp,
                .false_branch = false_branchp,
            }
        }
    };
}

AST parse_function_def(FILE *file) {

    Token name;
    if (!take_token_if(file, TK_IDENT, &name)) {
        ERROR("Expected name for function, got %s", token_string(*peek_token(file)));
    }

    ParamList params = { 0 };

    Token param;
    while (take_token_if(file, TK_IDENT, &param)) {
        da_append(&params, param.value.ident);
    }

    Token *peek = peek_token(file);
    if (peek->kind != TK_LPAREN) {
        ERROR("Expected function body, got %s", token_string(*peek));
    }

    AST body = parse(file);
    AST *bodyp = malloc(sizeof(AST));
    *bodyp = body;

    expect_token(file, TK_RPAREN);

    return (AST) {
        .kind = EK_FUNCTION_DEF,
        .value = {
            .fn_def = {
                .name = name.value.ident,
                .params = params,
                .body = bodyp
            }
        }
    };
}

AST parse_declare(FILE *file) {
    Token name;
    if (!take_token_if(file, TK_IDENT, &name)) {
        ERROR("Expected name for varaible declaration, got %s", token_string(*peek_token(file)));
    }

    Token *peek = peek_token(file);
    AST *valuep;
    if (!take_token_if(file, TK_RPAREN, NULL)) {
        AST value = parse(file);
        valuep = malloc(sizeof(AST));
        *valuep = value;
        expect_token(file, TK_RPAREN);
    }

    return (AST) {
        .kind = EK_DECLARE_VAR,
        .value = {
            .declare_assign = {
                .name = name.value.ident,
                .value = valuep,
            }
        }
    };
}

AST parse_assign(FILE *file) {
    Token name;
    if (!take_token_if(file, TK_IDENT, &name)) {
        ERROR("Expected name for varaible declaration, got %s", token_string(*peek_token(file)));
    }

    AST value = parse(file);
    AST *valuep = malloc(sizeof(AST));
    *valuep = value;
    expect_token(file, TK_RPAREN);

    return (AST) {
        .kind = EK_ASSIGN_VAR,
        .value = {
            .declare_assign = {
                .name = name.value.ident,
                .value = valuep,
            }
        }
    };
}

AST parse_cons(FILE *file) {
    expect_token(file, TK_LPAREN);

    Token tok = take_token(file);

    if (tok.kind == TK_IF) return parse_cond(file);
    if (tok.kind == TK_FUNCTION) return parse_function_def(file);
    if (tok.kind == TK_LET) return parse_declare(file);
    if (tok.kind == TK_EQUALS) return parse_assign(file);

    if (!is_function_token(tok.kind)) {
        ERROR("Expected function name, got %s", token_string(tok));
    }

    ASTList args = {0};

    for (;;) {
        if (take_token_if(file, TK_RPAREN, NULL)) {
            break;
        }
        if (take_token_if(file, TK_EOF, NULL)) {
            ERROR("expected ')' or value, got EOF");
        }

        AST expr = parse(file);
        da_append(&args, expr);
    }

    return (AST) {
        .kind = EK_FUNCTION_CALL,
        .value = {
            .fn_call = {
                .op = tok,
                .args = args,
            }
        },
    };
}

AST parse(FILE *file) {
    Token *tok = peek_token(file);
    switch (tok->kind) {
        case TK_LPAREN:
            return parse_cons(file);
        default: // TODO: this should enumerate each case
            return (AST) {
                .kind = EK_ATOM,
                .value = {
                    .atom = take_token(file),
                }
            };
    }
}

void print_ast(AST *ast, size_t depth) {
    int prefix = depth * 4;
    printf("%*s", prefix, "");
    switch (ast->kind) {
        case EK_ATOM:
            printf("Atom -> %s\n", token_string(ast->value.atom));
            break;
        case EK_IF:
            printf("if {\n");

            printf("%*s", prefix + 4, "");
            printf("condition:\n");
                print_ast(ast->value.if_.cond, depth + 2);

            printf("%*s", prefix + 4, "");
            printf("true_branch:\n");
                print_ast(ast->value.if_.true_branch, depth + 2);
            if (ast->value.if_.false_branch) {
                printf("%*s", prefix + 4, "");
                printf("false_branch:\n");
                    print_ast(ast->value.if_.false_branch, depth + 2);
            }
            printf("%*s}\n", prefix, "");
            break;
        case EK_FUNCTION_DEF: {
            FunctionDefValue fn = ast->value.fn_def;
            printf("FunctionDef {\n");
            printf("%*s", prefix + 4, "");
            printf("name: %s\n", fn.name);
            printf("%*s", prefix + 4, "");
            printf("params: ");
            for (size_t i = 0; i < fn.params.count; ++i) {
                if (i != 0) printf(" ");
                printf("%s", fn.params.items[i]);
            }
            printf("\n");
            printf("%*s", prefix + 4, "");
            printf("body:\n");
                print_ast(fn.body, depth + 2);
            printf("%*s}\n", prefix, "");
        } break;
        case EK_FUNCTION_CALL:
            printf("FunctionCall {\n");
            printf("%*s", prefix + 4, "");
            printf("op: %s\n", token_string(ast->value.fn_call.op));
            printf("%*s", prefix + 4, "");
            printf("args: [\n");
            for (size_t i = 0; i < ast->value.fn_call.args.count; ++i) {
                print_ast(&ast->value.fn_call.args.items[i], depth + 2);
            }
            printf("%*s", prefix + 4, "");
            printf("]\n");
            printf("%*s}\n", prefix, "");
            break;
        case EK_DECLARE_VAR: {
            DeclareAssign dec = ast->value.declare_assign;
            printf("DeclareVar {\n");
            printf("%*s", prefix + 4, "");
            printf("name: %s\n", dec.name);
            if (dec.value) {
                printf("%*s", prefix + 4, "");
                printf("value:\n");
                    print_ast(dec.value, depth + 2);
            }
            printf("%*s}\n", prefix, "");
        } break;
        case EK_ASSIGN_VAR: {
            DeclareAssign dec = ast->value.declare_assign;
            printf("AssignVar {\n");
            printf("%*s", prefix + 4, "");
            printf("name: %s\n", dec.name);
            printf("%*s", prefix + 4, "");
            printf("value:\n");
                print_ast(dec.value, depth + 2);
            printf("%*s}\n", prefix, "");
        } break;
    }
}

typedef enum {
    VK_UNIT = 0,
    VK_INT,
    VK_STRING,
    VK_BOOL,
    VK_FUNCTION,
    VK_NATIVE_FUNCTION,
    __VK_LENGTH,
} ValueKind;

const char *vk_names[] = {
    [VK_UNIT] = "UNIT",
    [VK_INT] = "INT",
    [VK_STRING] = "STRING",
    [VK_BOOL] = "BOOL",
    [VK_FUNCTION] = "FUNCTION",
    [VK_NATIVE_FUNCTION] = "NATIVE_FUNCTION",
};

static_assert(sizeof(vk_names) / sizeof(*vk_names) == __VK_LENGTH, "");

typedef struct Value Value;
typedef struct EvalScope EvalScope;

typedef struct {
    size_t expected_args;
    Value (*fn)(EvalScope *scope, size_t argc, Value *argv);
} NativeFunctionValue;

typedef union {
    int integer;
    StringSlice string;
    FunctionDefValue fn;
    NativeFunctionValue native;
} ValueValue;

typedef struct Value {
    ValueKind kind;
    ValueValue value;
    bool immutable;
} Value;

void free_value(Value *v) {
    switch (v->kind) {
        case VK_UNIT:
        case VK_INT:
        case VK_BOOL:
        case VK_NATIVE_FUNCTION:
        case __VK_LENGTH:
            break;
        case VK_STRING:
            free((void *)v->value.string.data);
        case VK_FUNCTION:
            // TODO: Fix double free when we have a function that has itself in one of the parameters:
            // (eval
            //     (function foo a (println a ))
            //     (foo foo)
            // )
            // free((void *)v->value.fn.params.items);
            return;
    }
}

void add_value(Value *curr, Value new) {
    switch (curr->kind) {
        case __VK_LENGTH: PANIC("unreachable");
        case VK_NATIVE_FUNCTION:
        case VK_FUNCTION:
            PANIC("Cannot add %s to %s", vk_names[new.kind], vk_names[curr->kind]);
            return;
        case VK_BOOL: {
            switch (new.kind) {
            case __VK_LENGTH: PANIC("unreachable");
            case VK_UNIT:
            case VK_FUNCTION:
            case VK_NATIVE_FUNCTION:
                PANIC("Cannot add %s to %s", vk_names[new.kind], vk_names[curr->kind]);
                return;
            case VK_INT:
            case VK_BOOL:
                curr->value.integer += new.value.integer;
                curr->kind = VK_INT;
                return;
            case VK_STRING: {
                size_t new_length = (curr->value.integer ? 4 : 5) + new.value.string.length + 1;
                char new_string[new_length];
                new_length = snprintf(
                    new_string, new_length,
                    "%s%.*s",
                    curr->value.integer ? "true" : "false",
                    (int)new.value.string.length, new.value.string.data
                );
                curr->value.string.data = strdup(new_string);
                curr->value.string.length = new_length;
                curr->kind = VK_STRING;
            } break;
            }
        } break;
        case VK_INT: {
            switch (new.kind) {
            case __VK_LENGTH: PANIC("unreachable");
            case VK_UNIT:
            case VK_FUNCTION:
            case VK_NATIVE_FUNCTION:
                PANIC("Cannot add %s to %s", vk_names[new.kind], vk_names[curr->kind]);
                return;
            case VK_INT:
            case VK_BOOL:
                curr->value.integer += new.value.integer;
                curr->kind = VK_INT;
                return;
            case VK_STRING: {
                size_t new_length = 20 + new.value.string.length;
                char new_string[new_length];
                new_length = snprintf(
                    new_string, new_length,
                    "%d%.*s",
                    curr->value.integer,
                    (int)new.value.string.length, new.value.string.data
                );
                curr->value.string.data = strdup(new_string);
                curr->value.string.length = new_length;
                curr->kind = VK_STRING;
            } break;
            }
        } break;
        case VK_STRING: {
            switch (new.kind) {
            case __VK_LENGTH: PANIC("unreachable");
            case VK_UNIT:
            case VK_FUNCTION:
            case VK_NATIVE_FUNCTION:
                PANIC("Cannot add %s to %s", vk_names[new.kind], vk_names[curr->kind]);
                return;
            case VK_BOOL: {
                size_t new_length = curr->value.string.length + (new.value.integer ? 4 : 5) + 1;
                char new_string[new_length];
                new_length = snprintf(
                    new_string, new_length,
                    "%.*s%s",
                    (int)curr->value.string.length, curr->value.string.data,
                    new.value.integer ? "true" : "false"
                );
                free((void*)curr->value.string.data);
                curr->value.string.data = strdup(new_string);
                curr->value.string.length = new_length;
                curr->kind = VK_STRING;
            } break;
            case VK_INT: {
                size_t new_length = curr->value.string.length + 20;
                char new_string[new_length];
                new_length = snprintf(
                    new_string, new_length,
                    "%.*s%d",
                    (int)curr->value.string.length, curr->value.string.data,
                    new.value.integer
                );
                free((void*)curr->value.string.data);
                curr->value.string.data = strdup(new_string);
                curr->value.string.length = new_length;
                curr->kind = VK_STRING;
            } break;
            case VK_STRING: {
                size_t new_length = curr->value.string.length + new.value.string.length + 1;
                char new_string[new_length];
                new_length = snprintf(
                    new_string, new_length,
                    "%.*s%.*s",
                    (int)curr->value.string.length, curr->value.string.data,
                    (int)new.value.string.length, new.value.string.data
                );
                free((void*)curr->value.string.data);
                curr->value.string.data = strdup(new_string);
                curr->value.string.length = new_length;
                curr->kind = VK_STRING;
            } break;
            }
        } break;
        case VK_UNIT:
            *curr = new;
            return;
        }
}

void sub_value(Value *curr, Value new) {
    if (curr->kind != VK_INT || new.kind != VK_INT) PANIC("Cannot subtract %s from %s", vk_names[new.kind], vk_names[curr->kind]);

    curr->value.integer -= new.value.integer;
    curr->kind = VK_INT;
}

void mult_value(Value *curr, Value new) {
    if (curr->kind != VK_INT || new.kind != VK_INT) PANIC("Cannot multiply %s by %s", vk_names[curr->kind], vk_names[new.kind]);

    curr->value.integer *= new.value.integer;
    curr->kind = VK_INT;
}

void div_value(Value *curr, Value new) {
    if (curr->kind != VK_INT || new.kind != VK_INT) PANIC("Cannot divide %s by %s", vk_names[curr->kind], vk_names[new.kind]);

    curr->value.integer /= new.value.integer;
    curr->kind = VK_INT;
}

bool is_value_truthy(Value v) {
    switch (v.kind) {
    case VK_UNIT:
        return false;
    case VK_INT:
    case VK_BOOL:
        return v.value.integer != 0;
    case VK_STRING:
        return v.value.string.length != 0;
    case VK_FUNCTION:
    case VK_NATIVE_FUNCTION:
        PANIC("Cannot convert function to BOOL");
    case __VK_LENGTH: PANIC("unreachable");
    }
    PANIC("unreachable");
}

void print_value(Value value) {
    switch (value.kind) {
        case __VK_LENGTH: PANIC("unreachable");
        case VK_INT:
            printf("%d", value.value.integer);
            break;
        case VK_STRING:
            printf("%.*s", (int)value.value.string.length, value.value.string.data);
            break;
        case VK_BOOL:
            printf("%s", value.value.integer ? "true" : "false");
            break;
        case VK_FUNCTION:
            printf("<function '%s'>", value.value.fn.name);
            break;
        case VK_NATIVE_FUNCTION:
            printf("<native function '%s'>", value.value.fn.name);
            break;
        case VK_UNIT:
            printf("()");
            break;
    }
}

typedef struct {
    const char *key;
    Value value;
} VariableMapEntry;

// TODO: use hasmap here
typedef struct {
    VariableMapEntry *items;
    size_t count;
    size_t capacity;
} VariableMap;

typedef struct EvalScope {
    VariableMap vars;
    EvalScope *parent;
} EvalScope;

EvalScope create_scope(EvalScope *parent) {
    return (EvalScope) {
        .vars = { 0 },
        .parent = parent,
    };
}

void free_scope(EvalScope scope) {
    for (size_t i = 0; i < scope.vars.count; ++i) {
        free_value(&scope.vars.items[i].value);
    }
    free(scope.vars.items);
}

// adds a var to the scope with the value of UNIT.
Value *add_var(EvalScope *scope, const char *name) {
    for (size_t i = 0; i < scope->vars.count; ++i) {
        VariableMapEntry entry = scope->vars.items[i];
        if (!strcmp(entry.key, name)) PANIC("Variable '%s' already declared.", name);
    }
    Value v = { 0 };
    VariableMapEntry entry = {
        .key = name,
        .value = v,
    };
    da_append(&scope->vars, entry);
    return &scope->vars.items[scope->vars.count - 1].value;
}

void set_var(EvalScope *scope, const char *name, Value v) {
    for (size_t i = 0; i < scope->vars.count; ++i) {
        VariableMapEntry *entry = &scope->vars.items[i];
        if (!strcmp(entry->key, name)) {
            entry->value = v;
            return;
        }
    }
    VariableMapEntry entry = {
        .key = name,
        .value = v,
    };
    da_append(&scope->vars, entry);
}

Value *get_var(EvalScope *scope, const char *name) {
    for (size_t i = 0; i < scope->vars.count; ++i) {
        VariableMapEntry entry = scope->vars.items[i];
        if (!strcmp(entry.key, name))
            return &scope->vars.items[i].value;
    }
    if (scope->parent == NULL) return NULL;
    return get_var(scope->parent, name);
}

Value eval(AST ast, EvalScope *scope);
Value eval_in_scope(AST ast, EvalScope *scope) {
    switch (ast.kind) {
        case EK_ATOM: {
            switch (ast.value.atom.kind) {
                case TK_LPAREN:
                case TK_RPAREN:
                case TK_PLUS:
                case TK_MINUS:
                case TK_STAR:
                case TK_SLASH:
                case TK_IF:
                case TK_EOF:
                case TK_EVAL:
                case TK_FUNCTION:
                case TK_LET:
                case TK_EQUALS:
                case __TK_LENGTH:
                    PANIC("unreachable: %s", token_string(ast.value.atom));
                case TK_INT:
                    return (Value) {
                        .kind = VK_INT,
                        .value = {
                            .integer = ast.value.atom.value.integer,
                        }
                    };
                case TK_IDENT: {
                    Value *var = get_var(scope, ast.value.atom.value.ident);
                    if (!var) PANIC("Variable '%s' does not exist in current scope.", ast.value.atom.value.ident);
                    return *var;
                } break;
                case TK_TRUE:
                case TK_FALSE: {
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = ast.value.atom.kind == TK_TRUE,
                        }
                    };
                }
                case TK_STRING:
                    return (Value) {
                        .kind = VK_STRING,
                        .value = {
                            .string = ast.value.atom.value.string,
                        }
                    };
            }
        } break;
        case EK_FUNCTION_CALL: {
            switch(ast.value.fn_call.op.kind) {
                case TK_EOF:
                case TK_LPAREN:
                case TK_RPAREN:
                case TK_INT:
                case TK_STRING:
                case TK_IF:
                case TK_TRUE:
                case TK_FALSE:
                case TK_FUNCTION:
                case TK_LET:
                case TK_EQUALS:
                case __TK_LENGTH:
                    PANIC("unreachable");

                case TK_EVAL: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 1) PANIC("Eval operation must have at least on expression");
                    Value ret = { 0 };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        ret = eval(fn.args.items[i], scope);
                    }
                    return ret;
                } break;
                case TK_PLUS: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 1) PANIC("Add operation must contain at least one value.");
                    Value out = { 0 };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        add_value(&out, eval(fn.args.items[i], scope));
                    }
                    return out;
                } break;
                case TK_MINUS: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 2) PANIC("Subtract operation must contain at least two values.");
                    Value out = eval(fn.args.items[0], scope);
                    for (size_t i = 1; i < fn.args.count; ++i) {
                        sub_value(&out, eval(fn.args.items[i], scope));
                    }
                    return out;
                } break;
                case TK_STAR: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 1) PANIC("Multiply operation must contain at least one value.");
                    Value out = {
                        .kind = VK_INT,
                        .value = {
                            .integer = 1,
                        },
                    };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        mult_value(&out, eval(fn.args.items[i], scope));
                    }
                    return out;
                } break;
                case TK_SLASH: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 2) PANIC("Subtract operation must contain at least two values.");
                    Value out = eval(fn.args.items[0], scope);
                    for (size_t i = 1; i < fn.args.count; ++i) {
                        div_value(&out, eval(fn.args.items[i], scope));
                    }
                    return out;
                } break;
                case TK_IDENT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    const char *name = fn.op.value.ident;
                    Value *var = get_var(scope, name);
                    if (var == NULL) {
                        PANIC("Unknown function '%s'", name);
                    } else if (var->kind == VK_FUNCTION) {
                        FunctionDefValue fndef = var->value.fn;

                        if (fn.args.count != fndef.params.count)
                            PANIC("Function '%s' expected %ld params, received %ld.", name, fndef.params.count, fn.args.count);
                        
                        EvalScope fn_scope = create_scope(scope);
                        for (size_t i = 0; i < fndef.params.count; ++i) {
                            Value *v = add_var(&fn_scope, fndef.params.items[i]);
                            *v = eval(fn.args.items[i], scope);
                        }
                        eval_in_scope(*fndef.body, &fn_scope);
                        free_scope(fn_scope);
                    } else if (var->kind == VK_NATIVE_FUNCTION) {
                        NativeFunctionValue fndef = var->value.native;

                        if (fndef.expected_args != -1 && fn.args.count != fndef.expected_args)
                            PANIC("Function '%s' expected %ld params, received %ld.", name, fndef.expected_args, fn.args.count);

                        Value argv[fn.args.count];

                        for (size_t i = 0; i < fn.args.count; ++i) {
                            argv[i] = eval(fn.args.items[i], scope);
                        }

                        return fndef.fn(scope, fn.args.count, argv);
                    } else {
                        PANIC("Variable '%s' is not a function.", name);
                    }
                } break;
            }
        } break;
        case EK_IF: {
            IfValue if_ = ast.value.if_;
            Value cond = eval(*if_.cond, scope);
            if (is_value_truthy(cond)) {
                return eval(*if_.true_branch, scope);
            }

            if (if_.false_branch) {
                return eval(*if_.false_branch, scope);
            }

            return (Value) { 0 };
        } break;
        case EK_FUNCTION_DEF: {
            FunctionDefValue fn = ast.value.fn_def;
            Value *var = add_var(scope->parent, fn.name);
            var->kind = VK_FUNCTION;
            var->value.fn = fn;
            return (Value) { 0 };
        } break;
        case EK_DECLARE_VAR: {
            DeclareAssign dec = ast.value.declare_assign;
            Value *var = add_var(scope->parent, dec.name);
            if (dec.value) {
                *var = eval(*dec.value, scope);
            }
            return (Value) { 0 };
        } break;
        case EK_ASSIGN_VAR: {
            DeclareAssign ass = ast.value.declare_assign;
            Value *var = get_var(scope->parent, ass.name);
            if (var == NULL) {
                PANIC("Variable '%s' does not exist in scope.", ass.name);
            }
            if (var->immutable) {
                PANIC("Variable '%s' is immutable.", ass.name);
            }
            *var = eval(*ass.value, scope);
            return (Value) { 0 };
        } break;
    }
    return (Value) { 0 };
}

Value eval(AST ast, EvalScope *parent_scope) {
    EvalScope scope = create_scope(parent_scope);
    Value ret = eval_in_scope(ast, &scope);
    free_scope(scope);
    return ret;
}

Value native_print(EvalScope *scope, size_t argc, Value *argv) {
    for (size_t i = 0; i < argc; ++i) {
        if (i != 0) printf(" ");
        print_value(argv[i]);
    }
    return (Value) { 0 };
}

Value native_println(EvalScope *scope, size_t argc, Value *argv) {
    Value ret = native_print(scope, argc, argv);
    printf("\n");
    return ret;
}

Value native_parseint(EvalScope *scope, size_t argc, Value *argv) {
    assert(argc == 1);
    Value arg = argv[0];
    if (arg.kind != VK_STRING) {
        PANIC("parseint accepts one string as its argument, found %s.", vk_names[arg.kind]);
    }

    int value = atoi(arg.value.string.data);
    return (Value) {
        .kind = VK_INT,
        .value = {
            .integer = value,
        }
    };
}

Value native_readline(EvalScope *scope, size_t argc, Value *_argv) {
    assert(argc == 0);
    char *line = NULL;
    size_t n = 0;
    ssize_t r = getline(&line, &n, stdin);
    if (r < 0) PANIC("Unexpeced error while running readline: %m");
    line[--r] = '\0'; // remove newline
    return (Value) {
        .kind = VK_STRING,
        .value = {
            .string = {
                .data = line,
                .length = r,
            }
        }
    };
}

#define ADD_FN(name, native_fn, argc)  \
    set_var(&scope, #name, (Value) {    \
        .kind = VK_NATIVE_FUNCTION,    \
        .value = {                     \
            .native = {                \
                .expected_args = argc, \
                .fn = native_fn,       \
            }                          \
        },                             \
    });                                \

EvalScope create_global_scope() {
    EvalScope scope = create_scope(NULL);
    ADD_FN(print, native_print, -1);
    ADD_FN(println, native_println, -1);
    ADD_FN(parseint, native_parseint, 1);
    ADD_FN(readline, native_readline, 0);
    return scope;
}

int main(int argc, char **argv)
{
    FILE *file;
    if (argc == 1) {
        file = stdin;
        file_name = "stdin";
    } else {
        file_name = argv[1];
        file = fopen(argv[1], "rb");
        if (!file) PANIC("Could not open file for reading %s: %m", argv[1]);
    }
    // Token tok;
    // while ((tok = next_token(file)).kind != TK_EOF) {
    //     printf("%s\n", token_string(tok));
    // }
    AST ast = parse(file);
    Token tok = take_token(file);
    if (tok.kind != TK_EOF) {
        ERROR("Expected EOF, found %s", token_string(tok));
    }
    fclose(file);
    print_ast(&ast, 0);

    EvalScope global_scope = create_global_scope();
    eval(ast, &global_scope);
}

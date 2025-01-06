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
#define ERROR(...) do {                                                                        \
    fprintf(stderr, "[ERROR] (%s:%d) %s:%ld:%ld: ", __FILE__, __LINE__, file_name, col, line); \
    fprintf(stderr, __VA_ARGS__);                                                              \
    fprintf(stderr, "\n");                                                                     \
    exit(1);                                                                                   \
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
    TK_AT,
    TK_DOT,

    TK_DEQ, // ==
    TK_BANG, // !
    TK_NEQ, // !=
    TK_LT, // <
    TK_LEQ, // <=
    TK_GT, // >
    TK_GEQ, // >=

    // keywords
    TK_LET,
    TK_IF,
    TK_TRUE,
    TK_FALSE,
    TK_EVAL,
    TK_FUNCTION,
    TK_WHILE,
    TK_FOR,

    // literals
    TK_INT,
    TK_IDENT,
    TK_STRING,

    __TK_LENGTH,
} TokenKind;

typedef struct {
    char *items; 
    size_t count;
    ssize_t capacity;
} String;

String new_string(char *cstr) {
    return (String) {
        .items = cstr,
        .count = strlen(cstr),
        .capacity = -1,
    };
}

void resize_string(String *curr, size_t new_size) {
    if (curr->count > new_size) return;
    if (curr->capacity == -1) {
        char *data = malloc(curr->capacity = new_size);
        strcpy(data, curr->items);
        curr->items = data;
    } else if (curr->capacity < new_size) {
        curr->items = realloc(curr->items, curr->capacity = new_size);
    }
}

void extend_string(String *curr, String other) {
    resize_string(curr, curr->count + other.count + 1);
    strncpy(curr->items + curr->count, other.items, other.count);
    curr->count += other.count;
    curr->items[curr->count] = '\0';
}

typedef union {
    int integer;
    const char *ident;
    String string;
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
    if (!strcmp("while", ident)) return TK_WHILE;
    if (!strcmp("for", ident)) return TK_FOR;
    return TK_IDENT;
}

String take_string(FILE *file, char quote) {
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

    return (String) {
        .items = string,
        .count = len,
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
            case '(': return (Token) {
                .kind = TK_LPAREN,
            };
            case ')': return (Token) {
                .kind = TK_RPAREN,
            };
            case '+': return (Token) {
                .kind = TK_PLUS,
            };
            case '*': return (Token) {
                .kind = TK_STAR,
            };
            case '!': {
                if (fpeek(file) == '=') {
                    ftake(file);
                    return (Token) {
                        .kind = TK_NEQ,
                    };
                }
                return (Token) {
                    .kind = TK_BANG,
                };
            } break;
            case '<': {
                if (fpeek(file) == '=') {
                    ftake(file);
                    return (Token) {
                        .kind = TK_LEQ,
                    };
                }
                return (Token) {
                    .kind = TK_LT,
                };
            } break;
            case '>': {
                if (fpeek(file) == '=') {
                    ftake(file);
                    return (Token) {
                        .kind = TK_GEQ,
                    };
                }
                return (Token) {
                    .kind = TK_GT,
                };
            } break;
            case '=': {
                if (fpeek(file) == '=') {
                    ftake(file);
                    return (Token) {
                        .kind = TK_DEQ,
                    };
                }
                return (Token) {
                    .kind = TK_EQUALS,
                };
            } break;
            case '/': return (Token) {
                .kind = TK_SLASH,
            };
            case '@': return (Token) {
                .kind = TK_AT,
            };
            case '.': return (Token) {
                .kind = TK_DOT,
            };
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
                String string = take_string(file, c);
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
    [TK_AT] = "AT",
    [TK_DOT] = "DOT",

    [TK_LET] = "LET",
    [TK_IF] = "IF",
    [TK_TRUE] = "TRUE",
    [TK_FALSE] = "FALSE",
    [TK_EVAL] = "EVAL",
    [TK_FUNCTION] = "FUNCTION",
    [TK_WHILE] = "WHILE",
    [TK_FOR] = "FOR",

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
    case TK_AT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '@'");
        break;
    case TK_DOT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '.'");
        break;

    case TK_DEQ:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '=='");
        break;
    case TK_BANG:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '!'");
        break;
    case TK_NEQ:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '!='");
        break;
    case TK_LT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '<'");
        break;
    case TK_GT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '>'");
        break;
    case TK_LEQ:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '<='");
        break;
    case TK_GEQ:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '>='");
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
    case TK_WHILE:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'while'");
        break;
    case TK_FOR:
        n += snprintf(sbuf + n, SBUF_LEN - n, " 'for'");
        break;

    case TK_INT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " %d", tok.value.integer);
        break;
    case TK_IDENT:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '%s'", tok.value.ident);
        break;
    case TK_STRING:
        n += snprintf(sbuf + n, SBUF_LEN - n, " '%s'", tok.value.string.items);
        break;

    case __TK_LENGTH:
        PANIC("unreachable");
    }
    return sbuf;
}

typedef struct AST AST;

typedef enum {
    EK_ATOM = 0,
    EK_UNIT,
    EK_FUNCTION_CALL,
    EK_FUNCTION_DEF,
    EK_IF,
    EK_DECLARE_VAR,
    EK_ASSIGN_VAR,
    EK_WHILE,
    EK_FOR,
    __EK_LENGTH,
} ExpressionKind;

static const char *ek_names[] = {
    [EK_ATOM] = "ATOM",
    [EK_FUNCTION_CALL] = "FUNCTION_CALL",
    [EK_FUNCTION_DEF] = "FUNCTION_DEF",
    [EK_IF] = "IF",
    [EK_DECLARE_VAR] = "DECLARE_VAR",
    [EK_ASSIGN_VAR] = "ASSIGN_VAR",
    [EK_WHILE] = "WHILE",
    [EK_FOR] = "FOR",
};

static_assert(sizeof(ek_names) / sizeof(*ek_names) == __EK_LENGTH, "Missing names for tokens");

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
    ParamList params;
    AST *body;
} FunctionDefValue;

typedef struct {
    AST *cond;
    AST *true_branch;
    AST *false_branch; // optional
} IfValue;

typedef struct {
    AST *cond;
    AST *body;
} WhileValue;

typedef struct {
    AST *init;
    AST *cond;
    AST *post;
    AST *body;
} ForValue;

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
    WhileValue while_;
    ForValue for_;
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
    if (tok.kind != kind) ERROR("Expected token %s, found %s.", tk_names[kind], tk_names[tok.kind]);
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
    case TK_WHILE:
    case TK_FOR:
        return false;
    case TK_PLUS:
    case TK_MINUS:
    case TK_STAR:
    case TK_SLASH:
    case TK_IDENT:
    case TK_EVAL:
    case TK_AT:
    case TK_DOT:
    case TK_DEQ:
    case TK_NEQ:
    case TK_LT:
    case TK_GT:
    case TK_LEQ:
    case TK_GEQ:
    case TK_BANG:
        return true;
    case __TK_LENGTH:
        PANIC("unreachable");
    }
    PANIC("unreachable");
}

AST parse(FILE *file, const char *expected);

AST parse_cond(FILE *file) {
    AST cond = parse(file, "condition");
    AST *condp = malloc(sizeof(AST));
    *condp = cond;

    AST true_branch = parse(file, "IF true branch");
    AST *true_branchp = malloc(sizeof(AST));
    *true_branchp = true_branch;

    if (take_token_if(file, TK_EOF, NULL)) {
        ERROR("expected expression or ')', got EOF");
    }
    AST *false_branchp = NULL;
    if (!take_token_if(file, TK_RPAREN, NULL)) {
        AST false_branch = parse(file, "IF false branch");
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
    ParamList params = { 0 };

    Token param;
    while (take_token_if(file, TK_IDENT, &param)) {
        da_append(&params, param.value.ident);
    }

    Token *peek = peek_token(file);
    AST body = parse(file, "function body");
    AST *bodyp = malloc(sizeof(AST));
    *bodyp = body;

    expect_token(file, TK_RPAREN);

    return (AST) {
        .kind = EK_FUNCTION_DEF,
        .value = {
            .fn_def = {
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

    AST *valuep;
    if (!take_token_if(file, TK_RPAREN, NULL)) {
        AST value = parse(file, "variable value");
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

    AST value = parse(file, "variable value");
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

AST parse_while(FILE *file) {
    AST cond = parse(file, "while condition");
    AST *condp = malloc(sizeof(AST));
    *condp = cond;

    AST body = parse(file, "while body");
    AST *bodyp = malloc(sizeof(AST));
    *bodyp = body;

    expect_token(file, TK_RPAREN);

    return (AST) {
        .kind = EK_WHILE,
        .value = {
            .while_ = {
                .cond = condp,
                .body = bodyp,
            }
        }
    };
}

AST parse_for(FILE *file) {
    AST init = parse(file, "FOR init");
    AST *initp = malloc(sizeof(AST));
    *initp = init;

    AST cond = parse(file, "FOR condition");
    AST *condp = malloc(sizeof(AST));
    *condp = cond;

    AST post = parse(file, "FOR post");
    AST *postp = malloc(sizeof(AST));
    *postp = post;

    AST body = parse(file, "FOR body");
    AST *bodyp = malloc(sizeof(AST));
    *bodyp = body;

    expect_token(file, TK_RPAREN);

    return (AST) {
        .kind = EK_FOR,
        .value = {
            .for_ = {
                .init = initp,
                .cond = condp,
                .post = postp,
                .body = bodyp,
            }
        }
    };
}

AST parse_cons(FILE *file) {
    expect_token(file, TK_LPAREN);

    if(take_token_if(file, TK_RPAREN, NULL)) {
        return (AST) {
            .kind = EK_UNIT,
        };
    }

    Token tok = take_token(file);

    if (tok.kind == TK_IF) return parse_cond(file);
    if (tok.kind == TK_FUNCTION) return parse_function_def(file);
    if (tok.kind == TK_LET) return parse_declare(file);
    if (tok.kind == TK_EQUALS) return parse_assign(file);
    if (tok.kind == TK_WHILE) return parse_while(file);
    if (tok.kind == TK_FOR) return parse_for(file);

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

        AST expr = parse(file, "function argument");
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

bool is_expression_start(TokenKind tk) {
    switch (tk) {
        case TK_LPAREN:
        case TK_INT:
        case TK_IDENT:
        case TK_STRING:
        case TK_TRUE:
        case TK_FALSE:
            return true;

        case TK_EOF:
        case TK_RPAREN:
        case TK_PLUS:
        case TK_MINUS:
        case TK_STAR:
        case TK_SLASH:
        case TK_EQUALS:
        case TK_AT:
        case TK_LET:
        case TK_IF:
        case TK_EVAL:
        case TK_FUNCTION:
        case TK_WHILE:
        case TK_FOR:
        case TK_DOT:
        case TK_DEQ:
        case TK_NEQ:
        case TK_LT:
        case TK_GT:
        case TK_LEQ:
        case TK_GEQ:
        case TK_BANG:
        case __TK_LENGTH:
            return false;
    }
}

AST parse(FILE *file, const char *expected) {
    Token *tok = peek_token(file);
    if (!is_expression_start(tok->kind)) {
        ERROR("expected %s, got %s", expected ? expected : "expression", tk_names[tok->kind]);
    }
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
        case __EK_LENGTH: PANIC("unreachable");
        case EK_ATOM:
            printf("Atom -> %s\n", token_string(ast->value.atom));
            break;
        case EK_UNIT:
            printf("UNIT\n");
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
        case EK_WHILE: {
            WhileValue w = ast->value.while_;
            printf("while {\n");

            printf("%*s", prefix + 4, "");
            printf("condition:\n");
                print_ast(w.cond, depth + 2);

            printf("%*s", prefix + 4, "");
            printf("body:\n");
                print_ast(w.body, depth + 2);
            printf("%*s}\n", prefix, "");
        } break;
        case EK_FOR: {
            ForValue f = ast->value.for_;
            printf("for {\n");

            printf("%*s", prefix + 4, "");
            printf("init:\n");
                print_ast(f.init, depth + 2);

            printf("%*s", prefix + 4, "");
            printf("condition:\n");
                print_ast(f.cond, depth + 2);

            printf("%*s", prefix + 4, "");
            printf("post:\n");
                print_ast(f.post, depth + 2);

            printf("%*s", prefix + 4, "");
            printf("body:\n");
                print_ast(f.body, depth + 2);

            printf("%*s}\n", prefix, "");
        } break;
    }
}

typedef enum {
    VK_UNIT = 0,
    VK_INT,
    VK_CHAR,
    VK_STRING,
    VK_BOOL,
    VK_FUNCTION,
    VK_NATIVE_FUNCTION,
    VK_ARRAY,
    __VK_LENGTH,
} ValueKind;

const char *vk_names[] = {
    [VK_UNIT] = "UNIT",
    [VK_INT] = "INT",
    [VK_CHAR] = "CHAR",
    [VK_STRING] = "STRING",
    [VK_BOOL] = "BOOL",
    [VK_FUNCTION] = "FUNCTION",
    [VK_NATIVE_FUNCTION] = "NATIVE_FUNCTION",
    [VK_ARRAY] = "ARRAY",
};

static_assert(sizeof(vk_names) / sizeof(*vk_names) == __VK_LENGTH, "");

typedef struct Value Value;
typedef struct EvalContext EvalContext;

typedef struct {
    const char *name;
    ssize_t min_args;
    ssize_t max_args;
    Value (*fn)(EvalContext *ctx, size_t argc, Value *argv);
} NativeFunctionValue;

typedef struct {
    Value *items;
    size_t count;
    size_t capacity;
} ValueArray;

typedef union {
    int integer;
    char character;
    String string;
    FunctionDefValue fn;
    NativeFunctionValue native;
    ValueArray array;
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
        case VK_CHAR:
        case VK_BOOL:
        case VK_NATIVE_FUNCTION:
        case __VK_LENGTH:
            break;
        case VK_STRING:
        case VK_ARRAY:
            for (size_t i = 0; i < v->value.array.count; ++i) {
                free_value(&v->value.array.items[i]);
            }
            free(v->value.array.items);
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

bool value_to_bool(Value v) {
    switch (v.kind) {
    case VK_UNIT:
        return false;
    case VK_INT:
    case VK_CHAR:
    case VK_BOOL:
        return v.value.integer != 0;
    case VK_STRING:
        return v.value.string.count != 0;
    case VK_ARRAY:
    case VK_FUNCTION:
    case VK_NATIVE_FUNCTION:
        PANIC("Cannot convert %s to BOOL", vk_names[v.kind]);
    case __VK_LENGTH: PANIC("unreachable");
    }
    PANIC("unreachable");
}

char *value_to_string(Value value) {
    switch (value.kind) {
        case __VK_LENGTH: PANIC("unreachable");
        case VK_CHAR: {
            char *str = malloc(2);
            str[0] = value.value.character;
            str[1] = '\0';
            return str;
        }
        case VK_INT: {
            char buf[20];
            snprintf(buf, 20, "%d", value.value.integer);
            return strdup(buf);
        }
        case VK_STRING:
            return value.value.string.items;
        case VK_BOOL:
            return value.value.integer ? "true" : "false";
        case VK_FUNCTION: {
            char buf[256];
            snprintf(buf, 256, "<anonymous function>");
            return strdup(buf);
        }
        case VK_NATIVE_FUNCTION: {
            char buf[256];
            snprintf(buf, 256, "<native function '%s'>", value.value.native.name);
            return strdup(buf);
        }
        case VK_ARRAY: {
            String s = new_string("(@");
            for (size_t i = 0; i < value.value.array.count; ++i) {
                extend_string(&s, new_string(" "));
                extend_string(&s, new_string(value_to_string(value.value.array.items[i])));
            }
            extend_string(&s, new_string(")"));
            return strdup(s.items);
        }
        case VK_UNIT:
            return "()";
    }
    return NULL;
}

bool coerce(Value *value, ValueKind vk) {
    if (value->kind == vk) return true;
    switch (vk) {
        case VK_STRING:
            value->kind = vk;
            value->value.string = new_string((char *)value_to_string(*value));
            return true;
        case VK_BOOL:
            value->kind = vk;
            value->value.integer = value_to_bool(*value);
            return true;
        case VK_UNIT: // TODO: make everything coerce into a unit?
            return false;
        case VK_ARRAY:
            return false;
        case VK_CHAR: {
            if (value->kind != VK_INT) return false;
            int n = value->value.integer;
            value->kind = VK_CHAR;
            value->value.character = (char) n;
            return true;
        } break;
        case VK_INT:
            switch (value->kind) {
                case VK_INT: return true;
                case VK_CHAR: 
                    value->value.integer = value->value.character;
                case VK_BOOL: 
                    value->kind = vk;
                    return true;
                case VK_STRING: // TODO: implicit parse int?
                case VK_UNIT:
                case VK_FUNCTION:
                case VK_NATIVE_FUNCTION:
                case VK_ARRAY:
                    return false;
                case __VK_LENGTH: PANIC("unreachable");
            }
        case VK_FUNCTION:
        case VK_NATIVE_FUNCTION:
            return false;
        case __VK_LENGTH:
            PANIC("unreachable");
    }
    return false;
}

void add_value(Value *curr, Value new) {
    if (curr->kind == VK_UNIT) {
        *curr = new;
        return;
    }

    if (curr->kind == VK_STRING) {
        String right = new_string(value_to_string(new));
        extend_string(&curr->value.string, right);
        return;
    }

    if (coerce(&new, curr->kind)) {
        curr->value.integer += new.value.integer;
        return;
    }

    PANIC("Cannot add %s to %s", vk_names[new.kind], vk_names[curr->kind]);
}

void sub_value(Value *curr, Value new) {
    if (curr->kind != VK_INT || !coerce(&new, curr->kind)) PANIC("Cannot subtract %s from %s", vk_names[new.kind], vk_names[curr->kind]);

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

typedef struct {
    const char *key;
    Value value;
} VariableMapEntry;

// TODO: use hashmap here
typedef struct {
    VariableMapEntry *items;
    size_t count;
    size_t capacity;
} VariableMap;

typedef struct EvalContext {
    VariableMap vars;
    EvalContext *parent;
} EvalContext;

EvalContext create_ctx(EvalContext *parent) {
    return (EvalContext) {
        .vars = { 0 },
        .parent = parent,
    };
}

void free_ctx(EvalContext ctx) {
    for (size_t i = 0; i < ctx.vars.count; ++i) {
        free_value(&ctx.vars.items[i].value);
    }
    free(ctx.vars.items);
}

// adds a var to the ctx with the value of UNIT.
Value *add_var(EvalContext *ctx, const char *name) {
    for (size_t i = 0; i < ctx->vars.count; ++i) {
        VariableMapEntry entry = ctx->vars.items[i];
        if (!strcmp(entry.key, name)) PANIC("Variable '%s' already declared.", name);
    }
    Value v = { 0 };
    VariableMapEntry entry = {
        .key = name,
        .value = v,
    };
    da_append(&ctx->vars, entry);
    return &ctx->vars.items[ctx->vars.count - 1].value;
}

void set_var(EvalContext *ctx, const char *name, Value v) {
    for (size_t i = 0; i < ctx->vars.count; ++i) {
        VariableMapEntry *entry = &ctx->vars.items[i];
        if (!strcmp(entry->key, name)) {
            entry->value = v;
            return;
        }
    }
    VariableMapEntry entry = {
        .key = name,
        .value = v,
    };
    da_append(&ctx->vars, entry);
}

Value *get_var(EvalContext *ctx, const char *name) {
    for (size_t i = 0; i < ctx->vars.count; ++i) {
        VariableMapEntry entry = ctx->vars.items[i];
        if (!strcmp(entry.key, name))
            return &ctx->vars.items[i].value;
    }
    if (ctx->parent == NULL) return NULL;
    return get_var(ctx->parent, name);
}

// (inclusive)
const char *check_range(size_t x, ssize_t min, ssize_t max) {
    if (min > 0 && x < min) return "Not enough";
    if (max >= 0 && x > max) return "Too many";
    return NULL;
}

typedef enum {
    ORD_EQ = 0, // a == b
    ORD_LESS, // a < b
    ORD_GREATER, // a > b
    ORD_NEQ, // a != b (and doesn't make sense to have a < b or a > b)
    ORD_NONE, // no relation
} Ordering;

Ordering ord_from_int(int cmp) {
    if (cmp < 0) return ORD_LESS;
    if (cmp > 0) return ORD_GREATER;
    return ORD_EQ;
}

Ordering compare_values(Value a, Value b) {
    // TODO: Subtyping
    if (a.kind != b.kind) return ORD_NONE;

    switch (a.kind) {
        case VK_UNIT:
            return ORD_EQ;
        case VK_INT:
            // https://stackoverflow.com/questions/10996418/efficient-integer-compare-function#10997428
            return ord_from_int((a.value.integer > b.value.integer) - (a.value.integer < b.value.integer));
        case VK_CHAR:
            return ord_from_int(a.value.character - b.value.character);
        case VK_ARRAY:
            if (a.value.array.count < b.value.array.count) return ORD_LESS; 
            if (a.value.array.count > b.value.array.count) return ORD_GREATER; 
            for (size_t i = 0; i < a.value.array.count; ++i) {
                Value ai = a.value.array.items[i];
                Value bi = b.value.array.items[i];
                Ordering cmp = compare_values(ai, bi);
                if (cmp == ORD_EQ) continue;
                return cmp;
            }
            return ORD_EQ;
        case VK_STRING:
            if (a.value.string.count < b.value.string.count) return ORD_LESS; 
            if (a.value.string.count > b.value.string.count) return ORD_GREATER; 
            int cmp = strncmp(a.value.string.items, b.value.string.items, a.value.string.count);
            return ord_from_int(cmp);
        case VK_BOOL:
            if (!!a.value.integer == !!b.value.integer) return ORD_EQ;
            return ORD_NEQ;
        case VK_FUNCTION:
            return ORD_NONE;
        case VK_NATIVE_FUNCTION:
            return ORD_NONE;
        case __VK_LENGTH:
            PANIC("unreachable");
    }
}

Value eval(AST ast, EvalContext *ctx);
Value eval_in_ctx(AST ast, EvalContext *ctx);

Value apply_fn(EvalContext *ctx, const char *name, Value fn, size_t argc, Value *argv) {
    assert(fn.kind == VK_FUNCTION || fn.kind == VK_NATIVE_FUNCTION);
    if (fn.kind == VK_FUNCTION) {
        FunctionDefValue fndef = fn.value.fn;

        if (argc != fndef.params.count)
            PANIC("Function '%s' expected %ld params, received %ld.", name, fndef.params.count, argc);

        EvalContext fn_ctx = create_ctx(ctx);
        for (size_t i = 0; i < fndef.params.count; ++i) {
            Value *v = add_var(&fn_ctx, fndef.params.items[i]);
            *v = argv[i];
        }
        Value ret = eval_in_ctx(*fndef.body, &fn_ctx);
        free_ctx(fn_ctx);
        return ret;
    } else {
        NativeFunctionValue fndef = fn.value.native;

        const char *reason = check_range(argc, fndef.min_args, fndef.max_args);

        if (reason) {
            if (fndef.min_args == fndef.max_args)
                PANIC("%s arguments passed to function '%s'.  Expected %ld, got %ld", reason, fndef.name, fndef.min_args, argc);
            if (fndef.min_args <= 0)
                PANIC("%s arguments passed to function '%s'.  Expected at most %ld, got %ld", reason, fndef.name, fndef.max_args, argc);
            if (fndef.max_args < 0)
                PANIC("%s arguments passed to function '%s'.  Expected at least %ld, got %ld", reason, fndef.name, fndef.min_args, argc);
        }

        return fndef.fn(ctx, argc, argv);
    }
}

Value eval_in_ctx(AST ast, EvalContext *ctx) {
    switch (ast.kind) {
        case __EK_LENGTH: PANIC("unreachable");
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
                case TK_AT:
                case TK_WHILE:
                case TK_FOR:
                case TK_DEQ:
                case TK_NEQ:
                case TK_LT:
                case TK_GT:
                case TK_LEQ:
                case TK_GEQ:
                case TK_DOT:
                case TK_BANG:
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
                    Value *var = get_var(ctx, ast.value.atom.value.ident);
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
        case EK_UNIT: return (Value) { 0 };
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
                case TK_WHILE:
                case TK_FOR:
                case __TK_LENGTH:
                    PANIC("unreachable");

                case TK_BANG: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 1) {
                        PANIC("Expected one arguments to !, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    if (!coerce(&arg0, VK_BOOL)) PANIC("Cannot convert type %s to BOOL", vk_names[arg0.kind]);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = !arg0.value.integer,
                        }
                    };
                } break;
                case TK_DEQ: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = !compare_values(arg0, arg1),
                        }
                    };
                } break;
                case TK_NEQ: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = compare_values(arg0, arg1), // EQ is 0, everything else is nonzero
                        }
                    };
                } break;
                case TK_LT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = compare_values(arg0, arg1) == ORD_LESS,
                        }
                    };
                } break;
                case TK_GT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = compare_values(arg0, arg1) == ORD_GREATER,
                        }
                    };
                } break;
                case TK_LEQ: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    Ordering ord = compare_values(arg0, arg1);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = ord == ORD_LESS || ord == ORD_EQ,
                        }
                    };
                } break;
                case TK_GEQ: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    if (!coerce(&arg1, arg0.kind)) PANIC("Cannot compare type %s to type %s", vk_names[arg1.kind], vk_names[arg0.kind]);
                    Ordering ord = compare_values(arg0, arg1);
                    return (Value) {
                        .kind = VK_BOOL,
                        .value = {
                            .integer = ord == ORD_GREATER || ord == ORD_EQ,
                        }
                    };
                } break;

                case TK_DOT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count != 2) {
                        PANIC("Expected two arguments, got %ld", fn.args.count);
                    }
                    Value arg0 = eval(fn.args.items[0], ctx);
                    Value arg1 = eval(fn.args.items[1], ctx);
                    switch (arg0.kind) {
                        case VK_UNIT:
                        case VK_INT:
                        case VK_BOOL:
                        case VK_FUNCTION:
                        case VK_CHAR:
                        case VK_NATIVE_FUNCTION:
                            PANIC("Cannot index into %s", vk_names[arg0.kind]);
                        case VK_STRING: {
                            String string = arg0.value.string;
                            if (arg1.kind != VK_INT) PANIC("Cannot index into %s with type %s", vk_names[arg0.kind], vk_names[arg1.kind]);
                            int n = arg1.value.integer;
                            if (n < 0 || n > string.count) PANIC("Index %d out of bounds for length %ld", n, string.count);
                            return (Value) {
                                .kind = VK_CHAR,
                                .value = {
                                    .character = string.items[n],
                                },
                            };
                        } break;
                        case VK_ARRAY: {
                            ValueArray array = arg0.value.array;
                            if (arg1.kind != VK_INT) PANIC("Cannot index into %s with type %s", vk_names[arg0.kind], vk_names[arg1.kind]);
                            int n = arg1.value.integer;
                            if (n < 0 || n > array.count) PANIC("Index %d out of bounds for length %ld", n, array.count);
                            return array.items[n];
                        } break;
                        case __VK_LENGTH:
                            break;
                    }
                } break;
                case TK_AT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    Value ret = {
                        .kind = VK_ARRAY,
                        .value = {
                            .array = { 0 },
                        },
                    };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        da_append(&ret.value.array, eval(fn.args.items[i], ctx));
                    }
                    return ret;
                } break;
                case TK_EVAL: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 1) PANIC("Eval operation must have at least on expression");
                    Value ret = { 0 };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        ret = eval(fn.args.items[i], ctx);
                    }
                    return ret;
                } break;
                case TK_PLUS: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 1) PANIC("Add operation must contain at least one value.");
                    Value out = { 0 };
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        add_value(&out, eval(fn.args.items[i], ctx));
                    }
                    return out;
                } break;
                case TK_MINUS: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 2) PANIC("Subtract operation must contain at least two values.");
                    Value out = eval(fn.args.items[0], ctx);
                    for (size_t i = 1; i < fn.args.count; ++i) {
                        sub_value(&out, eval(fn.args.items[i], ctx));
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
                        mult_value(&out, eval(fn.args.items[i], ctx));
                    }
                    return out;
                } break;
                case TK_SLASH: {
                    FunctionCallValue fn = ast.value.fn_call;
                    if (fn.args.count < 2) PANIC("Subtract operation must contain at least two values.");
                    Value out = eval(fn.args.items[0], ctx);
                    for (size_t i = 1; i < fn.args.count; ++i) {
                        div_value(&out, eval(fn.args.items[i], ctx));
                    }
                    return out;
                } break;
                case TK_IDENT: {
                    FunctionCallValue fn = ast.value.fn_call;
                    const char *name = fn.op.value.ident;
                    Value *var = get_var(ctx, name);
                    if (var == NULL) {
                        PANIC("Unknown function '%s'", name);
                    }
                    if (var->kind != VK_FUNCTION && var->kind != VK_NATIVE_FUNCTION) {
                        PANIC("Variable '%s' is not a function.", name);
                    }
                    Value args[fn.args.count];
                    for (size_t i = 0; i < fn.args.count; ++i) {
                        args[i] = eval(fn.args.items[i], ctx);
                    }
                    return apply_fn(ctx, name, *var, fn.args.count, args);
                } break;
            }
        } break;
        case EK_FOR: {
            ForValue f = ast.value.for_;
            eval(*f.init, ctx);
            for (;;) {
                // if condition is (), then we pretend it's `true`, like C does.
                if (f.cond->kind != EK_UNIT) {
                    Value cond = eval(*f.cond, ctx);
                    if (!value_to_bool(cond)) break;
                }
                eval(*f.body, ctx);
                eval(*f.post, ctx);
            }

            return (Value) { 0 };
        } break;
        case EK_WHILE: {
            WhileValue w = ast.value.while_;
            for (;;) {
                Value cond = eval(*w.cond, ctx);
                if (!value_to_bool(cond)) break;
                eval(*w.body, ctx);
            }

            return (Value) { 0 };
        } break;
        case EK_IF: {
            IfValue if_ = ast.value.if_;
            Value cond = eval(*if_.cond, ctx);
            if (value_to_bool(cond)) {
                return eval(*if_.true_branch, ctx);
            }

            if (if_.false_branch) {
                return eval(*if_.false_branch, ctx);
            }

            return (Value) { 0 };
        } break;
        case EK_FUNCTION_DEF: {
            return (Value) {
                .kind = VK_FUNCTION,
                .value.fn = ast.value.fn_def,
            };
        } break;
        case EK_DECLARE_VAR: {
            DeclareAssign dec = ast.value.declare_assign;
            Value *var = add_var(ctx->parent, dec.name);
            if (dec.value) {
                *var = eval(*dec.value, ctx);
            }
            return (Value) { 0 };
        } break;
        case EK_ASSIGN_VAR: {
            DeclareAssign ass = ast.value.declare_assign;
            Value *var = get_var(ctx->parent, ass.name);
            if (var == NULL) {
                PANIC("Variable '%s' does not exist in ctx.", ass.name);
            }
            if (var->immutable) {
                PANIC("Variable '%s' is immutable.", ass.name);
            }
            return *var = eval(*ass.value, ctx);
        } break;
    }
    PANIC("Unknown expression kind: '%s'", ek_names[ast.kind]);
}

Value eval(AST ast, EvalContext *parent_ctx) {
    EvalContext ctx = create_ctx(parent_ctx);
    Value ret = eval_in_ctx(ast, &ctx);
    free_ctx(ctx);
    return ret;
}

Value native_print(EvalContext *ctx, size_t argc, Value *argv) {
    for (size_t i = 0; i < argc; ++i) {
        if (i != 0) printf(" ");
        printf("%s", value_to_string(argv[i]));
    }
    return (Value) { 0 };
}

Value native_println(EvalContext *ctx, size_t argc, Value *argv) {
    Value ret = native_print(ctx, argc, argv);
    printf("\n");
    return ret;
}

Value native_parseint(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    Value arg = argv[0];
    if (arg.kind != VK_STRING) {
        PANIC("parseint accepts one string as its argument, found %s.", vk_names[arg.kind]);
    }

    int value = atoi(arg.value.string.items);
    return (Value) {
        .kind = VK_INT,
        .value.integer = value,
    };
}

Value native_readline(EvalContext *ctx, size_t argc, Value *_argv) {
    assert(argc == 0);
    char *line = NULL;
    size_t n = 0;
    ssize_t r = getline(&line, &n, stdin);
    if (r < 0) PANIC("Unexpeced error while running readline: %m");
    line[--r] = '\0'; // remove newline
    return (Value) {
        .kind = VK_STRING,
        .value.string = {
            .items = line,
            .count = r,
        }
    };
}

Value native_append(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc >= 2);
    if (argv[0].kind != VK_ARRAY) PANIC("Argument one of append must be an array");
    ValueArray va = argv[0].value.array;
    va.capacity = va.count + argc - 1;
    Value *new_items = malloc(sizeof(Value) * va.capacity);
    va.items = va.count == 0 ? new_items : memcpy(new_items, va.items, sizeof(Value) * va.count);
    memcpy(va.items + va.count, argv + 1, sizeof(Value) * (argc - 1));
    va.count += argc - 1;
    return (Value) {
        .kind = VK_ARRAY,
        .value.array = va
    };
}

Value native_length(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    int n;
    switch (argv[0].kind) {
        case VK_STRING:
            n = argv[0].value.string.count;
            break;
        case VK_ARRAY:
            n = argv[0].value.array.count;
            break;
        case VK_UNIT:
        case VK_INT:
        case VK_CHAR:
        case VK_BOOL:
        case VK_FUNCTION:
        case VK_NATIVE_FUNCTION:
        case __VK_LENGTH:
            PANIC("Cannot get length of type %s.", vk_names[argv[0].kind]);
            break;
    }
    return (Value) {
        .kind = VK_INT,
        .value.integer = n
    };
}

Value native_int(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    Value v = argv[0];
    if (!coerce(&v, VK_INT)) PANIC("Cannot cast type %s to INT.", vk_names[v.kind]);
    return v;
}

Value native_string(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    return (Value) {
        .kind = VK_STRING,
        .value.string = new_string(value_to_string(argv[0]))
    };
}

Value native_char(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    Value v = argv[0];
    if (!coerce(&v, VK_CHAR)) PANIC("Cannot cast type %s to CHAR.", vk_names[v.kind]);
    return v;
}

Value native_bool(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 1);
    Value v = argv[0];
    if (!coerce(&v, VK_BOOL)) PANIC("Cannot cast type %s to BOOL.", vk_names[v.kind]);
    return v;
}

Value native_map(EvalContext *ctx, size_t argc, Value *argv) {
    assert(argc == 2);
    Value varray = argv[0];
    if (varray.kind != VK_ARRAY) PANIC("Argument one of append must be an array");
    ValueArray array = varray.value.array;
    Value mapper = argv[1];
    if (mapper.kind != VK_FUNCTION && mapper.kind != VK_NATIVE_FUNCTION) PANIC("Mapper must be a function");
    ValueArray out = { 0 };
    out.items = malloc(sizeof(Value) * (out.capacity = array.count));

    for (size_t i = 0; i < array.count; ++i) {
        out.items[i] = apply_fn(ctx, "mapper", mapper, 1, &array.items[i]);
    }
    out.count = array.count;

    return (Value) {
        .kind = VK_ARRAY,
        .value.array = out,
    };
}

#define ADD_FN(fn_name, native_fn, min_argc, max_argc) \
    set_var(&ctx, #fn_name, (Value) {  \
        .kind = VK_NATIVE_FUNCTION,      \
        .value = {                       \
            .native = {                  \
                .name = #fn_name,        \
                .min_args = min_argc,   \
                .max_args = max_argc,   \
                .fn = native_fn,         \
            }                            \
        },                               \
        .immutable = true                \
    });                                  \

EvalContext create_global_ctx() {
    EvalContext ctx = create_ctx(NULL);
    ADD_FN(print, native_print, -1, -1);
    ADD_FN(println, native_println, -1, -1);
    ADD_FN(parseint, native_parseint, 1, -1);
    ADD_FN(readline, native_readline, 0, 0);

    ADD_FN(append, native_append, 2, -1);
    ADD_FN(length, native_length, 1, 1);
    ADD_FN(map, native_map, 2, 2);

    ADD_FN(int, native_int, 1, 1);
    ADD_FN(char, native_char, 1, 1);
    ADD_FN(string, native_string, 1, 1);
    ADD_FN(bool, native_bool, 1, 1);
    return ctx;
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
    AST ast = parse(file, "expression");
    Token tok = take_token(file);
    if (tok.kind != TK_EOF) {
        ERROR("Expected EOF, found %s", token_string(tok));
    }
    fclose(file);
    print_ast(&ast, 0);

    EvalContext global_ctx = create_global_ctx();
    eval(ast, &global_ctx);
}

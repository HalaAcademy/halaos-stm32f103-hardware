/* SPDX-FileCopyrightText: 2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    hala_compiler.c
 * @brief   Lexer, parser và HBC emitter cho Hala-C educational subset.
 * @details Compiler dùng recursive descent, symbol table cố định và backpatch jump/call. Source
 *          không được nhận diện theo tên application. Mọi workspace đều là fixed-size để phù hợp
 *          STM32F103C8 và giúp kiểm thử giới hạn tài nguyên xác định.
 */
#include "halaos/internal/halaos_internal.h"

#define HC_MAX_TOKENS 80u
#define HC_MAX_FUNCTIONS 6u
#define HC_MAX_LOCALS 8u
#define HC_MAX_CALL_PATCHES 12u
#define HC_MAX_CODE 256u
#define HC_NAME_SIZE 12u

typedef enum {
    TK_EOF = 0,
    TK_INT,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_WRITE,
    TK_PRINT,
    TK_SLEEP,
    TK_GPIO,
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_SEMI,
    TK_COMMA,
    TK_ASSIGN,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_EQ,
    TK_NE
} HcTokenKind;

typedef struct {
    u8 kind;
    u8 length;
    u16 position;
    u16 line;
    u16 column;
    i32 value;
} HcToken;

typedef struct {
    char name[HC_NAME_SIZE];
    u16 address;
    u8 defined;
} HcFunction;
typedef struct {
    char name[HC_NAME_SIZE];
    u8 index;
} HcLocal;
typedef struct {
    char name[HC_NAME_SIZE];
    u16 operand_at;
} HcCallPatch;

typedef struct {
    const char *source;
    u32 source_length;
    HcToken tokens[HC_MAX_TOKENS];
    u8 token_count;
    u8 current;
    u8 failed;
    u8 code[HC_MAX_CODE];
    u16 code_size;
    HcFunction functions[HC_MAX_FUNCTIONS];
    u8 function_count;
    HcLocal locals[HC_MAX_LOCALS];
    u8 local_count;
    HcCallPatch calls[HC_MAX_CALL_PATCHES];
    u8 call_count;
} HcCompiler;

static HcCompiler g_hc;
volatile u32 g_compile_error_line;
volatile u32 g_compile_error_column;
volatile i32 g_compile_error_code;
volatile u32 g_compiler_tokens;
volatile u32 g_compiler_functions;
volatile u32 g_compiler_symbols;

static void hc_copy_name(char *destination, const char *source, u32 length) {
    u32 count = length;
    if (count >= HC_NAME_SIZE)
        count = HC_NAME_SIZE - 1u;
    for (u32 i = 0u; i < count; ++i)
        destination[i] = source[i];
    destination[count] = '\0';
}

static int hc_name_equal(const char *left, const char *right) {
    u32 index = 0u;
    while (left[index] == right[index]) {
        if (left[index] == '\0')
            return 1;
        index++;
    }
    return 0;
}

static int hc_token_text_equal(const HcToken *token, const char *text) {
    u32 length = str_len(text);
    if (length != token->length)
        return 0;
    for (u32 i = 0u; i < length; ++i) {
        if (g_hc.source[token->position + i] != text[i])
            return 0;
    }
    return 1;
}

static void hc_fail(const HcToken *token, i32 code) {
    if (g_hc.failed)
        return;
    g_hc.failed = 1u;
    g_compile_error_code = code;
    g_compile_error_line = token != NULL ? token->line : 1u;
    g_compile_error_column = token != NULL ? token->column : 1u;
}

static u8 hc_keyword_kind(u32 position, u32 length) {
    HcToken temporary = {TK_IDENT, (u8)length, (u16)position, 0u, 0u, 0};
    if (hc_token_text_equal(&temporary, "int"))
        return TK_INT;
    if (hc_token_text_equal(&temporary, "return"))
        return TK_RETURN;
    if (hc_token_text_equal(&temporary, "if"))
        return TK_IF;
    if (hc_token_text_equal(&temporary, "else"))
        return TK_ELSE;
    if (hc_token_text_equal(&temporary, "while"))
        return TK_WHILE;
    if (hc_token_text_equal(&temporary, "write"))
        return TK_WRITE;
    if (hc_token_text_equal(&temporary, "print"))
        return TK_PRINT;
    if (hc_token_text_equal(&temporary, "sleep"))
        return TK_SLEEP;
    if (hc_token_text_equal(&temporary, "gpio_toggle"))
        return TK_GPIO;
    return TK_IDENT;
}

static int hc_add_token(u8 kind, u32 position, u32 length, u32 line, u32 column, i32 value) {
    if (g_hc.token_count >= HC_MAX_TOKENS) {
        HcToken overflow = {kind, 0u, (u16)position, (u16)line, (u16)column, 0};
        hc_fail(&overflow, -61);
        return 0;
    }
    HcToken *token = &g_hc.tokens[g_hc.token_count++];
    token->kind = kind;
    token->length = (u8)length;
    token->position = (u16)position;
    token->line = (u16)line;
    token->column = (u16)column;
    token->value = value;
    return 1;
}

/** @brief Chuyển source thành token stream, có theo dõi dòng/cột và comment C cơ bản. */
static int hc_lex(void) {
    u32 position = 0u, line = 1u, column = 1u;
    while (position < g_hc.source_length) {
        char character = g_hc.source[position];
        if ((character == ' ') || (character == '\t') || (character == '\r')) {
            position++;
            column++;
            continue;
        }
        if (character == '\n') {
            position++;
            line++;
            column = 1u;
            continue;
        }

        /* Bỏ comment nhưng vẫn cập nhật dòng/cột để diagnostic giữ đúng vị trí source. */
        if ((character == '/') && (position + 1u < g_hc.source_length) &&
            (g_hc.source[position + 1u] == '/')) {
            position += 2u;
            column += 2u;
            while ((position < g_hc.source_length) && (g_hc.source[position] != '\n')) {
                position++;
                column++;
            }
            continue;
        }
        if ((character == '/') && (position + 1u < g_hc.source_length) &&
            (g_hc.source[position + 1u] == '*')) {
            u32 start_line = line, start_column = column;
            position += 2u;
            column += 2u;
            while ((position + 1u < g_hc.source_length) &&
                   !((g_hc.source[position] == '*') && (g_hc.source[position + 1u] == '/'))) {
                if (g_hc.source[position] == '\n') {
                    line++;
                    column = 1u;
                    position++;
                } else {
                    position++;
                    column++;
                }
            }
            if (position + 1u >= g_hc.source_length) {
                HcToken token = {TK_EOF, 0u, (u16)position, (u16)start_line, (u16)start_column, 0};
                hc_fail(&token, -62);
                return 0;
            }
            position += 2u;
            column += 2u;
            continue;
        }

        if (((character >= 'a') && (character <= 'z')) ||
            ((character >= 'A') && (character <= 'Z')) || character == '_') {
            u32 start = position, start_column = column;
            while (position < g_hc.source_length) {
                char c = g_hc.source[position];
                if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) ||
                      ((c >= '0') && (c <= '9')) || c == '_'))
                    break;
                position++;
                column++;
            }
            u32 length = position - start;
            if (length >= HC_NAME_SIZE) {
                HcToken token = {TK_IDENT, (u8)length, (u16)start, (u16)line, (u16)start_column, 0};
                hc_fail(&token, -63);
                return 0;
            }
            if (!hc_add_token(hc_keyword_kind(start, length), start, length, line, start_column, 0))
                return 0;
            continue;
        }

        if ((character >= '0') && (character <= '9')) {
            u32 start = position, start_column = column;
            i32 value = 0;
            while ((position < g_hc.source_length) && (g_hc.source[position] >= '0') &&
                   (g_hc.source[position] <= '9')) {
                if (value > 214748364) {
                    HcToken token = {TK_NUMBER, 0u, (u16)start, (u16)line, (u16)start_column, 0};
                    hc_fail(&token, -64);
                    return 0;
                }
                value = value * 10 + (g_hc.source[position] - '0');
                position++;
                column++;
            }
            if (!hc_add_token(TK_NUMBER, start, position - start, line, start_column, value))
                return 0;
            continue;
        }

        if (character == '"') {
            u32 start = ++position, start_column = column;
            column++;
            u32 decoded = 0u;
            while ((position < g_hc.source_length) && (g_hc.source[position] != '"')) {
                if (g_hc.source[position] == '\n') {
                    HcToken token = {TK_STRING, 0u, (u16)start, (u16)line, (u16)start_column, 0};
                    hc_fail(&token, -65);
                    return 0;
                }
                if ((g_hc.source[position] == '\\') && (position + 1u < g_hc.source_length)) {
                    position += 2u;
                    column += 2u;
                } else {
                    position++;
                    column++;
                }
                decoded++;
                if (decoded > 60u) {
                    HcToken token = {TK_STRING, 0u, (u16)start, (u16)line, (u16)start_column, 0};
                    hc_fail(&token, -66);
                    return 0;
                }
            }
            if (position >= g_hc.source_length) {
                HcToken token = {TK_STRING, 0u, (u16)start, (u16)line, (u16)start_column, 0};
                hc_fail(&token, -65);
                return 0;
            }
            if (!hc_add_token(TK_STRING, start, position - start, line, start_column, 0))
                return 0;
            position++;
            column++;
            continue;
        }

        u8 kind = TK_EOF;
        u32 length = 1u;
        if ((position + 1u < g_hc.source_length) && character == '=' &&
            g_hc.source[position + 1u] == '=') {
            kind = TK_EQ;
            length = 2u;
        } else if ((position + 1u < g_hc.source_length) && character == '!' &&
                   g_hc.source[position + 1u] == '=') {
            kind = TK_NE;
            length = 2u;
        } else if ((position + 1u < g_hc.source_length) && character == '<' &&
                   g_hc.source[position + 1u] == '=') {
            kind = TK_LE;
            length = 2u;
        } else if ((position + 1u < g_hc.source_length) && character == '>' &&
                   g_hc.source[position + 1u] == '=') {
            kind = TK_GE;
            length = 2u;
        } else if (character == '(')
            kind = TK_LPAREN;
        else if (character == ')')
            kind = TK_RPAREN;
        else if (character == '{')
            kind = TK_LBRACE;
        else if (character == '}')
            kind = TK_RBRACE;
        else if (character == ';')
            kind = TK_SEMI;
        else if (character == ',')
            kind = TK_COMMA;
        else if (character == '=')
            kind = TK_ASSIGN;
        else if (character == '+')
            kind = TK_PLUS;
        else if (character == '-')
            kind = TK_MINUS;
        else if (character == '*')
            kind = TK_STAR;
        else if (character == '/')
            kind = TK_SLASH;
        else if (character == '<')
            kind = TK_LT;
        else if (character == '>')
            kind = TK_GT;
        else {
            HcToken token = {TK_EOF, 0u, (u16)position, (u16)line, (u16)column, 0};
            hc_fail(&token, -67);
            return 0;
        }
        if (!hc_add_token(kind, position, length, line, column, 0))
            return 0;
        position += length;
        column += length;
    }
    return hc_add_token(TK_EOF, position, 0u, line, column, 0);
}

static HcToken *hc_peek(void) { return &g_hc.tokens[g_hc.current]; }
static HcToken *hc_previous(void) { return &g_hc.tokens[g_hc.current - 1u]; }
static int hc_match(u8 kind) {
    if (hc_peek()->kind != kind)
        return 0;
    g_hc.current++;
    return 1;
}
static HcToken *hc_expect(u8 kind, i32 code) {
    if (hc_peek()->kind != kind) {
        hc_fail(hc_peek(), code);
        return hc_peek();
    }
    g_hc.current++;
    return hc_previous();
}

static int hc_emit_u8(u8 value) {
    if (g_hc.code_size >= HC_MAX_CODE) {
        hc_fail(hc_peek(), -68);
        return 0;
    }
    g_hc.code[g_hc.code_size++] = value;
    return 1;
}
static int hc_emit_u16(u16 value) { return hc_emit_u8((u8)value) && hc_emit_u8((u8)(value >> 8)); }
static int hc_emit_i32(i32 value) {
    return hc_emit_u8((u8)value) && hc_emit_u8((u8)((u32)value >> 8)) &&
           hc_emit_u8((u8)((u32)value >> 16)) && hc_emit_u8((u8)((u32)value >> 24));
}
static u16 hc_emit_jump(u8 opcode) {
    if (!hc_emit_u8(opcode))
        return 0u;
    u16 operand = g_hc.code_size;
    (void)hc_emit_u16(0u);
    return operand;
}
static void hc_patch_u16(u16 operand, u16 value) {
    if ((u32)operand + 1u >= g_hc.code_size) {
        hc_fail(hc_peek(), -69);
        return;
    }
    g_hc.code[operand] = (u8)value;
    g_hc.code[operand + 1u] = (u8)(value >> 8);
}

static int hc_find_local(const HcToken *name) {
    char text[HC_NAME_SIZE];
    hc_copy_name(text, g_hc.source + name->position, name->length);
    for (u32 i = 0u; i < g_hc.local_count; ++i)
        if (hc_name_equal(text, g_hc.locals[i].name))
            return (int)i;
    return -1;
}
static int hc_add_local(const HcToken *name) {
    if (hc_find_local(name) >= 0) {
        hc_fail(name, -70);
        return -1;
    }
    if (g_hc.local_count >= HC_MAX_LOCALS) {
        hc_fail(name, -71);
        return -1;
    }
    HcLocal *local = &g_hc.locals[g_hc.local_count];
    hc_copy_name(local->name, g_hc.source + name->position, name->length);
    local->index = g_hc.local_count;
    g_hc.local_count++;
    return local->index;
}
static int hc_find_function_name(const char *name) {
    for (u32 i = 0u; i < g_hc.function_count; ++i)
        if (hc_name_equal(name, g_hc.functions[i].name))
            return (int)i;
    return -1;
}
static int hc_add_function(const HcToken *name, u16 address) {
    char text[HC_NAME_SIZE];
    hc_copy_name(text, g_hc.source + name->position, name->length);
    if (hc_find_function_name(text) >= 0) {
        hc_fail(name, -72);
        return -1;
    }
    if (g_hc.function_count >= HC_MAX_FUNCTIONS) {
        hc_fail(name, -73);
        return -1;
    }
    HcFunction *function = &g_hc.functions[g_hc.function_count];
    hc_copy_name(function->name, text, str_len(text));
    function->address = address;
    function->defined = 1u;
    return g_hc.function_count++;
}
static void hc_add_call_patch(const HcToken *name, u16 operand) {
    if (g_hc.call_count >= HC_MAX_CALL_PATCHES) {
        hc_fail(name, -74);
        return;
    }
    HcCallPatch *patch = &g_hc.calls[g_hc.call_count++];
    hc_copy_name(patch->name, g_hc.source + name->position, name->length);
    patch->operand_at = operand;
}

static void hc_expression(void);

static void hc_primary(void) {
    if (hc_match(TK_NUMBER)) {
        (void)hc_emit_u8(HBC_PUSH_I32);
        (void)hc_emit_i32(hc_previous()->value);
        return;
    }
    if (hc_match(TK_IDENT)) {
        HcToken *name = hc_previous();
        if (hc_match(TK_LPAREN)) {
            (void)hc_expect(TK_RPAREN, -75);
            (void)hc_emit_u8(HBC_CALL);
            u16 operand = g_hc.code_size;
            (void)hc_emit_u16(0u);
            hc_add_call_patch(name, operand);
            return;
        }
        int index = hc_find_local(name);
        if (index < 0) {
            hc_fail(name, -76);
            return;
        }
        (void)hc_emit_u8(HBC_LOAD_LOCAL);
        (void)hc_emit_u8((u8)index);
        return;
    }
    if (hc_match(TK_LPAREN)) {
        hc_expression();
        (void)hc_expect(TK_RPAREN, -77);
        return;
    }
    if (hc_match(TK_MINUS)) {
        (void)hc_emit_u8(HBC_PUSH_I32);
        (void)hc_emit_i32(0);
        hc_primary();
        (void)hc_emit_u8(HBC_SUB);
        return;
    }
    hc_fail(hc_peek(), -78);
}

static void hc_factor(void) {
    hc_primary();
    while (!g_hc.failed && ((hc_peek()->kind == TK_STAR) || (hc_peek()->kind == TK_SLASH))) {
        u8 operator_kind = hc_peek()->kind;
        g_hc.current++;
        hc_primary();
        (void)hc_emit_u8(operator_kind == TK_STAR ? HBC_MUL : HBC_DIV);
    }
}
static void hc_term(void) {
    hc_factor();
    while (!g_hc.failed && ((hc_peek()->kind == TK_PLUS) || (hc_peek()->kind == TK_MINUS))) {
        u8 operator_kind = hc_peek()->kind;
        g_hc.current++;
        hc_factor();
        (void)hc_emit_u8(operator_kind == TK_PLUS ? HBC_ADD : HBC_SUB);
    }
}
static void hc_expression(void) {
    hc_term();
    if (g_hc.failed)
        return;
    u8 operator_kind = hc_peek()->kind;
    if ((operator_kind >= TK_LT) && (operator_kind <= TK_NE)) {
        g_hc.current++;
        hc_term();
        const u8 opcodes[] = {HBC_LT, HBC_LE, HBC_GT, HBC_GE, HBC_EQ, HBC_NE};
        (void)hc_emit_u8(opcodes[operator_kind - TK_LT]);
    }
}

static void hc_block(void);

static void hc_emit_string(const HcToken *token) {
    u16 length_position;
    u8 decoded_length = 0u;
    (void)hc_emit_u8(HBC_WRITE);
    length_position = g_hc.code_size;
    (void)hc_emit_u8(0u);
    for (u32 i = 0u; i < token->length; ++i) {
        char character = g_hc.source[token->position + i];
        if (character == '\\') {
            if (++i >= token->length) {
                hc_fail(token, -79);
                return;
            }
            char escaped = g_hc.source[token->position + i];
            if (escaped == 'n')
                character = '\n';
            else if (escaped == 'r')
                character = '\r';
            else if (escaped == 't')
                character = '\t';
            else if (escaped == '"')
                character = '"';
            else if (escaped == '\\')
                character = '\\';
            else {
                hc_fail(token, -79);
                return;
            }
        }
        (void)hc_emit_u8((u8)character);
        decoded_length++;
    }
    g_hc.code[length_position] = decoded_length;
}

static void hc_statement(void) {
    if (hc_match(TK_INT)) {
        HcToken *name = hc_expect(TK_IDENT, -80);
        int index = hc_add_local(name);
        if (hc_match(TK_ASSIGN))
            hc_expression();
        else {
            (void)hc_emit_u8(HBC_PUSH_I32);
            (void)hc_emit_i32(0);
        }
        (void)hc_expect(TK_SEMI, -81);
        if (index >= 0) {
            (void)hc_emit_u8(HBC_STORE_LOCAL);
            (void)hc_emit_u8((u8)index);
        }
        return;
    }
    if (hc_match(TK_RETURN)) {
        hc_expression();
        (void)hc_expect(TK_SEMI, -82);
        (void)hc_emit_u8(HBC_RET);
        return;
    }
    if (hc_match(TK_IF)) {
        (void)hc_expect(TK_LPAREN, -83);
        hc_expression();
        (void)hc_expect(TK_RPAREN, -84);
        u16 false_operand = hc_emit_jump(HBC_JZ);
        hc_block();
        if (hc_match(TK_ELSE)) {
            u16 end_operand = hc_emit_jump(HBC_JMP);
            hc_patch_u16(false_operand, g_hc.code_size);
            hc_block();
            hc_patch_u16(end_operand, g_hc.code_size);
        } else
            hc_patch_u16(false_operand, g_hc.code_size);
        return;
    }
    if (hc_match(TK_WHILE)) {
        u16 loop_start = g_hc.code_size;
        (void)hc_expect(TK_LPAREN, -85);
        hc_expression();
        (void)hc_expect(TK_RPAREN, -86);
        u16 exit_operand = hc_emit_jump(HBC_JZ);
        hc_block();
        (void)hc_emit_u8(HBC_JMP);
        (void)hc_emit_u16(loop_start);
        hc_patch_u16(exit_operand, g_hc.code_size);
        return;
    }
    if (hc_match(TK_WRITE)) {
        (void)hc_expect(TK_LPAREN, -87);
        HcToken *fd = hc_expect(TK_NUMBER, -88);
        if (fd->value != 1)
            hc_fail(fd, -89);
        (void)hc_expect(TK_COMMA, -90);
        HcToken *string = hc_expect(TK_STRING, -91);
        if (hc_match(TK_COMMA))
            (void)hc_expect(TK_NUMBER, -92); /* Length is accepted for C-like syntax. */
        (void)hc_expect(TK_RPAREN, -93);
        (void)hc_expect(TK_SEMI, -94);
        hc_emit_string(string);
        return;
    }
    if (hc_match(TK_PRINT)) {
        (void)hc_expect(TK_LPAREN, -95);
        hc_expression();
        (void)hc_expect(TK_RPAREN, -96);
        (void)hc_expect(TK_SEMI, -97);
        (void)hc_emit_u8(HBC_PRINT_INT);
        return;
    }
    if (hc_match(TK_SLEEP)) {
        (void)hc_expect(TK_LPAREN, -98);
        HcToken *duration = hc_expect(TK_NUMBER, -99);
        if ((duration->value < 0) || (duration->value > 65535))
            hc_fail(duration, -100);
        (void)hc_expect(TK_RPAREN, -101);
        (void)hc_expect(TK_SEMI, -102);
        (void)hc_emit_u8(HBC_SLEEP);
        (void)hc_emit_u16((u16)duration->value);
        return;
    }
    if (hc_match(TK_GPIO)) {
        (void)hc_expect(TK_LPAREN, -103);
        (void)hc_expect(TK_RPAREN, -104);
        (void)hc_expect(TK_SEMI, -105);
        (void)hc_emit_u8(HBC_GPIO);
        return;
    }
    if (hc_match(TK_LBRACE)) {
        while (!g_hc.failed && (hc_peek()->kind != TK_RBRACE) && (hc_peek()->kind != TK_EOF))
            hc_statement();
        (void)hc_expect(TK_RBRACE, -106);
        return;
    }
    if (hc_peek()->kind == TK_IDENT) {
        HcToken *name = hc_peek();
        g_hc.current++;
        if (hc_match(TK_ASSIGN)) {
            int index = hc_find_local(name);
            if (index < 0) {
                hc_fail(name, -76);
                return;
            }
            hc_expression();
            (void)hc_expect(TK_SEMI, -107);
            (void)hc_emit_u8(HBC_STORE_LOCAL);
            (void)hc_emit_u8((u8)index);
            return;
        }
        if (hc_match(TK_LPAREN)) {
            (void)hc_expect(TK_RPAREN, -108);
            (void)hc_expect(TK_SEMI, -109);
            (void)hc_emit_u8(HBC_CALL);
            u16 operand = g_hc.code_size;
            (void)hc_emit_u16(0u);
            hc_add_call_patch(name, operand);
            (void)hc_emit_u8(HBC_POP);
            return;
        }
        hc_fail(name, -110);
        return;
    }
    hc_fail(hc_peek(), -111);
}

static void hc_block(void) {
    (void)hc_expect(TK_LBRACE, -112);
    while (!g_hc.failed && (hc_peek()->kind != TK_RBRACE) && (hc_peek()->kind != TK_EOF))
        hc_statement();
    (void)hc_expect(TK_RBRACE, -113);
}

static void hc_function(void) {
    (void)hc_expect(TK_INT, -114);
    HcToken *name = hc_expect(TK_IDENT, -115);
    (void)hc_expect(TK_LPAREN, -116);
    (void)hc_expect(TK_RPAREN, -117);
    if (g_hc.failed)
        return;
    (void)hc_add_function(name, g_hc.code_size);
    g_hc.local_count = 0u;
    hc_block();
    /* Function không có return tường minh nhận giá trị 0 theo educational contract. */
    if (!g_hc.failed && ((g_hc.code_size == 0u) || (g_hc.code[g_hc.code_size - 1u] != HBC_RET))) {
        (void)hc_emit_u8(HBC_PUSH_I32);
        (void)hc_emit_i32(0);
        (void)hc_emit_u8(HBC_RET);
    }
}

static int hc_patch_calls(void) {
    for (u32 i = 0u; i < g_hc.call_count; ++i) {
        int function = hc_find_function_name(g_hc.calls[i].name);
        if (function < 0) {
            hc_fail(hc_peek(), -118);
            return 0;
        }
        hc_patch_u16(g_hc.calls[i].operand_at, g_hc.functions[function].address);
    }
    return !g_hc.failed;
}

/**
 * @brief Compile một chương trình Hala-C mới không phụ thuộc tên application.
 * @return 0 khi bytecode đã được verifier chấp nhận và commit vào app store; mã âm khi lỗi.
 */
int hala_compile_general(const char *source, u32 length, const char *application_name) {
    mem_zero(&g_hc, sizeof(g_hc));
    g_hc.source = source;
    g_hc.source_length = length;
    g_compile_error_line = 0u;
    g_compile_error_column = 0u;
    g_compile_error_code = 0;

    if ((source == NULL) || (length == 0u) || (length >= 300u))
        return -60;
    if (!hc_lex())
        return g_compile_error_code;

    /* Entry stub gọi main rồi HALT; địa chỉ main được backpatch như mọi function call. */
    (void)hc_emit_u8(HBC_CALL);
    u16 entry_operand = g_hc.code_size;
    (void)hc_emit_u16(0u);
    (void)hc_emit_u8(HBC_HALT);
    HcToken synthetic_main = {TK_IDENT, 4u, 0u, 1u, 1u, 0};
    HcCallPatch *entry = &g_hc.calls[g_hc.call_count++];
    hc_copy_name(entry->name, "main", 4u);
    entry->operand_at = entry_operand;
    (void)synthetic_main;

    while (!g_hc.failed && (hc_peek()->kind != TK_EOF))
        hc_function();
    if (!g_hc.failed && (hc_find_function_name("main") < 0))
        hc_fail(hc_peek(), -119);
    if (!g_hc.failed)
        (void)hc_patch_calls();
    if (g_hc.failed)
        return g_compile_error_code;

    g_compiler_tokens = g_hc.token_count;
    g_compiler_functions = g_hc.function_count;
    g_compiler_symbols = g_hc.function_count + g_hc.local_count;
    return app_write(g_hc.code, g_hc.code_size, APP_TYPE_BYTECODE, application_name);
}

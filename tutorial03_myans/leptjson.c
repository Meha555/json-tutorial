#include "leptjson.h"
#include <assert.h> /* assert() */
#include <errno.h>  /* errno, ERANGE */
#include <math.h>   /* HUGE_VAL */
#include <stdlib.h> /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h> /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)             \
    do {                          \
        assert(*c->json == (ch)); \
        c->json++;                \
    } while (0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)                                        \
    do {                                                   \
        *(char*)lept_context_push(c, sizeof(char)) = (ch); \
    } while (0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
} lept_context;

// 压入字符串到缓冲区，同时自动为缓冲区扩容，返回新栈顶的指针
static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {                    // 如果确实空间不够，需要扩容
        if (c->size == 0)                              // 对于初始的NULL字符串的情况予以特判
            c->size = LEPT_PARSE_STACK_INIT_SIZE;      // 分配默认的最初缓冲区大小
        while (c->top + size >= c->size)               // 用循环确定缓冲区扩容后的大小，直到能装下为止
            c->size += c->size >> 1;                   /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);  // 扩容
    }
    ret = c->stack + c->top;  // 此时的c->stack已经是新指针了
    c->top += size;           // 更新大小
    return ret;
}

// 从缓冲区弹出size个字符（实际上是直接更改栈顶指针指向）
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
    const char* p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    if (*p == '-')
        p++;
    if (*p == '0')
        p++;
    else {
        if (!ISDIGIT1TO9(*p))
            return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++)
            ;
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p))
            return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++)
            ;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!ISDIGIT(*p))
            return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++)
            ;
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

// string的parser
static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    const char* p;
    EXPECT(c, '\"');  // 识别并去掉包裹字符串开头的 "
    p = c->json;
    for (;;) {  // 无限循环
        char ch = *p++;
        switch (ch) {
            case '\"': // 遇到包裹字符串末尾的 "
                len = c->top - head; // 此时可以计算字符串长度
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);  // 求出这个字符串
                c->json = p;
                return LEPT_PARSE_OK;
            case '\0':  // 遇到空字符，说明字符串为空或缺少 "
                c->top = head;
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            case '\\':// 遇到转义字符
                 switch (*p++) {//读取\后的第一个字符，压入对应的转义字符
                    case '\"': PUTC(c, '\"'); break;//"
                    case '\\': PUTC(c, '\\'); break;//'\'
                    case '/':  PUTC(c, '/' ); break;//'/'
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    default://除了以上的，其他的字符加在反斜线后面都是非法的
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            default:
                if((unsigned char)ch<0x20){
                    c->top = head;
                    return LEPT_PARSE_INVALID_STRING_CHAR;
                }
                PUTC(c, ch);  // 压入当前字符到缓冲区
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':
            return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':
            return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':
            return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:
            return lept_parse_number(c, v);
        case '"':
            return lept_parse_string(c, v);
        case '\0':
            return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);  // 确保所有数据都被弹出
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;  // lept_free(v) 之后，会把它的类型变成 null。这个设计能避免重复释放
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));  // 必须是bool值
    return v->type == LEPT_TRUE;                                           // 因为LEPT_TRUE和LEPT_FALSE是枚举值，其值不为01，所以要通过条件表达式转一下
}

void lept_set_boolean(lept_value* v, int b) {
    // assert(v != NULL);
    lept_free(v);
    // printf("=> v->type: %d,b= %d\n", v->type, b);
    switch (b) {
        case 0: v->type = LEPT_TRUE;
        case 1: v->type = LEPT_FALSE;
    }
    // v->type = b ? LEPT_TRUE : LEPT_FALSE;
    // printf("<= v->type: %d,b= %d\n", v->type,b);
    
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->type = LEPT_NUMBER;
    v->u.n = n;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

// 创建字符串
void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    // 分配长度为len+1的内存单元来存放字符串
    v->u.s.s = (char*)malloc(len + 1);  // malloc(len + 1) 中的 1 是因为结尾空字符
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';  // 末尾添加空字符
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

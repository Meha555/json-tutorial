#include "leptjson.h"
#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL, strtod() */
#include <math.h>
#include <errno.h>

/*
重构合并 lept_parse_null()、lept_parse_false()、lept_parse_true() 为 lept_parse_literal()。
加入 维基百科双精度浮点数 的一些边界值至单元测试，如 min subnormal positive double、max double 等。
去掉 test_parse_invalid_value() 和 test_parse_root_not_singular() 中的 #if 0 ... #endif，执行测试，证实测试失败。按 JSON number 的语法在 lept_parse_number() 校验，不符合标准的情况返回 LEPT_PARSE_INVALID_VALUE 错误码。
去掉 test_parse_number_too_big 中的 #if 0 ... #endif，执行测试，证实测试失败。仔细阅读 strtod()，看看怎样从返回值得知数值是否过大，以返回 LEPT_PARSE_NUMBER_TOO_BIG 错误码。（提示：这里需要 #include 额外两个标准库头文件。）
*/

// 作用同pl0的getch，但还包含对读入的字符是否是期待的字符的断言
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char* json;  // 通过移动char字符串中的指针p来实现逐个字符的读入
} lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char* p = c->json;
    // 跳过空格
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

// 合并后的文法paser（null|true|false）
static int lept_parse_literal(lept_context* c, lept_value* v,const char* literal, lept_type type) {
    size_t i;// unsigned long
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1] != '\0';i++){
        if(c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

#if 0
static int lept_parse_true(lept_context* c, lept_value* v) {
    EXPECT(c, 't');
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context* c, lept_value* v) {
    EXPECT(c, 'f');
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}
#endif

static int lept_parse_number(lept_context* c, lept_value* v) {
    /* \TODO validate number */
    const char* p = c->json;
    /*数字文法
     * number = [ "-" ] int [ frac ] [ exp ]
     * int = "0" / digit1-9 *digit
     * frac = "." 1*digit
     * exp = ("e" / "E") ["-" / "+"] 1*digit
     */
    /*负数*/
    if(p[0]=='-') p++;//跳过 -号
    /*整数*/
    if(*p=='0') p++;//跳过单独 0
    else{//跳过数字
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;//非法情况：是字母或者0或者empty
        for (p++; ISDIGIT(*p);p++);
    }
    /*小数*/
    if(*p=='.'){
        p++;//跳过小数点
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;//非法情况：是字母或者0或者empty
        for (p++; ISDIGIT(*p);p++);
    }
    /*指数*/
    if(*p=='e'||*p=='E'){
        p++;//跳过e或E
        if(*p=='-'||*p=='+') p++;//跳过可选的正负号
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE; // 非法情况：是字母或者empty
        for (p++; ISDIGIT(*p);p++);
    }
    
    errno = 0;
    v->n = strtod(c->json, NULL);
    if (errno = ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't': return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f': return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n': return lept_parse_literal(c, v, "null", LEPT_NULL);
        default: return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

// 仅当 type == LEPT_NUMBER 时，n 才表示 JSON 数字的数值
double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}

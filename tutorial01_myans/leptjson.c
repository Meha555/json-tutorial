#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

//作用同pl0的getch
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

/* 本节目的：
 * JSON-text = ws value ws
 * ws = *(%x20 / %x09 / %x0A / %x0D)
 * value = null / false / true 
 * null  = "null"
 * false = "false"
 * true  = "true" 
 */

typedef struct {
    const char* json;//通过移动char字符串中的指针p来实现逐个字符的读入
}lept_context;

//由于 lept_parse_whitespace() 是不会出现错误的，返回类型为 void。其它的解析函数会返回错误码，传递至顶层

//ws的parser
static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;//来一个工作指针p，省的一直写c->json累
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;//去掉已分析过的部分，更新json字符串
}

//null的parser
static int lept_parse_null(lept_context* c, lept_value* v) {
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    //能到这里说明确实是null
    c->json += 3;// ull
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;//正确码
}

//true的parser
static int lept_parse_true(lept_context* c,lept_value* v) {
    EXPECT(c,'t');//p已经++了
    if(c->json[0]!='r'||c->json[1]!='u'||c->json[2]!='e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;// rue
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

//false的parser
static int lept_parse_false(lept_context* c,lept_value* v) {
    EXPECT(c,'f');//p已经++了
    if(c->json[0]!='a'||c->json[1]!='l'||c->json[2]!='s'||c->json[3]!='e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;// alse
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

//对value的内容的parser
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_null(c, v);
        case 't':  return lept_parse_true(c, v);
        case 'f':  return lept_parse_false(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return LEPT_PARSE_INVALID_VALUE;
    }
}

//按照文法展开识别
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);//识别掉第一个ws
    int ret;
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0')
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

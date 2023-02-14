#ifndef LEPTJSON_H__
#define LEPTJSON_H__
// JSON 中有 6 种数据类型，如果把 true 和 false 当作两个类型就是 7 种
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

// JSON 树的每个结点的类型
typedef struct {
    double n; //number类型的数值
    lept_type type;
}lept_value;

// 错误码
enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,
    LEPT_PARSE_INVALID_VALUE,
    LEPT_PARSE_ROOT_NOT_SINGULAR,
    LEPT_PARSE_NUMBER_TOO_BIG
};

// 递归下降的入口
// 解析给定的JSON
int lept_parse(lept_value* v, const char* json);

lept_type lept_get_type(const lept_value* v);

double lept_get_number(const lept_value* v);

#endif /* LEPTJSON_H__ */

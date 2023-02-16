# 从零开始的 JSON 库教程-学习笔记

> [json-tutorial: 从零开始的 JSON 库教程 (github.com)](https://github.com/Meha555/json-tutorial)
>
> [从零开始的 JSON 库教程 - 知乎 (zhihu.com)](https://www.zhihu.com/column/json-tutorial)

本项目使用CMake，需要添加编译参数 -std=c99，否则单行注释无法使用

## 第一节

### 头文件与 API 设计

```c
#ifndef LEPTJSON_H__
#define LEPTJSON_H__

/* ... */

#endif /* LEPTJSON_H__ */
```

宏的名字必须是唯一的，通常习惯以 _*H__* 作为后缀。由于 leptjson 只有一个头文件，可以简单命名为 LEPTJSON_H__。如果项目有多个文件或目录结构，可以用 `项目名称_目录_文件名称_H__` 这种命名方式。

因为 C 语言没有 C++ 的命名空间（namespace）功能，一般会使用项目的简写作为标识符的前缀。通常枚举值用全大写（如 LEPT_NULL），而类型及函数则用小写（如 lept_type）。

为了减少解析函数之间传递多个参数，我们把这些数据都放进一个 lept_context 结构体：

```c
typedef struct {
    const char* json;
}lept_context;
```

### 单元测试

本项目采用测试驱动开发（TDD，Test-Driven Development），即先写单元测试再写功能代码。

以下以第一节的第一个单元测试举例演示，可以先将其他单元测试先注释掉

```c
// tutorial01/test.c
#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
```

单元测试使用宏编写，现时只提供了一个 EXPECT_EQ_INT(expect, actual) 的宏，每次使用这个宏时，如果 expect != actual（预期值不等于实际值），便会输出错误信息。 

```c
// tutorial01/leptjson.h
// 解析JSON API
int lept_parse(lept_value* v, const char* json);

lept_type lept_get_type(const lept_value* v);
```

```c
// 对null的单元测试
static void test_parse_null() {
    lept_value v;
    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "null"));//1
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));//2
}
```

运行结果是：

```c
/Users/miloyip/github/json-tutorial/tutorial01/test.c:27: expect: 0 actual: 1
1/2 (50.00%) passed
```

解析：第一个测试传入的json是"null"，符合对null的单元测试标准，所以通过；第二个测试传入的参数没有把 v.type 改成 LEPT_NULL（因为lest_parse_false()内部操作的是指针），造成失败

### 宏的编写技巧

在C/C++中，很多底层以及和编译相关的代码都使用宏而不用函数或内联函数来写，原因如下：

- 使用宏比调用函数更加高效，因为宏是在预编译时就进行替换；而函数需要在内存中分配空间，取地址然后跳转，并占用调用堆栈
- 测试框架使用了 `__LINE__` 这个编译器提供的宏，代表编译时该行的行号。如果用函数或内联函数，每次的行号便都会相同
- 内联函数是 C99 的新增功能，不指定编译参数无法使用

反斜线代表该行未结束，会串接下一行。而**如果宏里有多过一个语句（statement），就需要用 `do { /**...**/ } while(0)` 包裹成单个语句**，否则会有如下的问题：

```c
#define M() a(); b() //M()宏包含2条语句
if (cond)
    M();
else
    c();

/* 预处理后 */

if (cond)
    a(); b(); //这两条语句会破坏if-else的对应
else /* <- else 缺乏对应 if */
    c();
```

只用 {} 也不行：

```c
#define M() { a(); b(); }

/* 预处理后 */

if (cond)
    { a(); b(); }; /* M();的最后的分号代表 if 语句结束 */
else               /* else 缺乏对应 if */
    c();
```

用 do while 就行了：

```c
#define M() do { a(); b(); } while(0)

/* 预处理后 */

if (cond)
    do { a(); b(); } while(0);
else
    c();
```

> 主要是使用者加在宏后面的分号 `;` 可能产生意想不到的后果，但是在正常代码中使用宏时还是建议加分号，形成统一

### JSON解析器

由于JSON语法很简单，只需要检测下一个读入的字符即可判断是什么单词，因此无需专门的词法分析（tokenize），可直接进行语法分析。

- n ➔ null
- t ➔ true
- f ➔ false
- " ➔ string
- 0-9/- ➔ number
- [ ➔ array
- { ➔ object

然后依据JSON语法的ENBF范式，得到递归下降的语法分析器（paser）

```
JSON-text = ws value ws
ws = *(%x20 / %x09 / %x0A / %x0D)
value = null / false / true 
null  = "null"
false = "false"
true  = "true"
```

代码中很多地方用到了 `static` 关键字修饰函数，该关键字的意思是指，该函数只作用于编译单元中，那么没有被调用时，编译器是能发现的。

如下不调用 `test_parse_false()` ，则编译器给出警告（当然不影响成功编译）：

![在这里插入图片描述](https://img-blog.csdnimg.cn/74c96eb1c2cb4a31a9e0997032d147dc.png)

如果去掉 `static` 修饰，则不会给出警告

代码中使用一个指针 `p` 来表示当前的解析字符位置。这样做有两个好处，一是代码更简单，二是在某些编译器下性能更好（因为不能确定 `lept_context` 对象 `c` 会否被改变，从而每次更改 `c->json` 都要做一次间接访问）。如果校验成功，才把 `p` 赋值至 `c->json`。

## 第二节

### 用宏减少重复代码

根据DRY（don't repeat yourself）原则，对代码进行重构。

单元测试中很多重复的语句，这里可以重构，封装为宏：

```c
//重构前
static void test_parse_expect_value() {
    lept_value v;

    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_EXPECT_VALUE, lept_parse(&v, ""));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));

    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_EXPECT_VALUE, lept_parse(&v, " "));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
}

static void test_parse_invalid_value() {
    lept_value v;
    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_INVALID_VALUE, lept_parse(&v, "nul"));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));

    v.type = LEPT_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_INVALID_VALUE, lept_parse(&v, "?"));
    EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));
}

// 重构后
#define TEST_ERROR(error, json)\
    do {\
        lept_value v;\
        v.type = LEPT_FALSE;\
        EXPECT_EQ_INT(error, lept_parse(&v, json));\
        EXPECT_EQ_INT(LEPT_NULL, lept_get_type(&v));\
    } while(0)

static void test_parse_expect_value() {
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value() {
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "?");
}
```

### 断言的使用

代码中很多地方使用了断言 `assert` ，作用是保证执行过程中逻辑的正确性。通过断言对可能出现问题的地方加以判断，如果出现不符合预期的情况则断言为假，`abort` 掉；否则断言通过，继续执行。

比如要保证API操作的对象正确，可以通过断言在操作对象前保证对象的正确性。

### C中的嵌套注释

在C中要注释一部分代码，首先想到多行注释 `/* ... */` 。但是多行注释不支持嵌套，所以技巧是使用  `#if 0 ... #endif` 实现。

C 的注释不支持嵌套（nested），而 `#if ... #endif` 是支持嵌套的。代码中已有注释时，用 `#if 0 ... #endif` 去禁用代码是一个常用技巧，而且可以把 `0` 改为 `1` 去恢复。

### 跨平台类型名

在 C 语言中，数组长度、索引值最好使用 `size_t` 类型，而不是 `int` 或 `unsigned`。

### 字符串处理

C语言中的字符串处理，在不知道字符串长度时，善于利用终结符 `'\0'`

```c
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i; // unsigned long
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++) // literal[i + 1] != '\0'
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        /* ... */
    }
}
```

> [errno.h 详解_ultraji的博客-CSDN博客](https://blog.csdn.net/qq_23827747/article/details/79753552)
>
> [strtof, strtod, strtold - cppreference.com](https://zh.cppreference.com/w/c/string/byte/strtof)

## 第三节

### 使用union来实现变体，节约内存

如果不使用`union`，则同时可用的成员较多，但是一个值不可能同时为数字和字符串，在语义上不太符合，也不节约内存

```c
typedef struct {
    char* s;
    size_t len;
    double n;
    lept_type type;
}lept_value;
```

使用`union`：

```c
typedef struct {
	union {
        double n; // 数字
        struct {
            char* s;
            size_t len;
        }s; // 字符串
    }u; // 同时用一个成员u表示
    lept_type type;
}lept_value;
```

在 32 位平台时的内存布局

[![union_layout](https://github.com/Meha555/json-tutorial/raw/master/tutorial03/images/union_layout.png)](https://github.com/Meha555/json-tutorial/blob/master/tutorial03/images/union_layout.png)

### 宏定义设置参数

把初始大小以宏 `LEPT_PARSE_STACK_INIT_SIZE` 的形式定义，使用 `#ifndef X #define X ... #endif` 方式的好处是，使用者可在编译选项中自行设置宏，没设置的话就用缺省值。

### 可变数据类型

之前的单元都是固定长度的数据类型（fixed length data type），而字符串类型是**可变长度的数据类型（variable length data type），因此涉及到内存管理和数据结构的设计和实现**。

因为在解析 JSON 字符串时，在开始时不能知道字符串的长度，而又需要进行转义，所以需要一个临时缓冲区去存储解析后的结果。我们为此实现了一个动态增长的堆栈，可以不断压入字符，最后一次性把整个字符串弹出，复制至新分配的内存之中。

如果我们知道了字符串的长度，那么这个缓冲区就不必要了，我们直接开辟所需要长度的内存空间来存储这个字符串即可。

### 自动测试工具

#### Windows 下的内存泄漏检测方法

在 Windows 下，可使用 Visual C++ 的 [C Runtime Library（CRT） 检测内存泄漏](https://msdn.microsoft.com/zh-cn/library/x98tx3cf.aspx)。

首先，我们在两个 .c 文件首行插入这一段代码：

```c
#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
```

并在 `main()` 开始位置插入：

```c
int main() {
#ifdef _WINDOWS
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
```

在 Debug 配置下按 F5 生成、开始调试程序，没有任何异样。

然后，我们删去 `lept_set_boolean()` 中的 `lept_free(v)`：

```c
void lept_set_boolean(lept_value* v, int b) {
    /* lept_free(v); */
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}
```

再次按 F5 生成、开始调试程序，在输出会看到内存泄漏信息：

```shell
Detected memory leaks!
Dumping objects ->
C:\GitHub\json-tutorial\tutorial03_answer\leptjson.c(212) : {79} normal block at 0x013D9868, 2 bytes long.
 Data: <a > 61 00 
Object dump complete.
```

这正是我们在单元测试中，先设置字符串，然后设布尔值时没释放字符串所分配的内存。比较麻烦的是，它没有显示调用堆栈。从输出信息中 `... {79} ...` 我们知道是第 79 次分配的内存做成问题，我们可以加上 `_CrtSetBreakAlloc(79);` 来调试，那么它便会在第 79 次时中断于分配调用的位置，那时候就能从调用堆栈去找出来龙去脉。

#### Linux/OSX 下的内存泄漏检测方法

在 Linux、OS X 下，我们可以使用 [valgrind](https://valgrind.org/) 工具（用 `apt-get install valgrind`、 `brew install valgrind`）。我们完全不用修改代码，只要在命令行执行：

```shell
$ valgrind --leak-check=full  ./leptjson_test
==22078== Memcheck, a memory error detector
==22078== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
==22078== Using Valgrind-3.11.0 and LibVEX; rerun with -h for copyright info
==22078== Command: ./leptjson_test
==22078== 
--22078-- run: /usr/bin/dsymutil "./leptjson_test"
160/160 (100.00%) passed
==22078== 
==22078== HEAP SUMMARY:
==22078==     in use at exit: 27,728 bytes in 209 blocks
==22078==   total heap usage: 301 allocs, 92 frees, 34,966 bytes allocated
==22078== 
==22078== 2 bytes in 1 blocks are definitely lost in loss record 1 of 79
==22078==    at 0x100012EBB: malloc (in /usr/local/Cellar/valgrind/3.11.0/lib/valgrind/vgpreload_memcheck-amd64-darwin.so)
==22078==    by 0x100008F36: lept_set_string (leptjson.c:208)
==22078==    by 0x100008415: test_access_boolean (test.c:187)
==22078==    by 0x100001849: test_parse (test.c:229)
==22078==    by 0x1000017A3: main (test.c:235)
==22078== 
...
```

它发现了在 `test_access_boolean()` 中，由 `lept_set_string()` 分配的 2 个字节（`"a"`）泄漏了。

> ```shell
> ==22078==    at 0x100012EBB: malloc (in /usr/local/Cellar/valgrind/3.11.0/lib/valgrind/vgpreload_memcheck-amd64-darwin.so)
> ==22078==    by 0x100008F36: lept_set_string (leptjson.c:208)
> ==22078==    by 0x100008415: test_access_boolean (test.c:187)
> ==22078==    by 0x100001849: test_parse (test.c:229)
> ==22078==    by 0x1000017A3: main (test.c:235)
> ```
>
> 这是函数调用栈，可以看出调用顺序： `main -> test_parse -> test_access_boolean -> lept_set_string -> malloc` ，因此我们在 `lept_set_string` 中的 `malloc` 中查找错误，是由这个 `malloc` 所分配的内存在某处没有释放，导致内存泄漏

Valgrind 还有很多功能，例如可以发现未初始化变量。我们若在应用程序或测试程序中，忘了调用 `lept_init(&v)`，那么 `v.type` 的值没被初始化，其值是不确定的（indeterministic），一些函数如果读取那个值就会出现问题：

```c
static void test_access_boolean() {
    lept_value v;
    /* lept_init(&v); */
    lept_set_string(&v, "a", 1);
    ...
}
```

这种错误有时候测试时能正确运行（刚好 `v.type` 被设为 `0`），使我们误以为程序正确，而在发布后一些机器上却可能崩溃。这种误以为正确的假像是很危险的，我们可利用 valgrind 能自动测出来：

```shell
$ valgrind --leak-check=full  ./leptjson_test
...
==22174== Conditional jump or move depends on uninitialised value(s)
==22174==    at 0x100008B5D: lept_free (leptjson.c:164)
==22174==    by 0x100008F26: lept_set_string (leptjson.c:207)
==22174==    by 0x1000083FE: test_access_boolean (test.c:187)
==22174==    by 0x100001839: test_parse (test.c:229)
==22174==    by 0x100001793: main (test.c:235)
==22174== 
```

它发现 `lept_free()` 中依靠了一个未初始化的值来跳转，就是 `v.type`，而错误是沿自 `test_access_boolean()`。

编写单元测试时，应考虑哪些执行次序会有机会出错，例如内存相关的错误。然后我们可以利用 TDD 的步骤，先令测试失败（以内存工具检测），修正代码，再确认测试是否成功。

## 第四节

根据 UTF-8 编码表实现的Unicode转UTF-8存储：

```c
static void lept_encode_utf8(lept_context* c, unsigned u) {
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF); // 用断言检测
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}
```

最终也是写进一个 `char`，为什么要做 `x & 0xFF` 这种操作呢？

这是因为 `u` 是 `unsigned` 类型，一些编译器可能会警告这个转型可能会截断数据。但实际上，配合了范围的检测然后右移之后，可以保证写入的是 0~255 内的值。为了避免一些编译器的警告误判，我们加上 `x & 0xFF`。一般来说，编译器在优化之后，这与操作是会被消去的，不会影响性能。

其实超过 1 个字符输出时，可以只调用 1 次 `lept_context_push()`。这里全用 `PUTC()` 只是为了代码看上去简单一点，我是这样写的：

```c
#define PUTC_S(c, ch, size)                                         \
    do {                                                            \
        int i = 0;                                                  \
        while (i < size) {                                            \
            *(char*)lept_context_push(c, sizeof(char)) = (ch[i++]); \
        }                                                           \
    } while (0)

static void lept_encode_utf8(lept_context* c, unsigned u) {
    if (u <= 0x7F) {
        PUTC(c, u & 0xFF);
    } else if (u <= 0x07FF) {
        char* ch = (char*)malloc(2 * sizeof(char));
        ch[0] = 0xC0 | ((u >> 6) & 0xFF);
        ch[1] = 0x80 | (u & 0x3F);
        PUTC_S(c, ch, 2);
        free(ch);
        // PUTC(c, 0xC0 | ((u >> 6) & 0xFF));  // 110xxxxx
        // PUTC(c, 0x80 | (u & 0x3F));         // 10xxxxxx
    } else if (u <= 0xFFFF) {
        char* ch = (char*)malloc(3 * sizeof(char));
        ch[0] = 0xE0 | ((u >> 12) & 0xFF);
        ch[1] = 0x80 | ((u >> 6) & 0x3F);
        ch[2] = 0x80 | (u & 0x3F);
        PUTC_S(c, ch, 3);
        free(ch);
        // PUTC(c, 0xE0 | ((u >> 12) & 0xFF));  // 1110xxxx
        // PUTC(c, 0x80 | ((u >> 6) & 0x3F));   // 10xxxxxx
        // PUTC(c, 0x80 | (u & 0x3F));          // 10xxxxxx
    } else {
        assert(u <= 0x10FFFF);
        char* ch = (char*)malloc(4 * sizeof(char));
        ch[0] = 0xF0 | ((u >> 18) & 0xFF);
        ch[1] = 0x80 | ((u >> 12) & 0x3F);
        ch[2] = 0x80 | ((u >> 6) & 0x3F);
        ch[3] = 0x80 | (u & 0x3F);
        PUTC_S(c, ch, 4);
        free(ch);
        // PUTC(c, 0xF0 | ((u >> 18) & 0xFF));  // 11110xxx
        // PUTC(c, 0x80 | ((u >> 12) & 0x3F));  // 10xxxxxx
        // PUTC(c, 0x80 | ((u >> 6) & 0x3F));   // 10xxxxxx
        // PUTC(c, 0x80 | (u & 0x3F));          // 10xxxxxx
    }
}
```

> 代理对没看懂

## 第五节

### 相互引用与前向声明

#### 类型的前向声明

在C/C++中出现不完全类，只能使用该不完全类的指针，且必须对其进行前向声明，而不能像Java哪样直接使用其对象。

```c
typedef struct lept_value lept_value;  // 由于 lept_value 内使用了自身类型的指针，我们必须前向声明（forward declare）此类型

struct lept_value { /* value */
    union {
        struct { lept_value* e; size_t size; }a;    /* array:  elements, element count */
        struct { char* s; size_t len; }s;           /* string: null-terminated string, string length */
        double n;                                   /* number */
    }u;
    lept_type type;
};
```

#### 函数的前向声明

互相引用的函数，必须要在2者之前加入函数的前向声明

```c
static int lept_parse_value(lept_context* c, lept_value* v);  // lept_parse_value() 会调用 lept_parse_array()，而 lept_parse_array() 又会调用 lept_parse_value()，这是互相引用，必须要加入函数前向声明
```

### 迭代器失效

```c
    for (;;) {
        /* bug! */
        lept_value* e = lept_context_push(c, sizeof(lept_value));
        lept_init(e);
        size++;
        if ((ret = lept_parse_value(c, e)) != LEPT_PARSE_OK)
            return ret;
        /* ... */
    }
```

我们把这个指针调用 `lept_parse_value(c, e)`，这里会出现问题，因为 `lept_parse_value()` 及之下的函数都需要调用 `lept_context_push()`，而 `lept_context_push()` 在发现栈满了的时候会用 `realloc()` 扩容。这时候，我们上层的 `e` 就会失效，变成一个悬挂指针（dangling pointer），而且 `lept_parse_value(c, e)` 会通过这个指针写入解析结果，造成非法访问。

在使用 C++ 容器时，也会遇到类似的问题。从容器中取得的迭代器（iterator）后，如果改动容器内容，之前的迭代器会失效。这里的悬挂指针问题也是相同的。

## 第六节

### 选择合适的数据结构

在软件开发过程中，许多时候，选择合适的数据结构后已等于完成一半工作。没有完美的数据结构，所以最好考虑多一些应用的场合，看看时间／空间复杂度以至相关系数是否合适。

要表示键值对的集合，有很多数据结构可供选择，例如：

- 动态数组（dynamic array）：可扩展容量的数组，如 C++ 的 [`std::vector`](https://en.cppreference.com/w/cpp/container/vector)。
- 有序动态数组（sorted dynamic array）：和动态数组相同，但保证元素已排序，可用二分搜寻查询成员。
- 平衡树（balanced tree）：平衡二叉树可有序地遍历成员，如红黑树和 C++ 的 [`std::map`](https://en.cppreference.com/w/cpp/container/map)（[`std::multi_map`](https://en.cppreference.com/w/cpp/container/multimap) 支持重复键）。
- 哈希表（hash table）：通过哈希函数能实现平均 O(1) 查询，如 C++11 的 [`std::unordered_map`](https://en.cppreference.com/w/cpp/container/unordered_map)（[`unordered_multimap`](https://en.cppreference.com/w/cpp/container/unordered_multimap) 支持重复键）。

设一个对象有 n 个成员，数据结构的容量是 m，n ⩽ m，那么一些常用操作的时间／空间复杂度如下：

|                 | 动态数组  | 有序动态数组 | 平衡树     | 哈希表                 |
| --------------- | --------- | ------------ | ---------- | ---------------------- |
| 有序            | 否        | 是           | 是         | 否                     |
| 自定成员次序    | 可        | 否           | 否         | 否                     |
| 初始化 n 个成员 | O(n)      | O(n log n)   | O(n log n) | 平均 O(n)、最坏 O(n^2) |
| 加入成员        | 分摊 O(1) | O(n)         | O(log n)   | 平均 O(1)、最坏 O(n)   |
| 移除成员        | O(n)      | O(n)         | O(log n)   | 平均 O(1)、最坏 O(n)   |
| 查询成员        | O(n)      | O(log n)     | O(log n)   | 平均 O(1)、最坏 O(n)   |
| 遍历成员        | O(n)      | O(n)         | O(n)       | O(m)                   |
| 检测对象相等    | O(n^2)    | O(n)         | O(n)       | 平均 O(n)、最坏 O(n^2) |
| 空间            | O(m)      | O(m)         | O(n)       | O(m)                   |

### 重构——提取方法[^1]

将完成一个模块的内部的功能拆分成单独的函数，这样能获得更大的灵活性。【效果类似于C++Primer中提到的 `new/delete` 运算符是将内存的分配/释放和对象的生成/销毁这两个操作绑定在一起，而 `allocator` 类则是将 `new/delete` 的每一个原子操作拆分开】

```c
// 解析JSON字符串，并将识别出的字符串写入str，其长度写入len
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len)；

// 将解析出的字符串存入所给的JSON结点(分配内存)
void lept_set_string(lept_value* v, const char* s, size_t len)；

// 识别JSON字符串的对外API
static int lept_parse_string(lept_context* c, lept_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK) // 使用API
        lept_set_string(v, s, len); // 使用API
    return ret;
}
```

[^1]: Fowler, Martin. Refactoring: improving the design of existing code. Pearson Education India, 2009. 中译本：熊节译，《重构——改善既有代码的设计》，人民邮电出版社，2010年。

## 第七节

### 往返（roundtrip）测试

最简单的测试方式，把一个 JSON 解析，然后再生成另一 JSON，逐字符比较两个 JSON 是否一模一样。

## 第八节

### 解决资源所有权的问题

涉及指针的赋值，就可能引起资源拥有权（resource ownership）的问题。当指向对象 `o` 的指针 `p` 被写入另一处时，程序中便有2处指向同一对象 `o` 的指针，释放这两个指针所指向的内存单元时将会导致后被释放的指针重复释放那块内存单元。

#### 深拷贝

直接将指针 `p` 所指向的内存单元的内容完全拷贝一份，放在新开辟的一块内存中，然后把这块新内存的指针写入目的位置。

为了减少深度复制的成本，有些程序库或运行时还会采用[写入时复制](https://zh.wikipedia.org/zh-cn/寫入時複製)（copy-on-write, COW）。而浅复制（shallow copy）则需要 [引用计数](https://zh.wikipedia.org/wiki/引用计数)（reference count）或 [垃圾回收](https://zh.wikipedia.org/zh-cn/垃圾回收_(計算機科學))（garbage collection, GC）等技术。

#### 移动语义

将指针 `p` 的所有权转移至目的位置后，将原来的指针 `p` 置空。


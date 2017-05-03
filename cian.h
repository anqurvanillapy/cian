#ifndef __CIAN_CIAN_H
#define __CIAN_CIAN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

/// Tokens and classes.
enum {
    /* Basic classes. */
    Num = 128,      // number
    Fun,            // function
    Sys,            // system call
    Glo,            // global
    Loc,            // local
    Id,             // symbol
    /* Reserved words. */
    Char, Int,
    If, Else, While,
    Return,
    Sizeof,
    Enum,
    /* Operators. */
    Assign,         // assignment
    Cond,           // condition
    Lor, Lan,       // logical or/and
    Or, Xor, And,
    Eq, Ne, Lt, Gt, Le, Ge,
    Shl, Shr,       // shift left/right
    Add, Sub, Mul, Div, Mod, Inc, Dec,
    Brak            // open/closed bracket
};

/// Opcodes.
enum {
    /* Instructions. */
    LEA,            // load effective address
    IMM,            // immediate value or global address
    JMP,            // jump
    JSR,            // jump to subroutine
    BZ,             // branch if zero
    BNZ,            // branch if not zero
    ENT,            // enter subroutine
    ADJ,            // stack adjust
    LEV,            // leave subroutine
    LI, LC,         // load int/char
    SI, SC,         // store int/char
    PSH,            // push
    /* Operations. */
    OR, XOR, AND,
    EQ, NE, LT, GT, LE, GE,
    SHL, SHR,
    ADD, SUB, MUL, DIV, MOD,
    /* System calls. */
    OPEN, READ, CLOS,
    PRTF,           // printf
    MALC, FREE,     // malloc/free
    MSET, MCMP,     // memset/memcmp
    EXIT
};

/// Types: If ty is even, it is a `long' pointer, or it is a `char' pointer if
/// it is odd.  The quotient is the pointer level.
enum { INT, CHAR, PTR };

/// Identifier offsets.
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

/**
 *  Utilities.
 */

// Pop next program argument.
#define next_flag(a)    argc > 0 && **argv == '-' && (*argv)[1] == (a)

// `malloc' `sz'-sized segment.
#define malloc_seg(seg, type, sz, name) do { \
    if ( (seg = (type *)malloc(sz)) == NULL) { \
        printf("malloc error: " name " (size=%zd)\n", sz); \
        exit(-1); \
    } \
} while(0)

// Throw an error with line count and messages and exit.
#define throw_err(errmsg)   do { \
    printf("%d: " errmsg "\n", line); exit(-1); \
} while(0)

// `assert' with line count provided.
#define lassert(expr, errmsg)   do { \
    if (!(expr)) { \
        throw_err(errmsg); \
    } \
} while(0)

/**
 *  Interpretation.
 */

/// Tokenizer.
void next();
/// Parser: Expression.
void expr(int lev);
/// Parser: Statement.
void stmt();

#endif /* !__CIAN_CIAN_H */

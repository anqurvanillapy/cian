#include "cian.h"

/// States.
char *p, *lp,   // current position in source code
     *data;     // bss pointer (data segment)

long *e, *le,   // current position in emitted code
     tk,        // current token
     ival,      // current token value
     ty,        // current expression type
     loc,       // local variable offset
     *id,       // currently parsed identifier
     *sym;      // symbol table

int line;       // current line number

void
next()
{
    char *pp;   // previous position

    while ( (tk = *p)) {
        ++p;
        if (tk == '\n') {
            /* TODO */
        } else if ((tk >= 'a' && tk <= 'z')
                   || (tk >= 'A' && tk <= 'Z')
                   || tk == '_') {
            /* Identifier. */
            pp = p - 1;

            // Hash the symbol token.  BTW, there are 19 reserved words
            // (128 + 19 = 147).
            while ((*p >= 'a' && *p <= 'z')
                   || (*p >= 'A' && *p <= 'Z')
                   || (*p >= '0' && *p <= '9')
                   || *p == '_') tk = tk * 147 + *p++;

            tk = (tk << 6) + (p - pp);
            id = sym;   // get the current symbol

            while (id[Tk]) {
                // Checks if it exists.
                if (tk == id[Hash]
                    && memcmp((char *)id[Name], pp, p - pp) == 0) {
                    tk = id[Tk];
                    return;
                }
                id = id + Idsz;
            }

            id[Name] = (long)pp;
            id[Hash] = tk;
            tk = id[Tk] = Id;   // is an identifier
            return;
        }
    }
}

void
expr(int lev)   // lev: Level, precedence of an operator
{
    // Temps.
    long t, *d;

    lassert(tk, "Unexpected EOF in expression");

    switch (tk) {
        case Num:   // number
            *++e = IMM; *++e = ival; next(); ty = INT;
            break;
        case '"':   // string literal
            *++e = IMM; *++e = ival; next();
            while (tk == '"') next();   // literal concatenation
            data = (char *)((long)data + sizeof(long)); ty = PTR;
            break;
        case Sizeof:
            next();
            lassert(tk == '(', "Open paren expected in sizeof");
            next();

            ty = INT;
            if      (tk == Int) next();
            else if (tk == Char) { next(); ty = CHAR; }
            while (tk == Mul) { next(); ty = ty + PTR; }

            lassert(tk == ')', "Closed paren expected in sizeof");
            next();

            // PTR, INT: sizeof(long); CHAR: sizeof(char).
            *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(long);
            ty = INT;
            break;
        case Id:
            d = id; next();

            if (tk == '(') {    // function call
                next(); t = 0;

                while (tk != ')') {
                    expr(Assign);   // eval
                    *++e = PSH; ++t;
                    if (tk == ',') next();
                }   next(); // trailing next()

                if      (d[Class] == Sys) *++e = d[Val];
                else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; }
                else    { throw_err("Bad function call"); }

                // Adjust stack due to assignments.
                if (t) { *++e = ADJ; *++e = t; }

                // Type forwarding.
                ty = d[Type];
            } else if (d[Class] == Num) {   // return variable value
                *++e = IMM; *++e = d[Val]; ty = INT;
            } else {    // load global/local variable
                if      (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; }
                else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }
                else    { throw_err("Undefined variable"); }
                *++e = ( (ty = d[Type]) == CHAR) ? LC : LI;
            }

            break;
        case '(':
            next();

            if (tk == Int || tk == Char) {  // cast
                t = (tk == Int) ? INT : CHAR;
                next();
                while (tk == Mul) { next(); t = t + PTR; }
                lassert(tk == ')', "Bad cast");
                next();
                expr(Inc);  // might be an incremental prefix
                ty = t;
            } else {
                expr(Assign);   // eval
                lassert(tk == ')', "Closed paren expected");
                next();
            }

            break;
        case Mul:
            next(); expr(Inc);  // eval multiple dereferences
            lassert(ty > INT, "Bad dereference");
            ty = ty - PTR;
            *++e = (ty == CHAR) ? LC : LI;
            break;
        case And:
            next(); expr(Inc);  // eval multiple `address-of's
            lassert(*e == LC || *e == LI, "Bad address-of");
            ty = ty + PTR;
            break;
        case '!':
            // Unary `!': Return n == 0.
            next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ;
            ty = INT;
            break;
        case '~':
            // Unary `~': Return n ^ -1 (-1 got all 1s).
            next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR;
            ty = INT;
            break;
        case Add:
            next(); expr(Inc); ty = INT;
            break;
        case Sub:
            next(); *++e = IMM;
            if (tk == Num) { *++e = -ival; next(); }                // binary
            else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; }  // unary
            ty = INT;
            break;
        case Inc:
        case Dec:
            t = tk; next(); expr(Inc);
            if      (*e == LC) { *e = PSH; *++e = LC; }
            else if (*e == LI) { *e = PSH; *++e = LI; }
            else    { throw_err("Bad lvalue in pre-increment"); }

            *++e = PSH;
            // In-/decrement a pointer/variable.
            *++e = IMM; *++e = (ty > PTR) ? sizeof(long) : sizeof(char);
            *++e = (t == Inc) ? ADD : SUB;
            *++e = (ty == CHAR) ? SC : SI;

            break;
        default: throw_err("Bad expression");
    }

    // Precedence climbing (or TDOP).
    while (tk >= lev) {
        t = ty;

        switch (tk) {
            case Assign:
                next();
                lassert(*e == LC || *e == LI, "Bad lvalue in assignment");
                *e = PSH;
                expr(Assign);
                *++e = ( (ty = t) == CHAR) ? SC : SI;
                break;
            case Cond:
                next();
                *++e = BZ; d = ++e;
                expr(Assign);
                lassert(tk == ':', "Conditional missing colon");
                next();
                *d = (long)(e + 3); *++e = JMP; d = ++e;
                expr(Cond);
                *d = (long)(e + 1);
                break;
            case Lor:
                next();
                *++e = BNZ; d = ++e; expr(Lan); *d = (long)(e + 1);
                ty = INT; break;
            case Lan:
                next();
                *++e = BZ; d = ++e; expr(Or); *d = (long)(e + 1);
                ty = INT; break;
            case Or:
                next();
                *++e = PSH; expr(Xor); *++e = OR;
                ty = INT; break;
            case Xor:
                next();
                *++e = PSH; expr(And); *++e = XOR;
                ty = INT; break;
            case And:
                next();
                *++e = PSH; expr(Eq); *++e = AND;
                ty = INT; break;
            case Eq:
                next();
                *++e = PSH; expr(Lt); *++e = EQ;
                ty = INT; break;
            case Ne:
                next();
                *++e = PSH; expr(Lt); *++e = NE;
                ty = INT; break;
            case Lt:
                next();
                *++e = PSH; expr(Shl); *++e = LT;
                ty = INT; break;
            case Gt:
                next();
                *++e = PSH; expr(Shl); *++e = GT;
                ty = INT; break;
            // TODO
            default: throw_err("Token error: tk=%ld", tk);
        }
    }
}

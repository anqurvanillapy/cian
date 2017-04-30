#include "cian.h"

/// States.
char *p, *lp;   // current position in source code

long tk,        // current token
     ival,      // current token value
     loc,       // local variable offset
     *id,       // currently parsed identifier
     *sym;      // symbol table

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

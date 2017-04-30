#include "cian.h"

/// See interpret.c: States.
extern char *p, *lp;
extern long tk, ival, loc, *id, *sym;

int
main(int argc, const char *argv[])
{
    // Source file descriptor.
    int fd;

    // Flags: print source and assembly/executed instructions.
    bool src, debug;

    // Pre-allocate pools for different areas.
    size_t poolsz;

    // VM registers.
    long *pc,       // program counter
         *sp, *bp,  // stack/base pointer
         a,         // accumulator
         cycle;     // cycle count

    // States.
    char *data;     // data/bss area
    long *le, *e,   // current position in emitted code
         line,
         ty, bt;    // type and basetype

    // Main function in source file.
    long *idmain;

    // Temps.
    long i, *t;

    // Pop the argv for parsing flags.
    --argc; ++argv; // skip the $0 argument
    if (next_flag('S')) { src = true; --argc; ++argv; }
    if (next_flag('d')) { debug = true; --argc; ++argv; }
    // The following positional arguments are filenames.
    if (argc < 1) {
        printf("usage: cian [-S] [-d] file ...\n");
        return -1;
    }

    if ( (fd = open(*argv, 0)) < 0) {
        printf("open error: %s", *argv);
        return -1;
    }

    poolsz = 256 * 1024;    // 256 KiB

    malloc_seg(sym,    long,   poolsz, "symbol table");
    malloc_seg(le = e, long,   poolsz, "code segment");
    malloc_seg(data,   char,   poolsz, "data segment");
    malloc_seg(sp,     long,   poolsz, "stack segment");

    memset(sym,     0,  poolsz);
    memset(e,       0,  poolsz);
    memset(data,    0,  poolsz);

    // XXX: Presumably it is a `const' array of reserved words, thus
    // modifications would raise errors.
    p = (char *)"char int enum if else while return sizeof "
        "open read close printf malloc free memset memcmp exit void main";

    // Add keywords to symbol table.
    for (i = Char; i <= Sizeof; ++i) { next(); id[Tk] = i; }
    // Add system calls (library functions) to symbol table.
    for (i = OPEN; i <= EXIT; ++i) {
        next();
        id[Class] = Sys; id[Type] = INT; id[Val] = i;
    }

    next(); id[Tk] = Char;  // void type
    next(); idmain = id;    // keeps track of main

    // Load source file.
    malloc_seg(lp = p,  char,   poolsz, "source area");
    if ( (i = read(fd, p, poolsz - 1)) <= 0) {
        printf("read error or EOF: %ld returned\n", i);
        return -1;
    }

    // *p wasn't memset, so guarantee that the long string of the source code
    // can be terminated by a null character.
    p[i] = 0;
    close(fd);

    // Parse declarations (of type CHAR, INT, ENUM).
    // XXX: Declaration and initialization cannot be done simultaneously.
    line = 1;
    next();
    while (tk) {
        bt = INT;   // might be a global declaration

        if      (tk == Int) next();
        else if (tk == Char) { next(); bt = CHAR; }
        else if (tk == Enum) {
            next();
            if (tk != '{') next();  // enum name
            if (tk == '{') {
                next();

                // Enum is zero-based
                for (i = 0; tk != '}'; ++i) {
                    lassert(tk == Id, "Bad enum identifier");
                    next();

                    // Optional enum initializer.
                    if (tk == Assign) {
                        next();
                        lassert(tk == Num, "Bad enum initializer");
                        i = ival;   // enum initialized with current value
                        next();
                    }

                    // Store as identifiers.
                    id[Class] = Num; id[Type] = INT; id[Val] = i;
                    if (tk == ',') next();  // no commas would raise errors in
                                            // next loop
                }

                next();
            }
        }

        // Check if it is in global scope.
        while (tk != ';' && tk != '}') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }    // a pointer
            lassert(tk == Id, "Bad global declaration");
            lassert(!id[Class], "Duplicate global definition");
            next();
            id[Type] = ty;

            // Oops, a function.
            if (tk == '(') {
                id[Class] = Fun;
                id[Val] = (long)(e + 1);
                next();

                // Function parameters.
                i = 0;
                while (tk != ')') {
                    ty = INT;   // basetype in parameters list
                    if      (tk == Int) next();
                    else if (tk == Char) { next(); ty = CHAR; }
                    while (tk == Mul) { next(); ty = ty + PTR; }
                    lassert(tk == Id, "Bad parameter declaration");
                    lassert(id[Class] != Loc, "Duplicate param declaration");
                    // Local variable states with function states.
                    id[HClass]  = id[Class];    id[Class]   = Loc;
                    id[HType]   = id[Type];     id[Type]    = ty;
                    id[HVal]    = id[Val];      id[Val]     = i++;
                    next();
                    if (tk == ',') next();
                }

                next();
                lassert(tk == '{', "Bad function declaration");
                loc = ++i;
                next();

                // Function local variables.
                while (tk == Int || tk == Char) {
                    bt = (tk == Int) ? INT : CHAR;
                    next();

                    while (tk != ';') {
                        ty = bt;
                        while (tk == Mul) { next(); ty = ty + PTR; }
                        lassert(tk == Id, "Bad local declaration");
                        lassert(id[Class] != Loc, "Duplicate local definition");
                        id[HClass]  = id[Class];    id[Class]   = Loc;
                        id[HType]   = id[Type];     id[Type]    = ty;
                        id[HVal]    = id[Val];      id[Val]     = ++i;
                        next();
                        if (tk == ',') next();
                    }

                    next();
                }

                // ENT: Enter a function, and specify an offset to skip local
                // variable declarations.
                *++e = ENT; *++e = i - loc;
                // Potential assignments.
                while (tk != '}') stmt();
                // LEV: Leave the function.
                *++e = LEV;
                id = sym;   // unwind symbol table locals

                while (id[Tk]) {
                    if (id[Class] == Loc) {
                        id[Class] = id[HClass];
                        id[Type] = id[HType];
                        id[Val] = id[HVal];
                    }
                    id = id + Idsz;
                }
            } else {
                // Okay it's not a function, but a global declaration.
                id[Class] = Glo;
                id[Val] = (long)data;
                data += sizeof(long);
            }

            if (tk == ',') next();
        }

        next();
    }

    if ( !(pc = (long *)idmain[Val])) {
        printf("main function not defined\n");
        return -1;
    }

    // `src' flag: expr() will print source and opcodes.
    if (src) return 0;

    // Setup stack.
    sp = (long *)((long)sp + poolsz); // stack base at highest address
    *--sp = EXIT;   // calls exit() if main returns

    *--sp = PSH; t = sp;
    *--sp = argc;
    *--sp = (long)argv;
    *--sp = (long)t;

    // Run.
    cycle = 0;
    for (;;) {
        i = *pc++; ++cycle;

        if (debug) {
            /* TODO */
        }

        switch (i) {
            /* Basic instructions. */
            case ENT:   *--sp = (long)bp; bp = sp; sp -= *pc++;         break;
            case LEV:   sp = bp; bp = (long *)sp++; pc = (long *)*sp++; break;
            case LEA:   a = (long)(bp + *pc++);                         break;
            case IMM:   a = *pc++;                                      break;
            case JMP:   pc = (long *)*pc;                               break;
            case JSR:   *--sp = (long)(pc + 1); pc = (long *)*pc;       break;
            case BZ:    pc = a ? pc + 1 : (long *)pc;                   break;
            case BNZ:   pc = a ? (long *)pc : pc + 1;                   break;
            case ADJ:   sp += *pc++;                                    break;
            case LI:    a = *(long *)a;                                 break;
            case LC:    a = *(char *)a;                                 break;
            case SI:    *(long *)*sp++ = a;                             break;
            case SC:    a = *(char *)+sp++ = a;                         break;
            case PSH:   *--sp = a;                                      break;

            /* Operations. */
            case OR:    a = *sp++   |   a;  break;
            case XOR:   a = *sp++   ^   a;  break;
            case AND:   a = *sp++   &   a;  break;
            case EQ:    a = *sp++   ==  a;  break;
            case NE:    a = *sp++   !=  a;  break;
            case LT:    a = *sp++   <   a;  break;
            case GT:    a = *sp++   >   a;  break;
            case LE:    a = *sp++   <=  a;  break;
            case GE:    a = *sp++   >=  a;  break;
            case SHL:   a = *sp++   <<  a;  break;
            case SHR:   a = *sp++   >>  a;  break;
            case ADD:   a = *sp++   +   a;  break;
            case SUB:   a = *sp++   -   a;  break;
            case MUL:   a = *sp++   *   a;  break;
            case DIV:   a = *sp++   /   a;  break;
            case MOD:   a = *sp++   %   a;  break;

            /* System calls and library functions. */
            case OPEN:  a = open((char *)sp[1], *sp);                   break;
            case READ:  a = read(sp[2], (char *)sp[1], *sp);            break;
            case CLOS:  a = close(*sp);                                 break;
            case PRTF:  t = sp + pc[1];
                        a = printf((char *)t[-1],
                                   t[-2], t[-3], t[-4], t[-5], t[-6]);  break;
            case MALC:  a = (long)malloc(*sp);                          break;
            case FREE:  free((void *)*sp);  /* safe here */             break;
            case MSET:  a = (long)memset((char *)sp[2], sp[1], *sp);    break;
            case MCMP:  a = memcmp((char *)sp[2], (char *)sp[1], *sp);  break;
            case EXIT:  printf("exit(%ld) cycle=%ld\n", *sp, cycle); return *sp;
            default:
                printf("Bad instruction %ld! cycle=%ld\n", i, cycle); return -1;
        }
    }

    return 0;
}

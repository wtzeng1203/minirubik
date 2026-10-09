/*
 * gcc reference build of the CLI program in rubik_rv32i.s: the same test
 * table, parse, rank, IDA* over per-depth frames, solution rebuild,
 * cubie-level replay check and ecall output. perm_move, ori_move, perm_pdb
 * and ori_pdb are linked from the verify_gates --emit output, so both builds
 * read the same bytes. Built freestanding without libc or libgcc, so the code
 * never uses *, / or % on variables. main returns the number of failed tests.
 */
#include <stdint.h>

#ifndef INPUT
#define INPUT "21345671111111"
#endif
#ifndef COUNT
#define COUNT 3
#endif
#ifndef SEARCH_ATTR
#define SEARCH_ATTR
#endif

extern const uint16_t perm_move[3][5040];
extern const uint16_t ori_move[3][729];
extern const uint8_t perm_pdb[5040];
extern const uint8_t ori_pdb[729];

struct test {
    char state[15];
    uint8_t expected;
};

struct frame {
    uint16_t p, o, cp, co;
    uint8_t face, turn, last, pad;
};

static const struct test tests[3] = {
    {INPUT, 11},
    {"12345671111111", 0},
    {"27561342131312", 3},
};
static const char msg_colon[] = ": ";
static const char msg_pass[] = "  [PASS T5/T6: Solved state reached, moves == expected]\n";
static const char msg_fail[] = "  [FAIL verification!]\n";
static const char msg_nl[] = "\n";
static const char move_names[9][4] = {
    "R ", "R2 ", "R' ", "B ", "B2 ", "B' ", "D ", "D2 ", "D' ",
};
static const uint8_t source_tab[3][8] = {
    {1, 4, 2, 0, 3, 5, 6, 0},
    {0, 1, 2, 4, 5, 6, 3, 0},
    {0, 2, 5, 3, 1, 4, 6, 0},
};
static const uint8_t twist_tab[3][8] = {
    {1, 2, 0, 2, 1, 0, 0, 0},
    {0, 0, 0, 1, 2, 1, 2, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

static uint8_t cur_p[7], cur_o[7];
static uint8_t sol_moves[16];
static struct frame frames[12];

static void print(const char *s) {
    register const char *a0 __asm__("a0") = s;
    register uint32_t a7 __asm__("a7") = 4;
    __asm__ volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
}

static void rank_state(uint32_t *rp, uint32_t *ro) {
    uint32_t p = 0, o = 0;
    for (uint32_t i = 0; i < 7; ++i) {
        uint32_t smaller = 0, prod = 0;
        for (uint32_t j = i + 1; j < 7; ++j)
            if (cur_p[j] < cur_p[i])
                ++smaller;
        /* The empty asm hides prod from gcc's loop analysis, which would
         * otherwise fold the additions into p * (7 - i), a __mulsi3 call. */
        for (uint32_t k = 7 - i; k != 0; --k) {
            prod += p;
            __asm__("" : "+r"(prod));
        }
        p = prod + smaller;
    }
    for (uint32_t i = 0; i < 6; ++i)
        o = (o << 1) + o + cur_o[i];
    *rp = p;
    *ro = o;
}

static void apply_move(uint32_t m) {
    uint32_t face = 0;
    while (m >= 3) {
        m -= 3;
        ++face;
    }
    for (uint32_t t = m + 1; t != 0; --t) {
        uint8_t np[7], no[7];
        for (uint32_t i = 0; i < 7; ++i) {
            uint32_t from = source_tab[face][i];
            uint32_t x = cur_o[from] + twist_tab[face][i];
            if (x >= 3)
                x -= 3;
            np[i] = cur_p[from];
            no[i] = (uint8_t)x;
        }
        for (uint32_t i = 0; i < 7; ++i) {
            cur_p[i] = np[i];
            cur_o[i] = no[i];
        }
    }
}

static void rebuild(uint32_t n) {
    const struct frame *f = frames;
    for (uint32_t d = 0; d < n; ++d, ++f) {
        uint32_t face = f->face, turn = f->turn;
        if (turn == 0) {
            --face;
            if (face == f->last)
                --face;
            turn = 2;
        } else {
            --turn;
        }
        sol_moves[d] = (uint8_t)((face << 1) + face + turn);
    }
}

static SEARCH_ATTR uint32_t ida(uint32_t sp, uint32_t so) {
    uint32_t bound = perm_pdb[sp];
    if (ori_pdb[so] > bound)
        bound = ori_pdb[so];
    if (bound == 0)
        return 0;
    for (;; ++bound) {
        struct frame *f = frames;
        uint32_t slack = bound - 1;
        f->p = (uint16_t)sp;
        f->o = (uint16_t)so;
        f->face = 0;
        f->turn = 0;
        f->last = 3;
        for (;;) {
            uint32_t face = f->face;
            if (face >= 3) {
                if (++slack == bound)
                    break;
                --f;
                continue;
            }
            uint32_t turn = f->turn, p, o;
            if (turn == 0) {
                p = f->p;
                o = f->o;
            } else {
                p = f->cp;
                o = f->co;
            }
            p = perm_move[face][p];
            o = ori_move[face][o];
            if (turn == 2) {
                uint32_t next = face + 1;
                if (next == f->last)
                    ++next;
                f->face = (uint8_t)next;
                f->turn = 0;
            } else {
                f->cp = (uint16_t)p;
                f->co = (uint16_t)o;
                f->turn = (uint8_t)(turn + 1);
            }
            if (slack == 0) {
                if ((p | o) == 0) {
                    rebuild(bound);
                    return bound;
                }
                continue;
            }
            if (perm_pdb[p] > slack || ori_pdb[o] > slack)
                continue;
            --slack;
            ++f;
            f->p = (uint16_t)p;
            f->o = (uint16_t)o;
            f->face = (uint8_t)(face == 0);
            f->turn = 0;
            f->last = (uint8_t)face;
        }
    }
}

static uint32_t solve_and_verify(const char *s, uint32_t expected) {
    uint32_t p, o, n;
    for (uint32_t i = 0; i < 7; ++i) {
        cur_p[i] = (uint8_t)(s[i] - '1');
        cur_o[i] = (uint8_t)(s[i + 7] - '1');
    }
    rank_state(&p, &o);
    n = ida(p, o);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t m = sol_moves[i];
        print(move_names[m]);
        apply_move(m);
    }
    print(msg_nl);
    if (n == expected) {
        rank_state(&p, &o);
        if ((p | o) == 0) {
            print(msg_pass);
            return 0;
        }
    }
    print(msg_fail);
    return 1;
}

int main(void) {
    uint32_t failures = 0;
    for (uint32_t i = 0; i < COUNT; ++i) {
        print(tests[i].state);
        print(msg_colon);
        failures += solve_and_verify(tests[i].state, tests[i].expected);
    }
    return (int)failures;
}

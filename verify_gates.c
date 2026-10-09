#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS
};

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static uint16_t perm_move[3][PERMUTATIONS];
static uint16_t ori_move[3][ORIENTATIONS];
static uint8_t perm_pdb[PERMUTATIONS];
static uint8_t ori_pdb[ORIENTATIONS];
static uint8_t perm_pdb_packed[PERMUTATIONS / 2];
static uint8_t ori_pdb_packed[(ORIENTATIONS + 1) / 2];

static inline uint8_t get_perm_pdb_packed(uint16_t p) {
    return (uint8_t)((perm_pdb_packed[p >> 1] >> ((p & 1U) << 2)) & 0x0FU);
}

static inline uint8_t get_ori_pdb_packed(uint16_t o) {
    return (uint8_t)((ori_pdb_packed[o >> 1] >> ((o & 1U) << 2)) & 0x0FU);
}

static state_t quarter_turn(state_t state, uint8_t face) {
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t)((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static uint32_t rank_state(const state_t *state) {
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t)(i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state) {
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t)(p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t)(o % 3U);
        sum = (uint8_t)(sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t)((3U - sum % 3U) % 3U);
}

static void build_tables_and_pdb(void) {
    state_t state;
    for (uint16_t r = 0; r < PERMUTATIONS; ++r) {
        unrank_state((uint32_t)r * ORIENTATIONS, &state);
        for (uint8_t f = 0; f < 3; ++f) {
            state_t next = quarter_turn(state, f);
            perm_move[f][r] = (uint16_t)(rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t r = 0; r < ORIENTATIONS; ++r) {
        unrank_state(r, &state);
        for (uint8_t f = 0; f < 3; ++f) {
            state_t next = quarter_turn(state, f);
            ori_move[f][r] = (uint16_t)(rank_state(&next) % ORIENTATIONS);
        }
    }

    memset(perm_pdb, 0xFF, sizeof(perm_pdb));
    uint16_t p_q[PERMUTATIONS];
    uint32_t head = 0, tail = 0;
    perm_pdb[0] = 0;
    p_q[tail++] = 0;
    while (head < tail) {
        uint16_t u = p_q[head++];
        uint8_t d = perm_pdb[u];
        for (uint8_t f = 0; f < 3; ++f) {
            uint16_t v = u;
            for (uint8_t t = 0; t < 3; ++t) {
                v = perm_move[f][v];
                if (perm_pdb[v] == 0xFF) {
                    perm_pdb[v] = (uint8_t)(d + 1U);
                    p_q[tail++] = v;
                }
            }
        }
    }

    memset(ori_pdb, 0xFF, sizeof(ori_pdb));
    uint16_t o_q[ORIENTATIONS];
    head = 0; tail = 0;
    ori_pdb[0] = 0;
    o_q[tail++] = 0;
    while (head < tail) {
        uint16_t u = o_q[head++];
        uint8_t d = ori_pdb[u];
        for (uint8_t f = 0; f < 3; ++f) {
            uint16_t v = u;
            for (uint8_t t = 0; t < 3; ++t) {
                v = ori_move[f][v];
                if (ori_pdb[v] == 0xFF) {
                    ori_pdb[v] = (uint8_t)(d + 1U);
                    o_q[tail++] = v;
                }
            }
        }
    }

    memset(perm_pdb_packed, 0, sizeof(perm_pdb_packed));
    for (uint16_t i = 0; i < PERMUTATIONS; ++i)
        perm_pdb_packed[i >> 1] |= (uint8_t)((perm_pdb[i] & 0x0FU) << ((i & 1U) << 2));

    memset(ori_pdb_packed, 0, sizeof(ori_pdb_packed));
    for (uint16_t i = 0; i < ORIENTATIONS; ++i)
        ori_pdb_packed[i >> 1] |= (uint8_t)((ori_pdb[i] & 0x0FU) << ((i & 1U) << 2));
}

static void build_exact_dist(uint8_t *exact_dist) {
    uint32_t head = 0, tail = 0;
    uint32_t *q = malloc((size_t)STATES * sizeof(*q));
    memset(exact_dist, 0xFF, STATES);
    exact_dist[0] = 0;
    q[tail++] = 0;
    while (head < tail) {
        uint32_t u = q[head++];
        uint8_t d = exact_dist[u];
        uint16_t p = (uint16_t)(u / ORIENTATIONS);
        uint16_t o = (uint16_t)(u % ORIENTATIONS);
        for (uint8_t f = 0; f < 3; ++f) {
            uint16_t np = p, no = o;
            for (uint8_t t = 0; t < 3; ++t) {
                np = perm_move[f][np];
                no = ori_move[f][no];
                uint32_t v = (uint32_t)np * ORIENTATIONS + no;
                if (exact_dist[v] == 0xFF) {
                    exact_dist[v] = (uint8_t)(d + 1U);
                    q[tail++] = v;
                }
            }
        }
    }
    free(q);
}

typedef struct {
    uint64_t nodes;
    uint64_t div_mod_ops;
    uint64_t move_table_loads;
    uint64_t pdb_byte_loads;
} stats_t;

static void profile_stage2_vs_stage3(uint16_t start_p, uint16_t start_o) {
    stats_t s2 = {0, 0, 0, 0}, s3 = {0, 0, 0, 0};

    uint8_t hp = get_perm_pdb_packed(start_p), ho = get_ori_pdb_packed(start_o);
    s2.pdb_byte_loads += 2;
    uint8_t bound = hp > ho ? hp : ho;
    uint16_t st_p[12], st_o[12];
    uint8_t st_move[12], st_last_face[12];
    for (;; ++bound) {
        int depth = 0, found = 0;
        st_p[0] = start_p; st_o[0] = start_o; st_move[0] = 0; st_last_face[0] = 3;
        while (depth >= 0) {
            uint8_t m = st_move[depth];
            if (m >= 9) { --depth; continue; }
            s2.div_mod_ops += 1;
            uint8_t face = (uint8_t)(m / 3U);
            if (face == st_last_face[depth]) { st_move[depth] = (uint8_t)(m + 3U); continue; }
            s2.div_mod_ops += 1;
            uint8_t turn = (uint8_t)(m % 3U);
            st_move[depth] = (uint8_t)(m + 1U);
            uint16_t np = st_p[depth], no = st_o[depth];
            for (uint8_t t = 0; t <= turn; ++t) {
                np = perm_move[face][np];
                no = ori_move[face][no];
                s2.move_table_loads += 2;
            }
            s2.nodes++;
            uint8_t next_g = (uint8_t)(depth + 1);
            if (np == 0 && no == 0) { found = 1; break; }
            uint8_t h1 = get_perm_pdb_packed(np);
            uint8_t h2 = get_ori_pdb_packed(no);
            s2.pdb_byte_loads += 2;
            uint8_t h = h1 > h2 ? h1 : h2;
            if (next_g + h <= bound) {
                depth = next_g; st_p[depth] = np; st_o[depth] = no;
                st_move[depth] = 0; st_last_face[depth] = face;
            }
        }
        if (found) break;
    }

    hp = get_perm_pdb_packed(start_p); ho = get_ori_pdb_packed(start_o);
    s3.pdb_byte_loads += 2;
    bound = hp > ho ? hp : ho;
    uint16_t cur_p[12], cur_o[12];
    uint8_t st_face[12], st_turn[12];
    for (;; ++bound) {
        int depth = 0, found = 0;
        st_p[0] = start_p; st_o[0] = start_o; st_face[0] = 0; st_turn[0] = 0; st_last_face[0] = 3;
        while (depth >= 0) {
            uint8_t f = st_face[depth];
            if (f >= 3) { --depth; continue; }
            if (f == st_last_face[depth]) { st_face[depth] = (uint8_t)(f + 1U); st_turn[depth] = 0; continue; }
            uint8_t t = st_turn[depth];
            uint16_t np, no;
            if (t == 0) {
                np = perm_move[f][st_p[depth]]; no = ori_move[f][st_o[depth]];
            } else {
                np = perm_move[f][cur_p[depth]]; no = ori_move[f][cur_o[depth]];
            }
            s3.move_table_loads += 2;
            cur_p[depth] = np; cur_o[depth] = no;
            if (t == 2) { st_face[depth] = (uint8_t)(f + 1U); st_turn[depth] = 0; }
            else { st_turn[depth] = (uint8_t)(t + 1U); }
            s3.nodes++;
            uint8_t next_g = (uint8_t)(depth + 1);
            if (np == 0 && no == 0) { found = 1; break; }
            if (next_g < bound) {
                uint8_t h1 = get_perm_pdb_packed(np);
                s3.pdb_byte_loads += 1;
                if (next_g + h1 <= bound) {
                    uint8_t h2 = get_ori_pdb_packed(no);
                    s3.pdb_byte_loads += 1;
                    if (next_g + h2 <= bound) {
                        depth = next_g; st_p[depth] = np; st_o[depth] = no;
                        st_face[depth] = 0; st_turn[depth] = 0; st_last_face[depth] = f;
                    }
                }
            }
        }
        if (found) break;
    }

    printf("=== Stage 2 vs Stage 3 Operation Counts on 21345671111111 (dist=11) ===\n");
    printf("Nodes evaluated  : Stage 2 = %llu | Stage 3 = %llu\n",
           (unsigned long long)s2.nodes, (unsigned long long)s3.nodes);
    printf("Div/Mod (/3, %%3) : Stage 2 = %llu | Stage 3 = %llu (-100%%)\n",
           (unsigned long long)s2.div_mod_ops, (unsigned long long)s3.div_mod_ops);
    printf("Move table loads : Stage 2 = %llu | Stage 3 = %llu (-50.0%%)\n",
           (unsigned long long)s2.move_table_loads, (unsigned long long)s3.move_table_loads);
    printf("PDB byte loads   : Stage 2 = %llu | Stage 3 = %llu\n",
           (unsigned long long)s2.pdb_byte_loads, (unsigned long long)s3.pdb_byte_loads);
    printf("=======================================================================\n");
    fflush(stdout);
}

static uint8_t ida_star_length(uint16_t start_p, uint16_t start_o, uint64_t *nodes) {
    uint8_t hp = get_perm_pdb_packed(start_p);
    uint8_t ho = get_ori_pdb_packed(start_o);
    uint8_t bound = hp > ho ? hp : ho;
    if (bound == 0)
        return 0;

    uint16_t st_p[12], st_o[12], cur_p[12], cur_o[12];
    uint8_t st_face[12], st_turn[12], st_last_face[12];

    for (;; ++bound) {
        int depth = 0;
        st_p[0] = start_p;
        st_o[0] = start_o;
        st_face[0] = 0;
        st_turn[0] = 0;
        st_last_face[0] = 3;

        while (depth >= 0) {
            uint8_t f = st_face[depth];
            if (f >= 3) {
                --depth;
                continue;
            }
            if (f == st_last_face[depth]) {
                st_face[depth] = (uint8_t)(f + 1U);
                st_turn[depth] = 0;
                continue;
            }
            uint8_t t = st_turn[depth];
            uint16_t np, no;
            if (t == 0) {
                np = perm_move[f][st_p[depth]];
                no = ori_move[f][st_o[depth]];
            } else {
                np = perm_move[f][cur_p[depth]];
                no = ori_move[f][cur_o[depth]];
            }
            cur_p[depth] = np;
            cur_o[depth] = no;
            ++*nodes;

            if (t == 2) {
                st_face[depth] = (uint8_t)(f + 1U);
                st_turn[depth] = 0;
            } else {
                st_turn[depth] = (uint8_t)(t + 1U);
            }

            uint8_t next_g = (uint8_t)(depth + 1);
            if (np == 0 && no == 0)
                return next_g;

            if (next_g < bound) {
                uint8_t h1 = get_perm_pdb_packed(np);
                if (next_g + h1 <= bound) {
                    uint8_t h2 = get_ori_pdb_packed(no);
                    if (next_g + h2 <= bound) {
                        depth = next_g;
                        st_p[depth] = np;
                        st_o[depth] = no;
                        st_face[depth] = 0;
                        st_turn[depth] = 0;
                        st_last_face[depth] = f;
                    }
                }
            }
        }
    }
}

typedef struct {
    uint32_t rank;
    uint64_t nodes;
} worst_t;

static int worst_cmp(const void *a, const void *b) {
    uint64_t na = ((const worst_t *)a)->nodes, nb = ((const worst_t *)b)->nodes;
    return na < nb ? 1 : na > nb ? -1 : 0;
}

static void print_state(uint32_t rank) {
    state_t state;
    unrank_state(rank, &state);
    for (uint8_t i = 0; i < CUBIES; ++i)
        putchar('1' + state.p[i]);
    for (uint8_t i = 0; i < CUBIES; ++i)
        putchar('1' + state.o[i]);
}

static void emit_half(const char *label, const uint16_t *v, size_t rows, size_t cols) {
    printf("    .align 2\n%s:\n", label);
    for (size_t r = 0; r < rows; ++r)
        for (size_t c = 0; c < cols; ++c)
            printf("%s%u%s", c % 16 ? "," : "    .half ", (unsigned)v[r * cols + c],
                   c % 16 == 15 || c == cols - 1 ? "\n" : "");
}

static void emit_byte(const char *label, const uint8_t *v, size_t n) {
    printf("    .align 2\n%s:\n", label);
    for (size_t i = 0; i < n; ++i)
        printf("%s0x%02x%s", i % 16 ? "," : "    .byte ", (unsigned)v[i],
               i % 16 == 15 || i == n - 1 ? "\n" : "");
}

static int gate_h3(const uint8_t *exact_dist) {
    struct timespec t0, t1;
    uint64_t h3_nodes = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        if (p % 504 == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
            double el = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
            printf("  Stage 3 H3 progress: %3d%% (%u / %u states, %.1f sec)\n",
                   (p * 100) / PERMUTATIONS, (uint32_t)p * ORIENTATIONS, STATES, el);
            fflush(stdout);
        }
        for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
            uint32_t s = (uint32_t)p * ORIENTATIONS + o;
            uint8_t len = ida_star_length(p, o, &h3_nodes);
            if (len != exact_dist[s]) {
                printf("H3 FAIL at state %u\n", s);
                return 1;
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    printf("[PASS] Stage 3 Gate H3: All 3,674,160 states verified! Wall-clock time: %.3f seconds\n", elapsed);
    printf("H3 total nodes: %llu (mean %.1f per state)\n",
           (unsigned long long)h3_nodes, (double)h3_nodes / STATES);
    return 0;
}

int main(int argc, char **argv) {
    int emit = argc == 2 && strcmp(argv[1], "--emit") == 0;
    int no_h3 = argc == 2 && strcmp(argv[1], "--no-h3") == 0;
    if (argc > 2 || (argc == 2 && !emit && !no_h3)) {
        fprintf(stderr, "usage: %s [--emit | --no-h3]\n", argv[0]);
        return 2;
    }
    if (emit) {
        build_tables_and_pdb();
        emit_half("perm_move", &perm_move[0][0], 3, PERMUTATIONS);
        emit_half("ori_move", &ori_move[0][0], 3, ORIENTATIONS);
        emit_byte("perm_pdb", perm_pdb, sizeof(perm_pdb));
        emit_byte("ori_pdb", ori_pdb, sizeof(ori_pdb));
        return 0;
    }

    uint8_t *exact_dist = malloc(STATES);
    if (!exact_dist) return 1;
    build_tables_and_pdb();
    build_exact_dist(exact_dist);

    /* Gate H2: Check table completeness, solved entry, and max values */
    uint8_t max_perm = 0, max_ori = 0;
    for (int i = 0; i < PERMUTATIONS; ++i) {
        if (perm_pdb[i] == 0xFF) { printf("H2 FAIL: perm_pdb incomplete\n"); return 1; }
        if (perm_pdb[i] > max_perm) max_perm = perm_pdb[i];
    }
    for (int i = 0; i < ORIENTATIONS; ++i) {
        if (ori_pdb[i] == 0xFF) { printf("H2 FAIL: ori_pdb incomplete\n"); return 1; }
        if (ori_pdb[i] > max_ori) max_ori = ori_pdb[i];
    }
    if (perm_pdb[0] != 0 || ori_pdb[0] != 0) { printf("H2 FAIL: solved != 0\n"); return 1; }
    printf("[PASS] Gate H2: perm_pdb (5040 entries, solved=%d, max=%d), ori_pdb (729 entries, solved=%d, max=%d)\n",
           perm_pdb[0], max_perm, ori_pdb[0], max_ori);

    /* Gate H4: Verify packed accessors at even and odd indices */
    for (uint16_t i = 0; i < PERMUTATIONS; ++i) {
        if (get_perm_pdb_packed(i) != perm_pdb[i]) { printf("H4 FAIL at perm %d\n", i); return 1; }
    }
    for (uint16_t i = 0; i < ORIENTATIONS; ++i) {
        if (get_ori_pdb_packed(i) != ori_pdb[i]) { printf("H4 FAIL at ori %d\n", i); return 1; }
    }
    printf("[PASS] Gate H4: 4-bit packed nibble accessors match unpacked reference at all even and odd indices\n");

    /* Gate H1: Verify admissibility h(s) <= d(s) across all 3,674,160 states */
    for (uint32_t s = 0; s < STATES; ++s) {
        uint16_t p = (uint16_t)(s / ORIENTATIONS);
        uint16_t o = (uint16_t)(s % ORIENTATIONS);
        uint8_t h1 = get_perm_pdb_packed(p);
        uint8_t h2 = get_ori_pdb_packed(o);
        uint8_t h = h1 > h2 ? h1 : h2;
        if (h > exact_dist[s]) {
            printf("H1 FAIL at state %u: h=%d > d=%d\n", s, h, exact_dist[s]);
            return 1;
        }
    }
    printf("[PASS] Gate H1: Admissibility h(s) <= d(s) verified across all 3,674,160 states\n");

    state_t test_s = {{1, 0, 2, 3, 4, 5, 6}, {0, 0, 0, 0, 0, 0, 0}};
    uint32_t rk = rank_state(&test_s);
    profile_stage2_vs_stage3((uint16_t)(rk / ORIENTATIONS), (uint16_t)(rk % ORIENTATIONS));

    if (no_h3)
        printf("[SKIP] Stage 3 Gate H3 (--no-h3)\n");
    else if (gate_h3(exact_dist))
        return 1;

    enum { TOP = 10 };
    uint32_t count = 0;
    for (uint32_t s = 0; s < STATES; ++s)
        count += exact_dist[s] == 11;
    worst_t *worst = count ? malloc(count * sizeof(*worst)) : NULL;
    if (!worst) return 1;
    uint64_t total = 0;
    uint32_t n = 0;
    for (uint32_t s = 0; s < STATES; ++s) {
        if (exact_dist[s] != 11)
            continue;
        uint64_t nodes = 0;
        if (ida_star_length((uint16_t)(s / ORIENTATIONS), (uint16_t)(s % ORIENTATIONS), &nodes) != 11) {
            printf("WORST FAIL at state %u\n", s);
            return 1;
        }
        worst[n].rank = s;
        worst[n].nodes = nodes;
        total += nodes;
        ++n;
    }
    qsort(worst, count, sizeof(*worst), worst_cmp);
    printf("=== %u distance-11 states, Stage 3 IDA* nodes: min %llu, median %llu, mean %.1f, max %llu ===\n",
           count, (unsigned long long)worst[count - 1].nodes, (unsigned long long)worst[count / 2].nodes,
           (double)total / count, (unsigned long long)worst[0].nodes);
    for (uint32_t i = 0; i < count; ++i) {
        if (i >= TOP && worst[i].rank != rk)
            continue;
        uint16_t p = (uint16_t)(worst[i].rank / ORIENTATIONS), o = (uint16_t)(worst[i].rank % ORIENTATIONS);
        uint8_t hp = get_perm_pdb_packed(p), ho = get_ori_pdb_packed(o);
        printf("  #%-4u ", i + 1);
        print_state(worst[i].rank);
        printf("  nodes %llu  h0 %u%s\n", (unsigned long long)worst[i].nodes, (unsigned)(hp > ho ? hp : ho),
               worst[i].rank == rk ? "  (T6 test state)" : "");
    }
    free(worst);

    free(exact_dist);
    return 0;
}

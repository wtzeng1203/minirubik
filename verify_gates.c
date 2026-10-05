#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9
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

static void build_tables_and_pdb(uint8_t *exact_dist) {
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

    /* BFS for Permutation PDB */
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

    /* BFS for Orientation PDB */
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

    /* Pack into 4-bit nibbles */
    memset(perm_pdb_packed, 0, sizeof(perm_pdb_packed));
    for (uint16_t i = 0; i < PERMUTATIONS; ++i)
        perm_pdb_packed[i >> 1] |= (uint8_t)((perm_pdb[i] & 0x0FU) << ((i & 1U) << 2));

    memset(ori_pdb_packed, 0, sizeof(ori_pdb_packed));
    for (uint16_t i = 0; i < ORIENTATIONS; ++i)
        ori_pdb_packed[i >> 1] |= (uint8_t)((ori_pdb[i] & 0x0FU) << ((i & 1U) << 2));

    /* Exact BFS distance oracle */
    uint32_t *q = malloc((size_t)STATES * sizeof(*q));
    memset(exact_dist, 0xFF, STATES);
    head = 0; tail = 0;
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

/* Non-recursive iterative IDA* search returning shortest solution length */
static uint8_t ida_star_length(uint16_t start_p, uint16_t start_o) {
    uint8_t hp = get_perm_pdb_packed(start_p);
    uint8_t ho = get_ori_pdb_packed(start_o);
    uint8_t bound = hp > ho ? hp : ho;
    if (bound == 0)
        return 0;

    uint16_t st_p[12], st_o[12];
    uint8_t st_move[12], st_last_face[12];

    for (;; ++bound) {
        int depth = 0;
        st_p[0] = start_p;
        st_o[0] = start_o;
        st_move[0] = 0;
        st_last_face[0] = 3; /* 3 = none */

        while (depth >= 0) {
            uint8_t m = st_move[depth];
            if (m >= 9) {
                --depth;
                continue;
            }
            uint8_t face = (uint8_t)(m / 3U);
            if (face == st_last_face[depth]) {
                st_move[depth] = (uint8_t)(m + 3U);
                continue;
            }
            uint8_t turn = (uint8_t)(m % 3U);
            st_move[depth] = (uint8_t)(m + 1U);

            uint16_t np = st_p[depth], no = st_o[depth];
            for (uint8_t t = 0; t <= turn; ++t) {
                np = perm_move[face][np];
                no = ori_move[face][no];
            }

            uint8_t next_g = (uint8_t)(depth + 1);
            if (np == 0 && no == 0)
                return next_g;

            uint8_t h1 = get_perm_pdb_packed(np);
            uint8_t h2 = get_ori_pdb_packed(no);
            uint8_t h = h1 > h2 ? h1 : h2;

            if (next_g + h <= bound) {
                depth = next_g;
                st_p[depth] = np;
                st_o[depth] = no;
                st_move[depth] = 0;
                st_last_face[depth] = face;
            }
        }
    }
}

int main(void) {
    uint8_t *exact_dist = malloc(STATES);
    if (!exact_dist) return 1;
    build_tables_and_pdb(exact_dist);

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

    /* Gate H3: Full-domain IDA* optimality check and wall-clock time */
    printf("Running Gate H3 across all 3,674,160 states (please wait ~10-30 seconds)...\n");
    fflush(stdout);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint32_t s = 0; s < STATES; ++s) {
        uint16_t p = (uint16_t)(s / ORIENTATIONS);
        uint16_t o = (uint16_t)(s % ORIENTATIONS);
        uint8_t len = ida_star_length(p, o);
        if (len != exact_dist[s]) {
            printf("H3 FAIL at state %u: ida_len=%d != exact_d=%d\n", s, len, exact_dist[s]);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    printf("[PASS] Gate H3: All 3,674,160 states returned exact optimal distance! Wall-clock time: %.3f seconds\n", elapsed);

    free(exact_dist);
    return 0;
}

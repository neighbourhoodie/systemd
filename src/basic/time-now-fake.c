/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "hashmap.h"
#include "time-util.h"
#include "time-now.h"

static void clockid_hash_func(const clockid_t *cid, struct siphash *state) {
        siphash24_compress_typesafe(*cid, state);
}

static int clockid_compare_func(const clockid_t *x, const clockid_t *y) {
        return CMP(*x, *y);
}

DEFINE_HASH_OPS(clockid_hash_ops, clockid_t, clockid_hash_func, clockid_compare_func);

static Hashmap *CURRENT_TIME = NULL;

static void init_current_time(clockid_t cid) {
        struct timespec ts;
        clockid_t *cid_box = NULL;
        nsec_t *time_box = NULL;

        hashmap_ensure_allocated(&CURRENT_TIME, &clockid_hash_ops);

        if (!hashmap_contains(CURRENT_TIME, &cid)) {
                cid_box = calloc(1, sizeof(clockid_t));
                *cid_box = cid;

                time_box = calloc(1, sizeof(nsec_t));
                assert_se(clock_gettime(cid, &ts) == 0);
                *time_box = timespec_load_nsec(&ts);

                hashmap_put(CURRENT_TIME, cid_box, time_box);
        }
}

static nsec_t* time_entry(clockid_t cid) {
        cid = map_clock_id(cid);
        init_current_time(cid);

        return hashmap_get(CURRENT_TIME, &cid);
}

usec_t now(clockid_t cid) {
        return now_nsec(cid) / NSEC_PER_USEC;
}

nsec_t now_nsec(clockid_t cid) {
        nsec_t *time = time_entry(cid);

        return *time;
}

void tick_sec(unsigned int sec, clockid_t cid) {
        nsec_t *time = time_entry(cid);

        *time += sec * NSEC_PER_SEC;
}

/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <sys/time.h>

#include "time-util.h"
#include "time-now.h"

usec_t now(clockid_t clock_id) {
        struct timespec ts;

        assert_se(clock_gettime(map_clock_id(clock_id), &ts) == 0);

        return timespec_load(&ts);
}

nsec_t now_nsec(clockid_t clock_id) {
        struct timespec ts;

        assert_se(clock_gettime(map_clock_id(clock_id), &ts) == 0);

        return timespec_load_nsec(&ts);
}

void tick_sec(unsigned int sec, clockid_t cid) {
        sleep(sec);
}

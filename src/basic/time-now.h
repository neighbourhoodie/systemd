/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

usec_t now(clockid_t clock);
nsec_t now_nsec(clockid_t clock);
void tick_sec(unsigned int sec, clockid_t cid);

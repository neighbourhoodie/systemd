/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <sys/socket.h>

#include "dns-type.h"
#include "resolve-util.h"
#include "resolved-def.h"
#include "resolved-dns-answer.h"
#include "resolved-dns-cache.h"
#include "resolved-dns-dnssec.h"
#include "resolved-dns-packet.h"
#include "resolved-dns-rr.h"

#include "log.h"
#include "tests.h"

static DnsCache new_cache(void) {
        return (DnsCache) {};
}

typedef struct PutArgs {
        DnsCacheMode cache_mode;
        DnsProtocol protocol;
        DnsResourceKey *key;
        int rcode;
        DnsAnswer *answer;
        DnsPacket *full_packet;
        uint64_t query_flags;
        DnssecResult dnssec_result;
        uint32_t nsec_ttl;
        int owner_family;
        const union in_addr_union owner_address;
        usec_t stale_retention_usec;
} PutArgs;

static PutArgs mk_put_args(void) {
        return (PutArgs) {
                .cache_mode = DNS_CACHE_MODE_YES,
                .protocol = DNS_PROTOCOL_DNS,
                .key = NULL,
                .rcode = DNS_RCODE_SUCCESS,
                .answer = dns_answer_new(0),
                .full_packet = NULL,
                .query_flags = SD_RESOLVED_AUTHENTICATED | SD_RESOLVED_CONFIDENTIAL,
                .dnssec_result = DNSSEC_UNSIGNED,
                .nsec_ttl = 3600,
                .owner_family = AF_INET,
                .owner_address = { .in.s_addr = htobe32(0x01020304) },
                .stale_retention_usec = 0
        };
}

static int cache_put(DnsCache *cache, PutArgs *args) {
        return dns_cache_put(cache,
                args->cache_mode,
                args->protocol,
                args->key,
                args->rcode,
                args->answer,
                args->full_packet,
                args->query_flags,
                args->dnssec_result,
                args->nsec_ttl,
                args->owner_family,
                &args->owner_address,
                args->stale_retention_usec);
}

static void dns_cache_unrefp(DnsCache *cache) {
        dns_cache_flush(cache);
}

static void put_args_unrefp(PutArgs *args) {
        dns_resource_key_unref(args->key);
        dns_answer_unref(args->answer);
        dns_packet_unref(args->full_packet);
}

static void answer_add_a(PutArgs *args, DnsResourceKey *key, int addr, int ttl, DnsAnswerFlags flags) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new(key);
        rr->a.in_addr.s_addr = htobe32(addr);
        rr->ttl = ttl;
        dns_answer_add(args->answer, rr, 1, flags, NULL);
}

static void answer_add_cname(PutArgs *args, DnsResourceKey *key, const char *alias, int ttl, DnsAnswerFlags flags) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new(key);
        rr->cname.name = strdup(alias);
        rr->ttl = ttl;
        dns_answer_add(args->answer, rr, 1, flags, NULL);
}

TEST(dns_a_success_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;
        answer_add_a(&put_args, put_args.key, 0xc0a8017f, 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_non_matching_type_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        answer_add_a(&put_args, key, 0xc0a8017f, 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_non_matching_name_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        answer_add_a(&put_args, key, 0xc0a8017f, 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_escaped_key_returns_error) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.\\example.com");
        answer_add_a(&put_args, key, 0xc0a8017f, 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_ERROR(cache_put(&cache, &put_args), EINVAL);
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_empty_answer_is_not_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

TEST(dns_a_nxdomain_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_NXDOMAIN;
        dns_answer_add_soa(put_args.answer, "example.com", 3600, 0);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_servfail_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SERVFAIL;

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_refused_is_not_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_REFUSED;

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_zero_ttl_is_not_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;
        answer_add_a(&put_args, put_args.key, 0xc0a8017f, 0, DNS_ANSWER_CACHEABLE);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

TEST(dns_a_success_not_cacheable_is_not_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;
        answer_add_a(&put_args, put_args.key, 0xc0a8017f, 3600, 0);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

TEST(dns_a_to_cname_success_is_cached) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        answer_add_cname(&put_args, key, "example.com", 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_OK(cache_put(&cache, &put_args));
        ASSERT_FALSE(dns_cache_is_empty(&cache));
}

TEST(dns_a_to_cname_success_escaped_name_returns_error) {
        _cleanup_(dns_cache_unrefp) DnsCache cache = new_cache();
        _cleanup_(put_args_unrefp) PutArgs put_args = mk_put_args();
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        put_args.key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        put_args.rcode = DNS_RCODE_SUCCESS;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.\\example.com");
        answer_add_cname(&put_args, key, "example.com", 3600, DNS_ANSWER_CACHEABLE);

        ASSERT_ERROR(cache_put(&cache, &put_args), EINVAL);
        ASSERT_TRUE(dns_cache_is_empty(&cache));
}

DEFINE_TEST_MAIN(LOG_DEBUG);

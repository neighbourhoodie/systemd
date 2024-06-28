/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "dns-type.h"
#include "resolved-dns-rr.h"

#include "log.h"
#include "tests.h"

/* ================================================================
 * DNS_RESOURCE_RECORD_RDATA()
 * ================================================================ */

TEST(dns_resource_record_rdata) {
        DnsResourceRecord rr = (DnsResourceRecord) {
                .wire_format = (void *)"abcdefghi",
                .wire_format_size = 9,
                .wire_format_rdata_offset = 3
        };

        const void *ptr = DNS_RESOURCE_RECORD_RDATA(&rr);
        ASSERT_STREQ(ptr, "defghi");

        size_t size = DNS_RESOURCE_RECORD_RDATA_SIZE(&rr);
        ASSERT_EQ(size, 6u);

        rr.wire_format = NULL;

        ptr = DNS_RESOURCE_RECORD_RDATA(&rr);
        ASSERT_NULL(ptr);

        size = DNS_RESOURCE_RECORD_RDATA_SIZE(&rr);
        ASSERT_EQ(size, 0u);
}

/* ================================================================
 * dns_resource_key_new()
 * ================================================================ */

TEST(dns_resource_key_new) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_EQ(key->class, DNS_CLASS_IN);
        ASSERT_EQ(key->type, DNS_TYPE_A);
        ASSERT_STREQ(dns_resource_key_name(key), "www.example.com");
}

/* ================================================================
 * dns_resource_key_new_redirect()
 * ================================================================ */

TEST(dns_resource_key_new_redirect_cname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *redirected = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        cname = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        cname->cname.name = strdup("example.com");

        redirected = dns_resource_key_new_redirect(key, cname);

        ASSERT_EQ(redirected->class, DNS_CLASS_IN);
        ASSERT_EQ(redirected->type, DNS_TYPE_A);
        ASSERT_STREQ(dns_resource_key_name(redirected), "example.com");
}

TEST(dns_resource_key_new_redirect_dname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *redirected = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *dname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        dname = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_DNAME, "example.com");
        dname->dname.name = strdup("v2.example.com");

        redirected = dns_resource_key_new_redirect(key, dname);

        ASSERT_EQ(redirected->class, DNS_CLASS_IN);
        ASSERT_EQ(redirected->type, DNS_TYPE_A);
        ASSERT_STREQ(dns_resource_key_name(redirected), "www.v2.example.com");
}

/* ================================================================
 * dns_resource_key_new_append_suffix()
 * ================================================================ */

TEST(dns_resource_key_new_append_suffix_root) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *source = NULL, *target = NULL;

        source = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_OK(dns_resource_key_new_append_suffix(&target, source, (char *)""));
        ASSERT_TRUE(target == source);

        ASSERT_OK(dns_resource_key_new_append_suffix(&target, source, (char *)"."));
        ASSERT_TRUE(target == source);
}

TEST(dns_resource_key_new_append_suffix_not_root) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *source = NULL, *target = NULL;

        source = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");

        ASSERT_OK(dns_resource_key_new_append_suffix(&target, source, (char *)"com"));
        ASSERT_FALSE(target == source);
        ASSERT_STREQ(dns_resource_key_name(target), "www.example.com");
}

/* ================================================================
 * dns_resource_key_is_*()
 * ================================================================ */

TEST(dns_resource_key_is_address) {
        DnsResourceKey *key = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_TRUE(dns_resource_key_is_address(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        ASSERT_TRUE(dns_resource_key_is_address(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A6, "www.example.com");
        ASSERT_FALSE(dns_resource_key_is_address(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        ASSERT_FALSE(dns_resource_key_is_address(key));
        dns_resource_key_unref(key);
}

TEST(dns_resource_key_is_dnssd_ptr) {
        DnsResourceKey *key = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "_tcp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "foo._tcp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "_udp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "bar._udp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "_tcp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "_abc.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "foo_tcp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_ptr(key));
        dns_resource_key_unref(key);
}

TEST(dns_resource_key_is_dnssd_two_label_ptr) {
        DnsResourceKey *key = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "_tcp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "foo._tcp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "_udp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "bar._udp.local");
        ASSERT_TRUE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "foo._tcp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "foo._abc.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_PTR, "foo_tcp.local");
        ASSERT_FALSE(dns_resource_key_is_dnssd_two_label_ptr(key));
        dns_resource_key_unref(key);
}

/* ================================================================
 * dns_resource_key_equal()
 * ================================================================ */

TEST(dns_resource_key_equal_same_pointer) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_TRUE(dns_resource_key_equal(a, a));
}

TEST(dns_resource_key_equal_equal_name) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_TRUE(dns_resource_key_equal(a, b));
}

TEST(dns_resource_key_equal_case_insensitive_name) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.EXAMPLE.com");
        ASSERT_TRUE(dns_resource_key_equal(a, b));
}

TEST(dns_resource_key_equal_trailing_dot) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com.");
        ASSERT_TRUE(dns_resource_key_equal(a, b));
}

TEST(dns_resource_key_equal_different_names) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.org");
        ASSERT_FALSE(dns_resource_key_equal(a, b));
}

TEST(dns_resource_key_equal_different_classes) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_ANY, DNS_TYPE_A, "www.example.com");
        ASSERT_FALSE(dns_resource_key_equal(a, b));
}

TEST(dns_resource_key_equal_different_types) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        ASSERT_FALSE(dns_resource_key_equal(a, b));
}

/* ================================================================
 * dns_resource_key_match_rr()
 * ================================================================ */

TEST(dns_resource_key_match_rr_simple) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_any_class) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_ANY, DNS_TYPE_A, "www.example.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_any_type) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_ANY, "www.example.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_different_type) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_different_name) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.other.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_case_insensitive_name) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.EXAMPLE.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_escape_error) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.\\example.com");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_ERROR(dns_resource_key_match_rr(key, rr, NULL), EINVAL);
}

TEST(dns_resource_key_match_rr_search_domain) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_rr(key, rr, "com"));
}

TEST(dns_resource_key_match_rr_no_search_domain) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_rr(key, rr, NULL));
}

TEST(dns_resource_key_match_rr_different_search_domain) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_rr(key, rr, "org"));
}

/* ================================================================
 * dns_resource_key_match_cname_or_dname()
 * ================================================================ */

TEST(dns_resource_key_match_cname_or_dname_simple) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_any_class) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_ANY, DNS_TYPE_A, "www.example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_bad_type) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_NSEC, "www.example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_case_insensitive_cname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.EXAMPLE.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_prefix_cname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_suffix_cname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_search_domain_cname_pass) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, "com"));
}

TEST(dns_resource_key_match_cname_or_dname_search_domain_cname_fail) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, "org"));
}

TEST(dns_resource_key_match_cname_or_dname_case_insensitive_dname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.EXAMPLE.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_DNAME, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_prefix_dname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_DNAME, "example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_suffix_dname) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_DNAME, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, NULL));
}

TEST(dns_resource_key_match_cname_or_dname_search_domain_dname_pass) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_DNAME, "example.com");

        ASSERT_TRUE(dns_resource_key_match_cname_or_dname(key, cname, "com"));
}

TEST(dns_resource_key_match_cname_or_dname_search_domain_dname_fail) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *cname = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example");
        cname = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_DNAME, "example.com");

        ASSERT_FALSE(dns_resource_key_match_cname_or_dname(key, cname, "org"));
}

/* ================================================================
 * dns_resource_key_match_soa()
 * ================================================================ */

TEST(dns_resource_key_match_soa_simple) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *soa = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        soa = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");

        ASSERT_TRUE(dns_resource_key_match_soa(key, soa));
}

TEST(dns_resource_key_no_match_soa_any_class) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *soa = NULL;

        key = dns_resource_key_new(DNS_CLASS_ANY, DNS_TYPE_A, "www.example.com");
        soa = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_soa(key, soa));
}

TEST(dns_resource_key_no_match_soa_bad_type) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *soa = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        soa = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_soa(key, soa));
}

TEST(dns_resource_key_match_soa_child_domain) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *soa = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        soa = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_SOA, "example.com");

        ASSERT_TRUE(dns_resource_key_match_soa(key, soa));
}

TEST(dns_resource_key_no_match_soa_parent_domain) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL, *soa = NULL;

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        soa = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");

        ASSERT_FALSE(dns_resource_key_match_soa(key, soa));
}

/* ================================================================
 * dns_resource_key_to_string()
 * ================================================================ */

TEST(dns_resource_key_to_string) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        char str[256];

        char *ans = dns_resource_key_to_string(key, str, 256);
        ASSERT_TRUE(ans == str);
        ASSERT_STREQ(ans, "www.example.com IN CNAME");
}

/* ================================================================
 * dns_resource_key_reduce()
 * ================================================================ */

TEST(dns_resource_key_reduce_same_pointer) {
        DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = a;

        ASSERT_TRUE(dns_resource_key_reduce(&a, &b));
        ASSERT_TRUE(a == b);

        dns_resource_key_unref(a);
}

TEST(dns_resource_key_reduce_same_values) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(a != b);

        ASSERT_TRUE(dns_resource_key_reduce(&a, &b));
        ASSERT_TRUE(a == b);
}

TEST(dns_resource_key_reduce_different_values) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");

        ASSERT_TRUE(a != b);

        ASSERT_FALSE(dns_resource_key_reduce(&a, &b));
        ASSERT_TRUE(a != b);
}

TEST(dns_resource_key_reduce_refcount) {
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *a = NULL, *b = NULL, *c = NULL;

        a = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        c = b;

        ASSERT_TRUE(a != b);

        a->n_ref = 3;
        b->n_ref = 2;

        ASSERT_TRUE(dns_resource_key_reduce(&a, &b));
        ASSERT_TRUE(a == b);

        ASSERT_EQ(a->n_ref, 4u);
        ASSERT_EQ(c->n_ref, 1u);
}

/* ================================================================
 * dns_resource_record_new_address()
 * ================================================================ */

TEST(dns_resource_record_new_address_ipv4) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        union in_addr_union addr = { .in.s_addr = htobe32(0xc0a8017f) };

        ASSERT_OK(dns_resource_record_new_address(&rr, AF_INET, &addr, "www.example.com"));

        ASSERT_EQ(rr->key->class, DNS_CLASS_IN);
        ASSERT_EQ(rr->key->type, DNS_TYPE_A);
        ASSERT_STREQ(dns_resource_key_name(rr->key), "www.example.com");
        ASSERT_EQ(rr->a.in_addr.s_addr, addr.in.s_addr);
}

TEST(dns_resource_record_new_address_ipv6) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        union in_addr_union addr = {
                .in6.s6_addr = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03 }
        };

        ASSERT_OK(dns_resource_record_new_address(&rr, AF_INET6, &addr, "www.example.com"));

        ASSERT_EQ(rr->key->class, DNS_CLASS_IN);
        ASSERT_EQ(rr->key->type, DNS_TYPE_AAAA);
        ASSERT_STREQ(dns_resource_key_name(rr->key), "www.example.com");
        ASSERT_EQ(memcmp(&rr->aaaa.in6_addr, &addr.in6, sizeof(struct in6_addr)), 0);
}

/* ================================================================
 * dns_resource_record_new_reverse()
 * ================================================================ */

TEST(dns_resource_record_new_reverse) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *rr = NULL;

        union in_addr_union addr = { .in.s_addr = htobe32(0xc0a8017f) };

        ASSERT_OK(dns_resource_record_new_reverse(&rr, AF_INET, &addr, "www.example.com"));

        ASSERT_EQ(rr->key->class, DNS_CLASS_IN);
        ASSERT_EQ(rr->key->type, DNS_TYPE_PTR);
        ASSERT_STREQ(dns_resource_key_name(rr->key), "127.1.168.192.in-addr.arpa");
        ASSERT_STREQ(rr->ptr.name, "www.example.com");
}

/* ================================================================
 * dns_resource_record_equal() : general cases
 * ================================================================ */

TEST(dns_resource_record_equal_same_pointer) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_TRUE(dns_resource_record_equal(a, a));
}

TEST(dns_resource_record_equal_equal_name) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_case_insensitive_name) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.EXAMPLE.com");
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_trailing_dot) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com.");
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_different_names) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.org");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_different_classes) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_ANY, DNS_TYPE_A, "www.example.com");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_different_types) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        b = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

/* ================================================================
 * dns_resource_record_equal() : A, AAAA
 * ================================================================ */

TEST(dns_resource_record_equal_a_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        a->a.in_addr.s_addr = htobe32(0xc0a8017f);

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_a_fail) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        a->a.in_addr.s_addr = htobe32(0xc0a8017f);

        b = dns_resource_record_copy(a);
        b->a.in_addr.s_addr = htobe32(0xc0a80180);
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_aaaa_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        a->aaaa.in6_addr = (struct in6_addr) { .s6_addr = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03 } };

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_aaaa_fail) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_AAAA, "www.example.com");
        a->aaaa.in6_addr = (struct in6_addr) { .s6_addr = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03 } };

        b = dns_resource_record_copy(a);
        b->aaaa.in6_addr = (struct in6_addr) { .s6_addr = { 0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04 } };
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

/* ================================================================
 * dns_resource_record_equal() : NS
 * ================================================================ */

TEST(dns_resource_record_equal_ns_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_NS, "www.example.com");
        a->ns.name = strdup("ns1.example.com");

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_ns_fail) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_NS, "www.example.com");
        a->ns.name = strdup("ns1.example.com");

        b = dns_resource_record_copy(a);
        b->ns.name = strdup("ns2.example.com");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

/* ================================================================
 * dns_resource_record_equal() : CNAME
 * ================================================================ */

TEST(dns_resource_record_equal_cname_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        a->cname.name = strdup("example.com");

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_cname_fail) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        a->cname.name = strdup("example.com");

        b = dns_resource_record_copy(a);
        b->cname.name = strdup("example.orb");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

/* ================================================================
 * dns_resource_record_equal() : SOA
 * ================================================================ */

TEST(dns_resource_record_equal_soa_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_mname) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.mname = strdup("ns.example.org");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_rname) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.rname = strdup("admin.example.org");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_serial) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.serial = 1111111112;
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_refresh) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.refresh = 86401;
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_retry) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.retry = 7201;
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_expire) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.expire = 4000001;
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_soa_bad_minimum) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SOA, "www.example.com");
        a->soa.mname = strdup("ns.example.com");
        a->soa.rname = strdup("admin.example.com");
        a->soa.serial = 1111111111;
        a->soa.refresh = 86400;
        a->soa.retry = 7200;
        a->soa.expire = 4000000;
        a->soa.minimum = 3600;

        b = dns_resource_record_copy(a);
        b->soa.minimum = 3601;
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

/* ================================================================
 * dns_resource_record_equal() : PTR
 * ================================================================ */

TEST(dns_resource_record_equal_ptr_copy) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_PTR, "127.1.168.192.in-addr-arpa");
        a->ptr.name = strdup("example.com");

        b = dns_resource_record_copy(a);
        ASSERT_TRUE(dns_resource_record_equal(a, b));
}

TEST(dns_resource_record_equal_ptr_fail) {
        _cleanup_(dns_resource_record_unrefp) DnsResourceRecord *a = NULL, *b = NULL;

        a = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_PTR, "127.1.168.192.in-addr-arpa");
        a->ptr.name = strdup("example.com");

        b = dns_resource_record_copy(a);
        b->ptr.name = strdup("example.org");
        ASSERT_FALSE(dns_resource_record_equal(a, b));
}

DEFINE_TEST_MAIN(LOG_DEBUG);

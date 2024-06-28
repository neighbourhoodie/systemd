/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "dns-type.h"
#include "resolved-dns-packet.h"
#include "resolved-dns-rr.h"

#include "list.h"
#include "log.h"
#include "tests.h"

#define BIT_QR (1 << 7)
#define BIT_AA (1 << 2)
#define BIT_TC (1 << 1)
#define BIT_RD (1 << 0)

#define BIT_RA (1 << 7)
#define BIT_AD (1 << 5)
#define BIT_CD (1 << 4)

/* ================================================================
 * dns_packet_set_flags()
 * ================================================================ */

TEST(packet_set_flags_dns_checking_enabled) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX);
        dns_packet_set_flags(packet, false, false);

        ASSERT_EQ(DNS_PACKET_QR(packet), 0);
        ASSERT_EQ(DNS_PACKET_OPCODE(packet), 0);
        ASSERT_EQ(DNS_PACKET_AA(packet), 0);
        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_RD(packet), 1);

        ASSERT_EQ(DNS_PACKET_RA(packet), 0);
        ASSERT_EQ(DNS_PACKET_AD(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 0);
        ASSERT_EQ(DNS_PACKET_RCODE(packet), 0);
}

TEST(packet_set_flags_dns_checking_disabled) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX);
        dns_packet_set_flags(packet, true, false);

        ASSERT_EQ(DNS_PACKET_QR(packet), 0);
        ASSERT_EQ(DNS_PACKET_OPCODE(packet), 0);
        ASSERT_EQ(DNS_PACKET_AA(packet), 0);
        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_RD(packet), 1);

        ASSERT_EQ(DNS_PACKET_RA(packet), 0);
        ASSERT_EQ(DNS_PACKET_AD(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 1);
        ASSERT_EQ(DNS_PACKET_RCODE(packet), 0);
}

TEST(packet_set_flags_llmnr) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new(&packet, DNS_PROTOCOL_LLMNR, 0, DNS_PACKET_SIZE_MAX);
        dns_packet_set_flags(packet, true, false);

        ASSERT_EQ(DNS_PACKET_QR(packet), 0);
        ASSERT_EQ(DNS_PACKET_OPCODE(packet), 0);
        ASSERT_EQ(DNS_PACKET_AA(packet), 0);
        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_RD(packet), 0);

        ASSERT_EQ(DNS_PACKET_RA(packet), 0);
        ASSERT_EQ(DNS_PACKET_AD(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 0);
        ASSERT_EQ(DNS_PACKET_RCODE(packet), 0);
}

TEST(packet_set_flags_mdns_not_truncated) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new(&packet, DNS_PROTOCOL_MDNS, 0, DNS_PACKET_SIZE_MAX);
        dns_packet_set_flags(packet, true, false);

        ASSERT_EQ(DNS_PACKET_QR(packet), 0);
        ASSERT_EQ(DNS_PACKET_OPCODE(packet), 0);
        ASSERT_EQ(DNS_PACKET_AA(packet), 0);
        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_RD(packet), 0);

        ASSERT_EQ(DNS_PACKET_RA(packet), 0);
        ASSERT_EQ(DNS_PACKET_AD(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 0);
        ASSERT_EQ(DNS_PACKET_RCODE(packet), 0);
}

TEST(packet_set_flags_mdns_truncated) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new(&packet, DNS_PROTOCOL_MDNS, 0, DNS_PACKET_SIZE_MAX);
        dns_packet_set_flags(packet, true, true);

        ASSERT_EQ(DNS_PACKET_QR(packet), 0);
        ASSERT_EQ(DNS_PACKET_OPCODE(packet), 0);
        ASSERT_EQ(DNS_PACKET_AA(packet), 0);
        ASSERT_EQ(DNS_PACKET_TC(packet), 1);
        ASSERT_EQ(DNS_PACKET_RD(packet), 0);

        ASSERT_EQ(DNS_PACKET_RA(packet), 0);
        ASSERT_EQ(DNS_PACKET_AD(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 0);
        ASSERT_EQ(DNS_PACKET_RCODE(packet), 0);
}

/* ================================================================
 * dns_packet_new_query()
 * ================================================================ */

TEST(packet_new_query_checking_enabled) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new_query(&packet, DNS_PROTOCOL_DNS, 0, false);

        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 0);
}

TEST(packet_new_query_checking_disabled) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        dns_packet_new_query(&packet, DNS_PROTOCOL_DNS, 0, true);

        ASSERT_EQ(DNS_PACKET_TC(packet), 0);
        ASSERT_EQ(DNS_PACKET_CD(packet), 1);
}

/* ================================================================
 * dns_packet_append_key()
 * ================================================================ */

TEST(packet_append_key_single_a) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        DnsResourceKey *key = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->qdcount = htobe16(1);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_OK(dns_packet_append_key(packet, key, 0, NULL));
        dns_resource_key_unref(key);

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x01,     0x00, 0x00,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, 'w', 'w', 'w',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_key_single_soa_any_class) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        DnsResourceKey *key = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->qdcount = htobe16(1);

        key = dns_resource_key_new(DNS_CLASS_ANY, DNS_TYPE_SOA, "www.example.com");
        ASSERT_OK(dns_packet_append_key(packet, key, 0, NULL));
        dns_resource_key_unref(key);

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x01,     0x00, 0x00,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, 'w', 'w', 'w',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* SOA */       0x00, 0x06,
        /* ANY */       0x00, 0xff
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

/* ================================================================
 * dns_packet_append_question()
 * ================================================================ */

TEST(packet_append_question_compression) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_question_unrefp) DnsQuestion *question = NULL;
        DnsResourceKey *key = NULL;

        question = dns_question_new(3);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        ASSERT_OK(dns_question_add(question, key, 0));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_MX, "mail.example.com");
        ASSERT_OK(dns_question_add(question, key, 0));
        dns_resource_key_unref(key);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_SOA, "host.mail.example.com");
        ASSERT_OK(dns_question_add(question, key, 0));
        dns_resource_key_unref(key);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->qdcount = htobe16(dns_question_size(question));

        ASSERT_OK(dns_packet_append_question(packet, question));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x03,     0x00, 0x00,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, 'w', 'w', 'w',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01,

        /* name */      0x04, 'm', 'a', 'i', 'l',
                        0xc0, 0x10,
        /* MX */        0x00, 0x0f,
        /* IN */        0x00, 0x01,

        /* name */      0x04, 'h', 'o', 's', 't',
                        0xc0, 0x21,
        /* SOA */       0x00, 0x06,
        /* IN */        0x00, 0x01
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

/* ================================================================
 * dns_packet_append_opt()
 * ================================================================ */

TEST(packet_append_opt_basic) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));

        ASSERT_OK(dns_packet_append_opt(packet, 512, false, false, NULL, 0, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x02, 0x00,
        /* rcode */     0x00,
        /* version */   0x00,
        /* flags */     0x00, 0x00,
        /* rdata */     0x00, 0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_opt_change_max_udp) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));

        ASSERT_OK(dns_packet_append_opt(packet, 4100, false, false, NULL, 0, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x10, 0x04,
        /* rcode */     0x00,
        /* version */   0x00,
        /* flags */     0x00, 0x00,
        /* rdata */     0x00, 0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_opt_dnssec_ok) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));

        ASSERT_OK(dns_packet_append_opt(packet, 512, true, false, NULL, 0, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x02, 0x00,
        /* rcode */     0x00,
        /* version */   0x00,
        /* flags */     0x80, 0x00,
        /* rdata */     0x00, 0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_opt_rcode) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));

        ASSERT_OK(dns_packet_append_opt(packet, 512, false, false, NULL, 0x97a, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x02, 0x00,
        /* rcode */     0x97,
        /* version */   0x00,
        /* flags */     0x00, 0x00,
        /* rdata */     0x00, 0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_opt_nsid) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));

        ASSERT_OK(dns_packet_append_opt(packet, 512, false, false, "nsid.example.com", 0, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x02, 0x00,
        /* rcode */     0x00,
        /* version */   0x00,
        /* flags */     0x00, 0x00,
        /* rdata */     0x00, 0x14,
                        0x00, 0x03,
                        0x00, 0x10,
                        'n', 's', 'i', 'd', '.', 'e', 'x', 'a',
                        'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_key_and_opt) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->qdcount = htobe16(1);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        ASSERT_OK(dns_packet_append_key(packet, key, 0, NULL));

        ASSERT_OK(dns_packet_append_opt(packet, 512, false, false, NULL, 0, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x01,     0x00, 0x00,     0x00, 0x00,     0x00, 0x01,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01,

        /* root */      0x00,
        /* OPT */       0x00, 0x29,
        /* udp max */   0x02, 0x00,
        /* rcode */     0x00,
        /* version */   0x00,
        /* flags */     0x00, 0x00,
        /* rdata */     0x00, 0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_truncate_opt) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_resource_key_unrefp) DnsResourceKey *key = NULL;

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(0, 0, 0, 0, 1, 0, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->qdcount = htobe16(1);

        key = dns_resource_key_new(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        ASSERT_OK(dns_packet_append_key(packet, key, 0, NULL));

        ASSERT_OK(dns_packet_append_opt(packet, 512, false, false, NULL, 0, NULL));

        ASSERT_TRUE(dns_packet_truncate_opt(packet));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_RD, DNS_RCODE_SUCCESS,
                        0x00, 0x01,     0x00, 0x00,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

/* ================================================================
 * dns_packet_append_answer()
 * ================================================================ */

TEST(packet_append_answer_single_a) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "example.com");
        rr->ttl = 3601;
        rr->a.in_addr.s_addr = htobe32(0xc0a8017f);

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x04,
        /* ip */        0xc0, 0xa8, 0x01, 0x7f
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_ns) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_NS, "example.com");
        rr->ttl = 3601;
        rr->ns.name = strdup("ns1.example.com");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* NS */        0x00, 0x02,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x06,
        /* name */      0x03, 'n', 's', '1',
                        0xc0, 0x0c
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_cname) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_CNAME, "www.example.com");
        rr->ttl = 3601;
        rr->cname.name = strdup("example.com");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, 'w', 'w', 'w',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* CNAME */     0x00, 0x05,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x02,
        /* name */      0xc0, 0x10
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_hinfo) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_HINFO, "example.com");
        rr->ttl = 3601;
        rr->hinfo.cpu = strdup("x64");
        rr->hinfo.os = strdup("GNU/Linux");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* HINFO */     0x00, 0x0d,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x0e,
        /* cpu */       0x03, 'x', '6', '4',
        /* os */        0x09, 'G', 'N', 'U', '/', 'L', 'i', 'n', 'u', 'x'
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_ptr) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_PTR, "127.1.168.192.in-addr.arpa");
        rr->ttl = 3601;
        rr->ptr.name = strdup("example.com");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, '1', '2', '7',
                        0x01, '1',
                        0x03, '1', '6', '8',
                        0x03, '1', '9', '2',
                        0x07, 'i', 'n', '-', 'a', 'd', 'd', 'r',
                        0x04, 'a', 'r', 'p', 'a',
                        0x00,
        /* PTR */       0x00, 0x0c,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x0d,
        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_mx) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_MX, "example.com");
        rr->ttl = 3601;
        rr->mx.priority = 9;
        rr->mx.exchange = strdup("mail.example.com");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* MX */        0x00, 0x0f,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x09,
        /* priority */  0x00, 0x09,
        /* name */      0x04, 'm', 'a', 'i', 'l',
                        0xc0, 0x0c
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_srv) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SRV, "_ldap._tcp.example.com");
        rr->ttl = 3601;
        rr->srv.priority = 17185;
        rr->srv.weight = 25976;
        rr->srv.port = 389;
        rr->srv.name = strdup("cloud.example.com");

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x05, '_', 'l', 'd', 'a', 'p',
                        0x04, '_', 't', 'c', 'p',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* SRV */       0x00, 0x21,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x19,
        /* priority */  0x43, 0x21,
        /* weight */    0x65, 0x78,
        /* port */      0x01, 0x85,
        /* name */      0x05, 'c', 'l', 'o', 'u', 'd',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_rrsig_with_a) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;

        answer = dns_answer_new(2);

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_A, "www.example.com");
        rr->ttl = 3601;
        rr->a.in_addr.s_addr = htobe32(0xc0a8017f);

        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_RRSIG, "www.example.com");
        rr->ttl = 3601;
        rr->rrsig.type_covered = DNS_TYPE_A;
        rr->rrsig.algorithm = DNSSEC_ALGORITHM_ECC;
        rr->rrsig.labels = 3;
        rr->rrsig.original_ttl = 3600;
        rr->rrsig.expiration = 1720361303;
        rr->rrsig.inception = 1717769303;
        rr->rrsig.key_tag = 0x1234;
        rr->rrsig.signer = strdup("example.com");

        const uint8_t signature[] = {
                0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
        };
        rr->rrsig.signature_size = sizeof(signature);
        rr->rrsig.signature = memdup(signature, rr->rrsig.signature_size);

        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x02,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x03, 'w', 'w', 'w',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* A */         0x00, 0x01,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x04,
        /* ip */        0xc0, 0xa8, 0x01, 0x7f,

        /* name */      0xc0, 0x0c,
        /* RRSIG */     0x00, 0x2e,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x27,
        /* type */      0x00, 0x01,
        /* algo */      0x04,
        /* labels */    0x03,
        /* orig ttl */  0x00, 0x00, 0x0e, 0x10,
        /* expiry */    0x66, 0x8a, 0xa1, 0x57,
        /* inception */ 0x66, 0x63, 0x14, 0x57,
        /* key tag */   0x12, 0x34,
        /* signer */    0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* signature */ 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

TEST(packet_append_answer_single_svcb) {
        _cleanup_(dns_packet_unrefp) DnsPacket *packet = NULL;
        _cleanup_(dns_answer_unrefp) DnsAnswer *answer = NULL;
        DnsResourceRecord *rr = NULL;
        DnsSvcParam *param = NULL;

        rr = dns_resource_record_new_full(DNS_CLASS_IN, DNS_TYPE_SVCB, "_443._wss.example.com");
        rr->ttl = 3601;
        rr->svcb.priority = 9;
        rr->svcb.target_name = strdup("sock.example.com");

        param = calloc(1, offsetof(DnsSvcParam, value) + 4);
        param->key = DNS_SVC_PARAM_KEY_MANDATORY;
        param->length = 4;
        memcpy(param->value, "\x00\x01\x00\x03", 4);
        LIST_APPEND(params, rr->svcb.params, param);

        param = calloc(1, offsetof(DnsSvcParam, value) + 10);
        param->key = DNS_SVC_PARAM_KEY_ALPN;
        param->length = 10;
        memcpy(param->value, "\x09websocket", 10);
        LIST_APPEND(params, rr->svcb.params, param);

        param = calloc(1, offsetof(DnsSvcParam, value));
        param->key = DNS_SVC_PARAM_KEY_NO_DEFAULT_ALPN;
        param->length = 0;
        LIST_APPEND(params, rr->svcb.params, param);

        param = calloc(1, offsetof(DnsSvcParam, value) + 2);
        param->key = DNS_SVC_PARAM_KEY_PORT;
        param->length = 2;
        memcpy(param->value, "\x01\xbb", 2);
        LIST_APPEND(params, rr->svcb.params, param);

        param = calloc(1, offsetof(DnsSvcParam, value_in_addr) + 2 * sizeof(struct in_addr));
        param->key = DNS_SVC_PARAM_KEY_IPV4HINT;
        param->length = 2 * sizeof(struct in_addr);
        param->value_in_addr[0].s_addr = htobe32(0x7284fd3a);
        param->value_in_addr[1].s_addr = htobe32(0x48bcc7c0);
        LIST_APPEND(params, rr->svcb.params, param);

        param = calloc(1, offsetof(DnsSvcParam, value_in6_addr) + sizeof(struct in6_addr));
        param->key = DNS_SVC_PARAM_KEY_IPV6HINT;
        param->length = sizeof(struct in6_addr);
        param->value_in6_addr[0] = (struct in6_addr) { .s6_addr = { 0xf2, 0x34, 0x32, 0x2e, 0xb8, 0x25, 0x38, 0x35, 0x2f, 0xd7, 0xdb, 0x7b, 0x28, 0x7e, 0x60, 0xbb } };
        LIST_APPEND(params, rr->svcb.params, param);

        answer = dns_answer_new(1);
        dns_answer_add(answer, rr, 1, 0, NULL);
        dns_resource_record_unref(rr);

        ASSERT_OK(dns_packet_new(&packet, DNS_PROTOCOL_DNS, 0, DNS_PACKET_SIZE_MAX));

        DNS_PACKET_ID(packet) = htobe16(42);
        DNS_PACKET_HEADER(packet)->flags = htobe16(DNS_PACKET_MAKE_FLAGS(1, 0, 1, 0, 1, 1, 0, 0, DNS_RCODE_SUCCESS));
        DNS_PACKET_HEADER(packet)->ancount = htobe16(dns_answer_size(answer));

        ASSERT_OK(dns_packet_append_answer(packet, answer, NULL));

        const uint8_t data[] = {
                        0x00, 0x2a,     BIT_QR | BIT_AA | BIT_RD, BIT_RA | DNS_RCODE_SUCCESS,
                        0x00, 0x00,     0x00, 0x01,     0x00, 0x00,     0x00, 0x00,

        /* name */      0x04, '_', '4', '4', '3',
                        0x04, '_', 'w', 's', 's',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* SVCB */      0x00, 0x40,
        /* IN */        0x00, 0x01,
        /* ttl */       0x00, 0x00, 0x0e, 0x11,
        /* rdata */     0x00, 0x54,
        /* priority */  0x00, 0x09,
        /* target */    0x04, 's', 'o', 'c', 'k',
                        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
                        0x03, 'c', 'o', 'm',
                        0x00,
        /* mandatory */ 0x00, 0x00,
                        0x00, 0x04,
                        0x00, 0x01, 0x00, 0x03,
        /* alpn */      0x00, 0x01,
                        0x00, 0x0a,
                        0x09, 'w', 'e', 'b', 's', 'o', 'c', 'k', 'e', 't',
        /* no-deflt */  0x00, 0x02,
                        0x00, 0x00,
        /* port */      0x00, 0x03,
                        0x00, 0x02,
                        0x01, 0xbb,
        /* ipv4hint */  0x00, 0x04,
                        0x00, 0x08,
                        0x72, 0x84, 0xfd, 0x3a,
                        0x48, 0xbc, 0xc7, 0xc0,
        /* ipv6hint */  0x00, 0x06,
                        0x00, 0x10,
                        0xf2, 0x34, 0x32, 0x2e, 0xb8, 0x25, 0x38, 0x35,
                        0x2f, 0xd7, 0xdb, 0x7b, 0x28, 0x7e, 0x60, 0xbb
        };

        ASSERT_EQ(packet->size, sizeof(data));
        ASSERT_EQ(memcmp(DNS_PACKET_DATA(packet), data, sizeof(data)), 0);
}

DEFINE_TEST_MAIN(LOG_DEBUG)

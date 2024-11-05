//
// Copyright 2024 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "nng/nng.h"
#include <nuts.h>
#include <stdlib.h>

// TCP tests.

static void
test_udp_wild_card_connect_fail(void)
{
	nng_socket s;
	char       addr[NNG_MAXADDRLEN];

	NUTS_OPEN(s);
	(void) snprintf(addr, sizeof(addr), "udp://*:%u", nuts_next_port());
	NUTS_FAIL(nng_dial(s, addr, NULL, 0), NNG_EADDRINVAL);
	NUTS_CLOSE(s);
}

void
test_udp_wild_card_bind(void)
{
	nng_socket s1;
	nng_socket s2;
	char       addr[NNG_MAXADDRLEN];
	uint16_t   port;

	port = nuts_next_port();

	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	(void) snprintf(addr, sizeof(addr), "udp4://*:%u", port);
	NUTS_PASS(nng_listen(s1, addr, NULL, 0));
	nng_msleep(500);
	(void) snprintf(addr, sizeof(addr), "udp://127.0.0.1:%u", port);
	NUTS_PASS(nng_dial(s2, addr, NULL, 0));
	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
}

void
test_udp_local_address_connect(void)
{

	nng_socket s1;
	nng_socket s2;
	char       addr[NNG_MAXADDRLEN];
	uint16_t   port;

	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	port = nuts_next_port();
	(void) snprintf(addr, sizeof(addr), "udp://127.0.0.1:%u", port);
	NUTS_PASS(nng_listen(s1, addr, NULL, 0));
	(void) snprintf(addr, sizeof(addr), "udp://127.0.0.1:%u", port);
	NUTS_PASS(nng_dial(s2, addr, NULL, 0));
	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
}

void
test_udp_port_zero_bind(void)
{
	nng_socket   s1;
	nng_socket   s2;
	nng_sockaddr sa;
	nng_listener l;
	char        *addr;
	int          port;

	NUTS_OPEN(s1);
	NUTS_OPEN(s2);
	NUTS_PASS(nng_listen(s1, "udp://127.0.0.1:0", &l, 0));
	nng_msleep(100);
	NUTS_PASS(nng_listener_get_string(l, NNG_OPT_URL, &addr));
	NUTS_TRUE(memcmp(addr, "udp://", 6) == 0);
	NUTS_PASS(nng_listener_get_addr(l, NNG_OPT_LOCADDR, &sa));
	NUTS_TRUE(sa.s_in.sa_family == NNG_AF_INET);
	NUTS_TRUE(sa.s_in.sa_port != 0);
	NUTS_TRUE(sa.s_in.sa_addr == nuts_be32(0x7f000001));
	NUTS_PASS(nng_dial(s2, addr, NULL, 0));
	NUTS_PASS(nng_listener_get_int(l, NNG_OPT_TCP_BOUND_PORT, &port));
	NUTS_TRUE(port == nuts_be16(sa.s_in.sa_port));
	nng_strfree(addr);

	NUTS_CLOSE(s2);
	NUTS_CLOSE(s1);
}

void
test_udp_non_local_address(void)
{
	nng_socket s1;

	NUTS_OPEN(s1);
	NUTS_FAIL(nng_listen(s1, "udp://8.8.8.8", NULL, 0), NNG_EADDRINVAL);
	NUTS_CLOSE(s1);
}

void
test_udp_malformed_address(void)
{
	nng_socket s1;

	NUTS_OPEN(s1);
	NUTS_FAIL(nng_dial(s1, "udp://127.0.0.1", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(nng_dial(s1, "udp://127.0.0.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(nng_dial(s1, "udp://127.0.x.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_listen(s1, "udp://127.0.0.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_FAIL(
	    nng_listen(s1, "udp://127.0.x.1.32", NULL, 0), NNG_EADDRINVAL);
	NUTS_CLOSE(s1);
}

void
test_udp_recv_max(void)
{
	char         msg[256];
	char         buf[256];
	nng_socket   s0;
	nng_socket   s1;
	nng_listener l;
	size_t       sz;
	char        *addr;

	NUTS_ADDR(addr, "udp");

	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_size(s0, NNG_OPT_RECVMAXSZ, 200));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_socket_get_size(s0, NNG_OPT_RECVMAXSZ, &sz));
	NUTS_TRUE(sz == 200);
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_RECVMAXSZ, 100));
	NUTS_PASS(nng_listener_get_size(l, NNG_OPT_RECVMAXSZ, &sz));
	NUTS_TRUE(sz == 100);
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_dial(s1, addr, NULL, 0));
	nng_msleep(1000);
	NUTS_PASS(nng_send(s1, msg, 95, 0));
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_recv(s0, buf, &sz, 0));
	NUTS_TRUE(sz == 95);
	NUTS_PASS(nng_send(s1, msg, 150, 0));
	NUTS_FAIL(nng_recv(s0, buf, &sz, 0), NNG_ETIMEDOUT);
	NUTS_PASS(nng_close(s0));
	NUTS_CLOSE(s1);
}

void
test_udp_recv_copy(void)
{
	char         msg[256];
	char         buf[256];
	nng_socket   s0;
	nng_socket   s1;
	nng_listener l;
	size_t       sz;
	char        *addr;

	NUTS_ADDR(addr, "udp");

	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_listener_get_size(l, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_TRUE(sz == 100);
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_dial(s1, addr, NULL, 0));
	nng_msleep(100);
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_send(s1, msg, 95, 0));
	NUTS_PASS(nng_recv(s0, buf, &sz, 0));
	NUTS_TRUE(sz == 95);
	NUTS_PASS(nng_send(s1, msg, 150, 0));
	NUTS_PASS(nng_recv(s0, buf, &sz, 0));
	NUTS_TRUE(sz == 150);
	NUTS_CLOSE(s0);
	NUTS_CLOSE(s1);
}

void
test_udp_multi_send_recv(void)
{
	char         msg[256];
	char         buf[256];
	nng_socket   s0;
	nng_socket   s1;
	nng_listener l;
	nng_dialer   d;
	size_t       sz;
	char        *addr;

	NUTS_ADDR(addr, "udp");

	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_listener_get_size(l, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_TRUE(sz == 100);
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_dialer_create(&d, s1, addr));
	NUTS_PASS(nng_dialer_set_size(d, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_dialer_get_size(d, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_PASS(nng_dialer_start(d, 0));
	nng_msleep(100);

	for (int i = 0; i < 1000; i++) {
		NUTS_PASS(nng_send(s1, msg, 95, 0));
		NUTS_PASS(nng_recv(s0, buf, &sz, 0));
		NUTS_TRUE(sz == 95);
		NUTS_PASS(nng_send(s0, msg, 95, 0));
		NUTS_PASS(nng_recv(s1, buf, &sz, 0));
		NUTS_TRUE(sz == 95);
	}
	NUTS_CLOSE(s0);
	NUTS_CLOSE(s1);
}

typedef struct {
	nng_aio    *aio;
	int         pass;
	int         fail;
	const char *expect;
	size_t      len;
	nng_socket  sock;
} udp_recv_count;

void
udp_recv_count_cb(void *arg)
{
	udp_recv_count *c = arg;
	nng_msg        *m;
	int             rv;

	rv = nng_aio_result(c->aio);
	switch (rv) {
	case NNG_ECLOSED:
	case NNG_ECANCELED:
		c->fail++;
		return;
	case NNG_ETIMEDOUT:
		c->fail++;
		break;
	case 0:
		m = nng_aio_get_msg(c->aio);
		nng_aio_set_msg(c->aio, NULL);
		if (nng_msg_len(m) != c->len) {
			c->fail++;
		} else {
			c->pass++;
		}
		nng_msg_free(m);
		break;
	default:
		c->fail++;
		nng_log_warn(
		    NULL, "Unexpected recv error %s", nng_strerror(rv));
		break;
	}

	nng_aio_set_timeout(c->aio, 1000);
	nng_recv_aio(c->sock, c->aio);
}

// This test uses callbacks above to ensure we
// receive as quickly as possible, reducing the likelihood
// of dropped messages.
void
test_udp_multi_small_burst(void)
{
	char           msg[256];
	nng_socket     s0;
	nng_socket     s1;
	nng_listener   l;
	nng_dialer     d;
	size_t         sz;
	char          *addr;
	udp_recv_count rc = { 0 };

	NUTS_ENABLE_LOG(NNG_LOG_NOTICE);
	NUTS_ADDR(addr, "udp");

	NUTS_PASS(nng_aio_alloc(&rc.aio, udp_recv_count_cb, &rc));

	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 10));
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_SENDTIMEO, 1000));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_listener_get_size(l, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_TRUE(sz == 100);
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_RECVTIMEO, 10));
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 1000));
	NUTS_PASS(nng_dialer_create(&d, s1, addr));
	NUTS_PASS(nng_dialer_set_size(d, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_dialer_get_size(d, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_PASS(nng_dialer_start(d, 0));
	nng_msleep(100);

	float actual  = 0;
	float expect  = 0;
	float require = 0.50;
	int   burst   = 20;
	int   count   = 40;
#if defined(NNG_PLATFORM_WINDOWS)
	// Windows seems to drop a lot - maybe because of virtualization
	burst   = 2;
	count   = 10;
	require = 0.10;
#endif

	const char *pass_rate = getenv("NNG_UDP_PASS_RATE");
	if (pass_rate != NULL && strlen(pass_rate) > 0) {
		require = (atoi(pass_rate) * 1.0) / 100.0;
		nng_log_notice(
		    "UDP", "required pass rate changed to %0.02f", require);
	} else {
		nng_log_notice("UDP", "required pass rate %0.02f", require);
		nng_log_notice(NULL,
		    "To change pass rate, set $NNG_UDP_PASS_RATE to "
		    "percentage of packets that must be received.");
	}

#if defined(NNG_SANITIZER) || defined(NNG_COVERAGE)
	// sanitizers may drop a lot, so can coverage
	require = 0.0;
#endif

	rc.sock   = s0;
	rc.len    = 95;
	rc.expect = msg;

	nng_recv_aio(rc.sock, rc.aio);

	// Experimentally at least on Darwin, we see some packet losses
	// even for loopback.  Loss rates appear depressingly high.
	for (int i = 0; i < count; i++) {
		for (int j = 0; j < burst; j++) {
			(void) nng_send(s1, msg, 95, 0);
			expect++;
		}
		nng_msleep(20);
	}
	nng_msleep(10);
	nng_aio_stop(rc.aio);
	nng_aio_free(rc.aio);
	actual = rc.pass;
	NUTS_TRUE(actual <= expect);
	NUTS_TRUE(actual / expect > require);
	nng_log_notice("UDP", "Packet loss: %.02f (got %.f of %.f)",
	    1.0 - actual / expect, actual, expect);
	NUTS_CLOSE(s0);
	NUTS_CLOSE(s1);
}

void
test_udp_stats(void)
{
	char         msg[256];
	char         buf[256];
	nng_socket   s0;
	nng_socket   s1;
	nng_listener l;
	nng_dialer   d;
	size_t       sz;
	char        *addr;
	nng_stat    *stat;

	NUTS_ADDR(addr, "udp");

	NUTS_OPEN(s0);
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_ms(s0, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_listener_create(&l, s0, addr));
	NUTS_PASS(nng_listener_set_size(l, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_listener_get_size(l, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_TRUE(sz == 100);
	NUTS_PASS(nng_listener_start(l, 0));

	NUTS_OPEN(s1);
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_RECVTIMEO, 100));
	NUTS_PASS(nng_socket_set_ms(s1, NNG_OPT_SENDTIMEO, 100));
	NUTS_PASS(nng_dialer_create(&d, s1, addr));
	NUTS_PASS(nng_dialer_set_size(d, NNG_OPT_UDP_COPY_MAX, 100));
	NUTS_PASS(nng_dialer_get_size(d, NNG_OPT_UDP_COPY_MAX, &sz));
	NUTS_PASS(nng_dialer_start(d, 0));
	nng_msleep(100);

	for (int i = 0; i < 50; i++) {
		NUTS_PASS(nng_send(s1, msg, 95, 0));
		NUTS_PASS(nng_recv(s0, buf, &sz, 0));
		NUTS_TRUE(sz == 95);
		NUTS_PASS(nng_send(s0, msg, 95, 0));
		NUTS_PASS(nng_recv(s1, buf, &sz, 0));
		NUTS_TRUE(sz == 95);
	}
	NUTS_PASS(nng_stats_get(&stat));
	nng_stats_dump(stat);
	nng_stats_free(stat);

	NUTS_CLOSE(s0);
	NUTS_CLOSE(s1);
}

NUTS_TESTS = {

	{ "udp wild card connect fail", test_udp_wild_card_connect_fail },
	{ "udp wild card bind", test_udp_wild_card_bind },
	{ "udp port zero bind", test_udp_port_zero_bind },
	{ "udp local address connect", test_udp_local_address_connect },
	{ "udp non-local address", test_udp_non_local_address },
	{ "udp malformed address", test_udp_malformed_address },
	{ "udp recv max", test_udp_recv_max },
	{ "udp recv copy", test_udp_recv_copy },
	{ "udp multi send recv", test_udp_multi_send_recv },
	{ "udp multi small burst", test_udp_multi_small_burst },
	{ "udp stats", test_udp_stats },
	{ NULL, NULL },
};

/*
 * We use a very basic strategy to use TCP_CORK semantics optimally
 * in most TCP servers:  On corked sockets, we will uncork on recv()
 * if there was a previous send().  Otherwise we do not fiddle
 * with TCP_CORK at all.
 *
 * Under Linux, we can rely on TCP_CORK being inherited in an
 * accept()-ed client socket so we can avoid syscalls for each
 * accept()-ed client if we know the accept() socket corks.
 *
 * This module does NOTHING for client TCP sockets, we only deal
 * with accept()-ed sockets right now.
 */

#include "kgio.h"

enum nopush_state {
	NOPUSH_STATE_IGNORE = -1,
	NOPUSH_STATE_WRITER = 0,
	NOPUSH_STATE_WRITTEN = 1,
	NOPUSH_STATE_ACCEPTOR = 2
};

struct nopush_socket {
	VALUE io;
	enum nopush_state state;
};

static int enabled;
static long capa;
static struct nopush_socket *active;

static void set_acceptor_state(struct nopush_socket *nps, int fd);
static void flush_pending_data(int fd);

static void grow(int fd)
{
	long new_capa = fd + 64;
	size_t size;

	assert(new_capa > capa && "grow()-ing for low fd");
	size = new_capa * sizeof(struct nopush_socket);
	active = xrealloc(active, size);

	while (capa < new_capa) {
		struct nopush_socket *nps = &active[capa++];

		nps->io = Qnil;
		nps->state = NOPUSH_STATE_IGNORE;
	}
}

static VALUE s_get_nopush_smart(VALUE self)
{
	return enabled ? Qtrue : Qfalse;
}

static VALUE s_set_nopush_smart(VALUE self, VALUE val)
{
	enabled = RTEST(val);

	return val;
}

void init_kgio_nopush(void)
{
	VALUE m = rb_define_module("Kgio");

	rb_define_singleton_method(m, "nopush_smart?", s_get_nopush_smart, 0);
	rb_define_singleton_method(m, "nopush_smart=", s_set_nopush_smart, 1);
}

/*
 * called after a successful write, just mark that we've put something
 * in the skb and will need to uncork on the next write.
 */
void kgio_nopush_send(VALUE io, int fd)
{
	struct nopush_socket *nps;

	if (fd >= capa) return;
	nps = &active[fd];
	if (nps->io == io && nps->state == NOPUSH_STATE_WRITER)
		nps->state = NOPUSH_STATE_WRITTEN;
}

/* called on successful accept() */
void kgio_nopush_accept(VALUE accept_io, VALUE io, int accept_fd, int fd)
{
	struct nopush_socket *accept_nps, *client_nps;

	if (!enabled)
		return;
	assert(fd >= 0 && "client_fd negative");
	assert(accept_fd >= 0 && "accept_fd negative");
	if (fd >= capa || accept_fd >= capa)
		grow(fd > accept_fd ? fd : accept_fd);

	accept_nps = &active[accept_fd];

	if (accept_nps->io != accept_io) {
		accept_nps->io = accept_io;
		set_acceptor_state(accept_nps, fd);
	}
	client_nps = &active[fd];
	client_nps->io = io;
	if (accept_nps->state == NOPUSH_STATE_ACCEPTOR)
		client_nps->state = NOPUSH_STATE_WRITER;
	else
		client_nps->state = NOPUSH_STATE_IGNORE;
}

void kgio_nopush_recv(VALUE io, int fd)
{
	struct nopush_socket *nps;

	if (fd >= capa)
		return;

	nps = &active[fd];
	if (nps->io != io || nps->state != NOPUSH_STATE_WRITTEN)
		return;

	/* reset internal state and flush corked buffers */
	nps->state = NOPUSH_STATE_WRITER;
	if (enabled)
		flush_pending_data(fd);
}

#ifdef __linux__
#include <netinet/tcp.h>
static void set_acceptor_state(struct nopush_socket *nps, int fd)
{
	int corked = 0;
	socklen_t optlen = sizeof(int);

	if (getsockopt(fd, SOL_TCP, TCP_CORK, &corked, &optlen) != 0) {
		if (errno != EOPNOTSUPP)
			rb_sys_fail("getsockopt(SOL_TCP, TCP_CORK)");
		errno = 0;
		nps->state = NOPUSH_STATE_IGNORE;
	} else if (corked) {
		nps->state = NOPUSH_STATE_ACCEPTOR;
	} else {
		nps->state = NOPUSH_STATE_IGNORE;
	}
}

/*
 * checks to see if we've written anything since the last recv()
 * If we have, uncork the socket and immediately recork it.
 */
static void flush_pending_data(int fd)
{
	int optval = 0;
	socklen_t optlen = sizeof(int);

	if (setsockopt(fd, SOL_TCP, TCP_CORK, &optval, optlen) != 0)
		rb_sys_fail("setsockopt(SOL_TCP, TCP_CORK, 0)");
	/* immediately recork */
	optval = 1;
	if (setsockopt(fd, SOL_TCP, TCP_CORK, &optval, optlen) != 0)
		rb_sys_fail("setsockopt(SOL_TCP, TCP_CORK, 1)");
}
/* TODO: add FreeBSD support */

#endif /* linux */

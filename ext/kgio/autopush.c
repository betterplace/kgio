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

enum autopush_state {
	AUTOPUSH_STATE_IGNORE = -1,
	AUTOPUSH_STATE_WRITER = 0,
	AUTOPUSH_STATE_WRITTEN = 1,
	AUTOPUSH_STATE_ACCEPTOR = 2
};

struct autopush_socket {
	VALUE io;
	enum autopush_state state;
};

static int enabled;
static long capa;
static struct autopush_socket *active;

static void set_acceptor_state(struct autopush_socket *aps, int fd);
static void flush_pending_data(int fd);

static void grow(int fd)
{
	long new_capa = fd + 64;
	size_t size;

	assert(new_capa > capa && "grow()-ing for low fd");
	size = new_capa * sizeof(struct autopush_socket);
	active = xrealloc(active, size);

	while (capa < new_capa) {
		struct autopush_socket *aps = &active[capa++];

		aps->io = Qnil;
		aps->state = AUTOPUSH_STATE_IGNORE;
	}
}

static VALUE s_get_autopush(VALUE self)
{
	return enabled ? Qtrue : Qfalse;
}

static VALUE s_set_autopush(VALUE self, VALUE val)
{
	enabled = RTEST(val);

	return val;
}

void init_kgio_autopush(void)
{
	VALUE m = rb_define_module("Kgio");

	rb_define_singleton_method(m, "autopush?", s_get_autopush, 0);
	rb_define_singleton_method(m, "autopush=", s_set_autopush, 1);
}

/*
 * called after a successful write, just mark that we've put something
 * in the skb and will need to uncork on the next write.
 */
void kgio_autopush_send(VALUE io, int fd)
{
	struct autopush_socket *aps;

	if (fd >= capa) return;
	aps = &active[fd];
	if (aps->io == io && aps->state == AUTOPUSH_STATE_WRITER)
		aps->state = AUTOPUSH_STATE_WRITTEN;
}

/* called on successful accept() */
void kgio_autopush_accept(VALUE accept_io, VALUE io, int accept_fd, int fd)
{
	struct autopush_socket *accept_aps, *client_aps;

	if (!enabled)
		return;
	assert(fd >= 0 && "client_fd negative");
	assert(accept_fd >= 0 && "accept_fd negative");
	if (fd >= capa || accept_fd >= capa)
		grow(fd > accept_fd ? fd : accept_fd);

	accept_aps = &active[accept_fd];

	if (accept_aps->io != accept_io) {
		accept_aps->io = accept_io;
		set_acceptor_state(accept_aps, fd);
	}
	client_aps = &active[fd];
	client_aps->io = io;
	if (accept_aps->state == AUTOPUSH_STATE_ACCEPTOR)
		client_aps->state = AUTOPUSH_STATE_WRITER;
	else
		client_aps->state = AUTOPUSH_STATE_IGNORE;
}

void kgio_autopush_recv(VALUE io, int fd)
{
	struct autopush_socket *aps;

	if (fd >= capa)
		return;

	aps = &active[fd];
	if (aps->io != io || aps->state != AUTOPUSH_STATE_WRITTEN)
		return;

	/* reset internal state and flush corked buffers */
	aps->state = AUTOPUSH_STATE_WRITER;
	if (enabled)
		flush_pending_data(fd);
}

#ifdef __linux__
#include <netinet/tcp.h>
static void set_acceptor_state(struct autopush_socket *aps, int fd)
{
	int corked = 0;
	socklen_t optlen = sizeof(int);

	if (getsockopt(fd, SOL_TCP, TCP_CORK, &corked, &optlen) != 0) {
		if (errno != EOPNOTSUPP)
			rb_sys_fail("getsockopt(SOL_TCP, TCP_CORK)");
		errno = 0;
		aps->state = AUTOPUSH_STATE_IGNORE;
	} else if (corked) {
		aps->state = AUTOPUSH_STATE_ACCEPTOR;
	} else {
		aps->state = AUTOPUSH_STATE_IGNORE;
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

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
#include <netinet/tcp.h>

/*
 * As of FreeBSD 4.5, TCP_NOPUSH == TCP_CORK
 * ref: http://dotat.at/writing/nopush.html
 * We won't care for older FreeBSD since nobody runs Ruby on them...
 */
#ifdef TCP_CORK
#  define KGIO_NOPUSH TCP_CORK
#elif defined(TCP_NOPUSH)
#  define KGIO_NOPUSH TCP_NOPUSH
#endif

#ifdef KGIO_NOPUSH
static ID id_autopush_state;
static int enabled;

enum autopush_state {
	AUTOPUSH_STATE_ACCEPTOR_IGNORE = -1,
	AUTOPUSH_STATE_IGNORE = 0,
	AUTOPUSH_STATE_WRITER = 1,
	AUTOPUSH_STATE_WRITTEN = 2,
	AUTOPUSH_STATE_ACCEPTOR = 3
};

static enum autopush_state state_get(VALUE io)
{
	VALUE val;

	if (rb_ivar_defined(io, id_autopush_state) == Qfalse)
		return AUTOPUSH_STATE_IGNORE;
	val = rb_ivar_get(io, id_autopush_state);

	return (enum autopush_state)NUM2INT(val);
}

static void state_set(VALUE io, enum autopush_state state)
{
	rb_ivar_set(io, id_autopush_state, INT2NUM(state));
}

static enum autopush_state detect_acceptor_state(VALUE io);
static void push_pending_data(VALUE io);

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
	id_autopush_state = rb_intern("@kgio_autopush_state");
}

/*
 * called after a successful write, just mark that we've put something
 * in the skb and will need to uncork on the next write.
 */
void kgio_autopush_send(VALUE io)
{
	if (state_get(io) == AUTOPUSH_STATE_WRITER)
		state_set(io, AUTOPUSH_STATE_WRITTEN);
}

/* called on successful accept() */
void kgio_autopush_accept(VALUE accept_io, VALUE client_io)
{
	enum autopush_state acceptor_state;

	if (!enabled)
		return;
	acceptor_state = state_get(accept_io);
	if (acceptor_state == AUTOPUSH_STATE_IGNORE)
		acceptor_state = detect_acceptor_state(accept_io);
	if (acceptor_state == AUTOPUSH_STATE_ACCEPTOR)
		state_set(client_io, AUTOPUSH_STATE_WRITER);
	else
		state_set(client_io, AUTOPUSH_STATE_IGNORE);
}

void kgio_autopush_recv(VALUE io)
{
	if (enabled && (state_get(io) == AUTOPUSH_STATE_WRITTEN)) {
		push_pending_data(io);
		state_set(io, AUTOPUSH_STATE_WRITER);
	}
}

static enum autopush_state detect_acceptor_state(VALUE io)
{
	int corked = 0;
	int fd = my_fileno(io);
	socklen_t optlen = sizeof(int);
	enum autopush_state state;

	if (getsockopt(fd, IPPROTO_TCP, KGIO_NOPUSH, &corked, &optlen) != 0) {
		if (errno != EOPNOTSUPP)
			rb_sys_fail("getsockopt(TCP_CORK/TCP_NOPUSH)");
		errno = 0;
		state = AUTOPUSH_STATE_ACCEPTOR_IGNORE;
	} else if (corked) {
		state = AUTOPUSH_STATE_ACCEPTOR;
	} else {
		state = AUTOPUSH_STATE_ACCEPTOR_IGNORE;
	}
	state_set(io, state);

	return state;
}

/*
 * checks to see if we've written anything since the last recv()
 * If we have, uncork the socket and immediately recork it.
 */
static void push_pending_data(VALUE io)
{
	int optval = 0;
	const socklen_t optlen = sizeof(int);
	const int fd = my_fileno(io);

	if (setsockopt(fd, IPPROTO_TCP, KGIO_NOPUSH, &optval, optlen) != 0)
		rb_sys_fail("setsockopt(TCP_CORK/TCP_NOPUSH, 0)");
	/* immediately recork */
	optval = 1;
	if (setsockopt(fd, IPPROTO_TCP, KGIO_NOPUSH, &optval, optlen) != 0)
		rb_sys_fail("setsockopt(TCP_CORK, 1)");
}
#else /* !KGIO_NOPUSH */
void init_kgio_autopush(void)
{
}
#endif /* ! KGIO_NOPUSH */

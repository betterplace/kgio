#include "kgio.h"
#include "missing_accept4.h"
#include "sock_for_fd.h"

static VALUE localhost;
static VALUE cClientSocket;
static VALUE cKgio_Socket;
static VALUE mSocketMethods;
static VALUE iv_kgio_addr;

#if defined(__linux__)
static int accept4_flags = SOCK_CLOEXEC;
#else /* ! linux */
static int accept4_flags = SOCK_CLOEXEC | SOCK_NONBLOCK;
#endif /* ! linux */

struct accept_args {
	int fd;
	struct sockaddr *addr;
	socklen_t *addrlen;
};

/*
 * Sets the default class for newly accepted sockets.  This is
 * legacy behavior, kgio_accept and kgio_tryaccept now take optional
 * class arguments to override this value.
 */
static VALUE set_accepted(VALUE klass, VALUE aclass)
{
	VALUE tmp;

	if (NIL_P(aclass))
		aclass = cKgio_Socket;

	tmp = rb_funcall(aclass, rb_intern("included_modules"), 0, 0);
	tmp = rb_funcall(tmp, rb_intern("include?"), 1, mSocketMethods);

	if (tmp != Qtrue)
		rb_raise(rb_eTypeError,
		         "class must include Kgio::SocketMethods");

	cClientSocket = aclass;

	return aclass;
}

/*
 * Returns the default class for newly accepted sockets when kgio_accept
 * or kgio_tryaccept are not passed arguments
 */
static VALUE get_accepted(VALUE klass)
{
	return cClientSocket;
}

static VALUE xaccept(void *ptr)
{
	struct accept_args *a = ptr;
	int rv;

	rv = accept_fn(a->fd, a->addr, a->addrlen, accept4_flags);
	if (rv == -1 && errno == ENOSYS && accept_fn != my_accept4) {
		accept_fn = my_accept4;
		rv = accept_fn(a->fd, a->addr, a->addrlen, accept4_flags);
	}

	return (VALUE)rv;
}

#ifdef HAVE_RB_THREAD_BLOCKING_REGION
#  include <time.h>
#  include "blocking_io_region.h"
/*
 * Try to use a (real) blocking accept() since that can prevent
 * thundering herds under Linux:
 * http://www.citi.umich.edu/projects/linux-scalability/reports/accept.html
 *
 * So we periodically disable non-blocking, but not too frequently
 * because other processes may set non-blocking (especially during
 * a process upgrade) with Rainbows! concurrency model changes.
 */
static int thread_accept(struct accept_args *a, int force_nonblock)
{
	if (force_nonblock)
		set_nonblocking(a->fd);
	return (int)rb_thread_io_blocking_region(xaccept, a, a->fd);
}

static void set_blocking_or_block(int fd)
{
	static time_t last_set_blocking;
	time_t now = time(NULL);

	if (last_set_blocking == 0) {
		last_set_blocking = now;
		(void)rb_io_wait_readable(fd);
	} else if ((now - last_set_blocking) <= 5) {
		(void)rb_io_wait_readable(fd);
	} else {
		int flags = fcntl(fd, F_GETFL);
		if (flags == -1)
			rb_sys_fail("fcntl(F_GETFL)");
		if (flags & O_NONBLOCK) {
			flags = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
			if (flags == -1)
				rb_sys_fail("fcntl(F_SETFL)");
		}
		last_set_blocking = now;
	}
}
#else /* ! HAVE_RB_THREAD_BLOCKING_REGION */
#  include <rubysig.h>
static int thread_accept(struct accept_args *a, int force_nonblock)
{
	int rv;

	/* always use non-blocking accept() under 1.8 for green threads */
	set_nonblocking(a->fd);
	TRAP_BEG;
	rv = (int)xaccept(a);
	TRAP_END;
	return rv;
}
#define set_blocking_or_block(fd) (void)rb_io_wait_readable(fd)
#endif /* ! HAVE_RB_THREAD_BLOCKING_REGION */

static VALUE acceptor(int argc, const VALUE *argv)
{
	if (argc == 0)
		return cClientSocket; /* default, legacy behavior */
	else if (argc == 1)
		return argv[0];

	rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
}

#if defined(__linux__)
#  define post_accept kgio_autopush_accept
#else
#  define post_accept(a,b) for(;0;)
#endif

static VALUE
my_accept(VALUE accept_io, VALUE klass,
          struct sockaddr *addr, socklen_t *addrlen, int nonblock)
{
	int client;
	VALUE client_io;
	struct accept_args a;

	a.fd = my_fileno(accept_io);
	a.addr = addr;
	a.addrlen = addrlen;
retry:
	client = thread_accept(&a, nonblock);
	if (client == -1) {
		switch (errno) {
		case EAGAIN:
			if (nonblock)
				return Qnil;
			set_blocking_or_block(a.fd);
#ifdef ECONNABORTED
		case ECONNABORTED:
#endif /* ECONNABORTED */
#ifdef EPROTO
		case EPROTO:
#endif /* EPROTO */
		case EINTR:
			goto retry;
		case ENOMEM:
		case EMFILE:
		case ENFILE:
#ifdef ENOBUFS
		case ENOBUFS:
#endif /* ENOBUFS */
			errno = 0;
			rb_gc();
			client = thread_accept(&a, nonblock);
		}
		if (client == -1) {
			if (errno == EINTR)
				goto retry;
			rb_sys_fail("accept");
		}
	}
	client_io = sock_for_fd(klass, client);
	post_accept(accept_io, client_io);
	return client_io;
}

static VALUE in_addr_set(VALUE io, struct sockaddr_storage *addr, socklen_t len)
{
	VALUE host;
	int host_len, rc;
	char *host_ptr;

	switch (addr->ss_family) {
	case AF_INET:
		host_len = (long)INET_ADDRSTRLEN;
		break;
	case AF_INET6:
		host_len = (long)INET6_ADDRSTRLEN;
		break;
	default:
		rb_raise(rb_eRuntimeError, "unsupported address family");
	}
	host = rb_str_new(NULL, host_len);
	host_ptr = RSTRING_PTR(host);
	rc = getnameinfo((struct sockaddr *)addr, len,
			 host_ptr, host_len, NULL, 0, NI_NUMERICHOST);
	if (rc != 0)
		rb_raise(rb_eRuntimeError, "getnameinfo: %s", gai_strerror(rc));
	rb_str_set_len(host, strlen(host_ptr));
	return rb_ivar_set(io, iv_kgio_addr, host);
}

/*
 * call-seq:
 *
 *	io.kgio_addr! => refreshes the given sock address
 */
static VALUE addr_bang(VALUE io)
{
	int fd = my_fileno(io);
	struct sockaddr_storage addr;
	socklen_t len = sizeof(struct sockaddr_storage);

	if (getpeername(fd, (struct sockaddr *)&addr, &len) != 0)
		rb_sys_fail("getpeername");

	if (addr.ss_family == AF_UNIX)
		return rb_ivar_set(io, iv_kgio_addr, localhost);

	return in_addr_set(io, &addr, len);
}

/*
 * call-seq:
 *
 *	server = Kgio::TCPServer.new('0.0.0.0', 80)
 *	server.kgio_tryaccept -> Kgio::Socket or nil
 *
 * Initiates a non-blocking accept and returns a generic Kgio::Socket
 * object with the kgio_addr attribute set to the IP address of the
 * connected client on success.
 *
 * Returns nil on EAGAIN, and raises on other errors.
 *
 * An optional class argument may be specified to override the
 * Kgio::Socket-class return value:
 *
 *      server.kgio_tryaccept(MySocket) -> MySocket
 */
static VALUE tcp_tryaccept(int argc, VALUE *argv, VALUE io)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	VALUE klass = acceptor(argc, argv);
	VALUE rv = my_accept(io, klass, (struct sockaddr *)&addr, &addrlen, 1);

	if (!NIL_P(rv))
		in_addr_set(rv, &addr, addrlen);
	return rv;
}

/*
 * call-seq:
 *
 *	server = Kgio::TCPServer.new('0.0.0.0', 80)
 *	server.kgio_accept -> Kgio::Socket or nil
 *
 * Initiates a blocking accept and returns a generic Kgio::Socket
 * object with the kgio_addr attribute set to the IP address of
 * the client on success.
 *
 * On Ruby implementations using native threads, this can use a blocking
 * accept(2) (or accept4(2)) system call to avoid thundering herds.
 *
 * An optional class argument may be specified to override the
 * Kgio::Socket-class return value:
 *
 *      server.kgio_accept(MySocket) -> MySocket
 */
static VALUE tcp_accept(int argc, VALUE *argv, VALUE io)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);
	VALUE klass = acceptor(argc, argv);
	VALUE rv = my_accept(io, klass, (struct sockaddr *)&addr, &addrlen, 0);

	in_addr_set(rv, &addr, addrlen);
	return rv;
}

/*
 * call-seq:
 *
 *	server = Kgio::UNIXServer.new("/path/to/unix/socket")
 *	server.kgio_tryaccept -> Kgio::Socket or nil
 *
 * Initiates a non-blocking accept and returns a generic Kgio::Socket
 * object with the kgio_addr attribute set (to the value of
 * Kgio::LOCALHOST) on success.
 *
 * Returns nil on EAGAIN, and raises on other errors.
 *
 * An optional class argument may be specified to override the
 * Kgio::Socket-class return value:
 *
 *      server.kgio_tryaccept(MySocket) -> MySocket
 */
static VALUE unix_tryaccept(int argc, VALUE *argv, VALUE io)
{
	VALUE klass = acceptor(argc, argv);
	VALUE rv = my_accept(io, klass, NULL, NULL, 1);

	if (!NIL_P(rv))
		rb_ivar_set(rv, iv_kgio_addr, localhost);
	return rv;
}

/*
 * call-seq:
 *
 *	server = Kgio::UNIXServer.new("/path/to/unix/socket")
 *	server.kgio_accept -> Kgio::Socket or nil
 *
 * Initiates a blocking accept and returns a generic Kgio::Socket
 * object with the kgio_addr attribute set (to the value of
 * Kgio::LOCALHOST) on success.
 *
 * On Ruby implementations using native threads, this can use a blocking
 * accept(2) (or accept4(2)) system call to avoid thundering herds.
 *
 * An optional class argument may be specified to override the
 * Kgio::Socket-class return value:
 *
 *      server.kgio_accept(MySocket) -> MySocket
 */
static VALUE unix_accept(int argc, VALUE *argv, VALUE io)
{
	VALUE klass = acceptor(argc, argv);
	VALUE rv = my_accept(io, klass, NULL, NULL, 0);

	rb_ivar_set(rv, iv_kgio_addr, localhost);
	return rv;
}

/*
 * call-seq:
 *
 *	Kgio.accept_cloexec? -> true or false
 *
 * Returns true if newly accepted Kgio::Sockets are created with the
 * FD_CLOEXEC file descriptor flag, false if not.
 */
static VALUE get_cloexec(VALUE mod)
{
	return (accept4_flags & SOCK_CLOEXEC) == SOCK_CLOEXEC ? Qtrue : Qfalse;
}

/*
 *
 * call-seq:
 *
 *	Kgio.accept_nonblock? -> true or false
 *
 * Returns true if newly accepted Kgio::Sockets are created with the
 * O_NONBLOCK file status flag, false if not.
 */
static VALUE get_nonblock(VALUE mod)
{
	return (accept4_flags & SOCK_NONBLOCK)==SOCK_NONBLOCK ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *
 *	Kgio.accept_cloexec = true
 *	Kgio.accept_cloexec = false
 *
 * Sets whether or not Kgio::Socket objects created by
 * TCPServer#kgio_accept,
 * TCPServer#kgio_tryaccept,
 * UNIXServer#kgio_accept,
 * and UNIXServer#kgio_tryaccept
 * are created with the FD_CLOEXEC file descriptor flag.
 *
 * This is on by default, as there is little reason to deal to enable
 * it for client sockets on a socket server.
 */
static VALUE set_cloexec(VALUE mod, VALUE boolean)
{
	switch (TYPE(boolean)) {
	case T_TRUE:
		accept4_flags |= SOCK_CLOEXEC;
		return boolean;
	case T_FALSE:
		accept4_flags &= ~SOCK_CLOEXEC;
		return boolean;
	}
	rb_raise(rb_eTypeError, "not true or false");
	return Qnil;
}

/*
 * call-seq:
 *
 *	Kgio.accept_nonblock = true
 *	Kgio.accept_nonblock = false
 *
 * Sets whether or not Kgio::Socket objects created by
 * TCPServer#kgio_accept,
 * TCPServer#kgio_tryaccept,
 * UNIXServer#kgio_accept,
 * and UNIXServer#kgio_tryaccept
 * are created with the O_NONBLOCK file status flag.
 *
 * This defaults to +false+ for GNU/Linux where MSG_DONTWAIT is
 * available (and on newer GNU/Linux, accept4() may also set
 * the non-blocking flag.  This defaults to +true+ on non-GNU/Linux
 * systems.
 */
static VALUE set_nonblock(VALUE mod, VALUE boolean)
{
	switch (TYPE(boolean)) {
	case T_TRUE:
		accept4_flags |= SOCK_NONBLOCK;
		return boolean;
	case T_FALSE:
		accept4_flags &= ~SOCK_NONBLOCK;
		return boolean;
	}
	rb_raise(rb_eTypeError, "not true or false");
	return Qnil;
}

void init_kgio_accept(void)
{
	VALUE cUNIXServer, cTCPServer;
	VALUE mKgio = rb_define_module("Kgio");

	localhost = rb_const_get(mKgio, rb_intern("LOCALHOST"));
	cKgio_Socket = rb_const_get(mKgio, rb_intern("Socket"));
	cClientSocket = cKgio_Socket;
	mSocketMethods = rb_const_get(mKgio, rb_intern("SocketMethods"));

	rb_define_method(mSocketMethods, "kgio_addr!", addr_bang, 0);

	rb_define_singleton_method(mKgio, "accept_cloexec?", get_cloexec, 0);
	rb_define_singleton_method(mKgio, "accept_cloexec=", set_cloexec, 1);
	rb_define_singleton_method(mKgio, "accept_nonblock?", get_nonblock, 0);
	rb_define_singleton_method(mKgio, "accept_nonblock=", set_nonblock, 1);
	rb_define_singleton_method(mKgio, "accept_class=", set_accepted, 1);
	rb_define_singleton_method(mKgio, "accept_class", get_accepted, 0);

	/*
	 * Document-class: Kgio::UNIXServer
	 *
	 * Kgio::UNIXServer should be used in place of the plain UNIXServer
	 * when kgio_accept and kgio_tryaccept methods are needed.
	 */
	cUNIXServer = rb_const_get(rb_cObject, rb_intern("UNIXServer"));
	cUNIXServer = rb_define_class_under(mKgio, "UNIXServer", cUNIXServer);
	rb_define_method(cUNIXServer, "kgio_tryaccept", unix_tryaccept, -1);
	rb_define_method(cUNIXServer, "kgio_accept", unix_accept, -1);

	/*
	 * Document-class: Kgio::TCPServer
	 *
	 * Kgio::TCPServer should be used in place of the plain TCPServer
	 * when kgio_accept and kgio_tryaccept methods are needed.
	 */
	cTCPServer = rb_const_get(rb_cObject, rb_intern("TCPServer"));
	cTCPServer = rb_define_class_under(mKgio, "TCPServer", cTCPServer);

	rb_define_method(cTCPServer, "kgio_tryaccept", tcp_tryaccept, -1);
	rb_define_method(cTCPServer, "kgio_accept", tcp_accept, -1);
	init_sock_for_fd();
	iv_kgio_addr = rb_intern("@kgio_addr");
}

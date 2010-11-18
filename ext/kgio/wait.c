#include "kgio.h"

static ID io_wait_rd, io_wait_wr;

/*
 * avoiding rb_thread_select() or similar since rb_io_wait_*able can be
 * made to use poll() later on.  It's highly unlikely Ruby will move to
 * use an edge-triggered event notification, so assign EAGAIN is safe...
 */
static VALUE force_wait_readable(VALUE self)
{
	errno = EAGAIN;
	if (!rb_io_wait_readable(my_fileno(self)))
		rb_sys_fail("wait readable");

	return self;
}

static VALUE force_wait_writable(VALUE self)
{
	errno = EAGAIN;
	if (!rb_io_wait_writable(my_fileno(self)))
		rb_sys_fail("wait writable");

	return self;
}

void kgio_call_wait_readable(VALUE io, int fd)
{
	/*
	 * we _NEVER_ set errno = EAGAIN here by default so we can work
	 * (or fail hard) with edge-triggered epoll()
	 */
	if (io_wait_rd) {
		(void)rb_funcall(io, io_wait_rd, 0, 0);
	} else {
		if (!rb_io_wait_readable(fd))
			rb_sys_fail("wait readable");
	}
}

void kgio_call_wait_writable(VALUE io, int fd)
{
	/*
	 * we _NEVER_ set errno = EAGAIN here by default so we can work
	 * (or fail hard) with edge-triggered epoll()
	 */
	if (io_wait_wr) {
		(void)rb_funcall(io, io_wait_wr, 0, 0);
	} else {
		if (!rb_io_wait_writable(fd))
			rb_sys_fail("wait writable");
	}
}

/*
 * call-seq:
 *
 *	Kgio.wait_readable = :method_name
 *	Kgio.wait_readable = nil
 *
 * Sets a method for kgio_read to call when a read would block.
 * This is useful for non-blocking frameworks that use Fibers,
 * as the method referred to this may cause the current Fiber
 * to yield execution.
 *
 * A special value of nil will cause Ruby to wait using the
 * rb_io_wait_readable() function.
 */
static VALUE set_wait_rd(VALUE mod, VALUE sym)
{
	switch (TYPE(sym)) {
	case T_SYMBOL:
		io_wait_rd = SYM2ID(sym);
		return sym;
	case T_NIL:
		io_wait_rd = 0;
		return sym;
	}
	rb_raise(rb_eTypeError, "must be a symbol or nil");
	return sym;
}

/*
 * call-seq:
 *
 *	Kgio.wait_writable = :method_name
 *	Kgio.wait_writable = nil
 *
 * Sets a method for kgio_write to call when a read would block.
 * This is useful for non-blocking frameworks that use Fibers,
 * as the method referred to this may cause the current Fiber
 * to yield execution.
 *
 * A special value of nil will cause Ruby to wait using the
 * rb_io_wait_writable() function.
 */
static VALUE set_wait_wr(VALUE mod, VALUE sym)
{
	switch (TYPE(sym)) {
	case T_SYMBOL:
		io_wait_wr = SYM2ID(sym);
		return sym;
	case T_NIL:
		io_wait_wr = 0;
		return sym;
	}
	rb_raise(rb_eTypeError, "must be a symbol or nil");
	return sym;
}

/*
 * call-seq:
 *
 *	Kgio.wait_writable	-> Symbol or nil
 *
 * Returns the symbolic method name of the method assigned to
 * call when EAGAIN is occurs on a Kgio::PipeMethods#kgio_write
 * or Kgio::SocketMethods#kgio_write call
 */
static VALUE wait_wr(VALUE mod)
{
	return io_wait_wr ? ID2SYM(io_wait_wr) : Qnil;
}

/*
 * call-seq:
 *
 *	Kgio.wait_readable	-> Symbol or nil
 *
 * Returns the symbolic method name of the method assigned to
 * call when EAGAIN is occurs on a Kgio::PipeMethods#kgio_read
 * or Kgio::SocketMethods#kgio_read call.
 */
static VALUE wait_rd(VALUE mod)
{
	return io_wait_rd ? ID2SYM(io_wait_rd) : Qnil;
}

void init_kgio_wait(void)
{
	VALUE mKgio = rb_define_module("Kgio");
	VALUE mWaiters = rb_define_module_under(mKgio, "DefaultWaiters");

	rb_define_method(mWaiters, "kgio_wait_readable",
	                 force_wait_readable, 0);
	rb_define_method(mWaiters, "kgio_wait_writable",
	                 force_wait_writable, 0);

	rb_define_singleton_method(mKgio, "wait_readable=", set_wait_rd, 1);
	rb_define_singleton_method(mKgio, "wait_writable=", set_wait_wr, 1);
	rb_define_singleton_method(mKgio, "wait_readable", wait_rd, 0);
	rb_define_singleton_method(mKgio, "wait_writable", wait_wr, 0);
}

#include "kgio.h"
#include "time_interval.h"
#include "wait_for_single_fd.h"

static ID id_wait_rd, id_wait_wr;

static int kgio_io_wait(int argc, VALUE *argv, VALUE self, int events)
{
	int fd = my_fileno(self);
	VALUE t;
	struct timeval *tp;
	struct timeval tv;

	if (rb_scan_args(argc, argv, "01", &t) == 0) {
		tp = NULL;
	} else {
		tv = rb_time_interval(t);
		tp = &tv;
	}
	return rb_wait_for_single_fd(fd, events, tp);
}

/*
 * call-seq:
 *
 *	io.kgio_wait_readable           -> IO
 *	io.kgio_wait_readable(timeout)  -> IO or nil
 *
 * Blocks the running Thread indefinitely until the IO object is readable
 * or if +timeout+ expires.  If +timeout+ is specified and expires, +nil+
 * is returned.
 *
 * This method is automatically called (without timeout argument) by default
 * whenever kgio_read needs to block on input.
 *
 * Users of alternative threading/fiber libraries are
 * encouraged to override this method in their subclasses or modules to
 * work with their threading/blocking methods.
 */
static VALUE kgio_wait_readable(int argc, VALUE *argv, VALUE self)
{
	int r = kgio_io_wait(argc, argv, self, RB_WAITFD_IN);

	if (r < 0) rb_sys_fail("kgio_wait_readable");
	return r == 0 ? Qnil : self;
}

/*
 * Blocks the running Thread indefinitely until the IO object is writable
 * or if +timeout+ expires.  If +timeout+ is specified and expires, +nil+
 * is returned.
 *
 * This method is automatically called (without timeout argument) by default
 * whenever kgio_write needs to block on output.
 *
 * Users of alternative threading/fiber libraries are
 * encouraged to override this method in their subclasses or modules to
 * work with their threading/blocking methods.
 */
static VALUE kgio_wait_writable(int argc, VALUE *argv, VALUE self)
{
	int r = kgio_io_wait(argc, argv, self, RB_WAITFD_OUT);

	if (r < 0) rb_sys_fail("kgio_wait_writable");
	return r == 0 ? Qnil : self;
}

VALUE kgio_call_wait_writable(VALUE io)
{
	return rb_funcall(io, id_wait_wr, 0, 0);
}

VALUE kgio_call_wait_readable(VALUE io)
{
	return rb_funcall(io, id_wait_rd, 0, 0);
}

void init_kgio_wait(void)
{
	VALUE mKgio = rb_define_module("Kgio");

	/*
	 * Document-module: Kgio::DefaultWaiters
	 *
	 * This module contains default kgio_wait_readable and
	 * kgio_wait_writable methods that block indefinitely (in a
	 * thread-safe manner) until an IO object is read or writable.
	 * This module is included in the Kgio::PipeMethods and
	 * Kgio::SocketMethods modules used by all bundled IO-derived
	 * objects.
	 */
	VALUE mWaiters = rb_define_module_under(mKgio, "DefaultWaiters");

	id_wait_rd = rb_intern("kgio_wait_readable");
	id_wait_wr = rb_intern("kgio_wait_writable");

	rb_define_method(mWaiters, "kgio_wait_readable",
	                 kgio_wait_readable, -1);
	rb_define_method(mWaiters, "kgio_wait_writable",
	                 kgio_wait_writable, -1);
}

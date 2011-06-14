#ifdef HAVE_RB_TIME_INTERVAL
/* not declared in public headers for Ruby <= 1.9.2 */
struct timeval rb_time_interval(VALUE num);
#else
#include <math.h>
#ifndef NUM2TIMET
#  define NUM2TIMET NUM2INT
#endif
#ifndef RFLOAT_VALUE
#  define RFLOAT_VALUE(f) (RFLOAT(f)->value)
#endif

static void negative_interval(void)
{
	rb_raise(rb_eArgError, "time interval must be positive");
}

static struct timeval kgio_time_interval(VALUE num)
{
	struct timeval tv;

	switch (TYPE(num)) {
	case T_FIXNUM:
	case T_BIGNUM:
		tv.tv_sec = NUM2TIMET(num);
		if (tv.tv_sec < 0)
			negative_interval();
		tv.tv_usec = 0;
		break;
	case T_FLOAT: {
		double f, d;
		double val = RFLOAT_VALUE(num);

		if (val < 0.0)
			negative_interval();

		d = modf(val, &f);
		if (d >= 0) {
			tv.tv_usec = (long)(d * 1e6 + 0.5);
		} else {
			tv.tv_usec = (long)(-d * 1e6 + 0.5);
			if (tv.tv_usec > 0) {
				tv.tv_usec = 1000000 - tv.tv_usec;
				f -= 1;
			}
		}
		tv.tv_sec = (time_t)f;
		if (f != tv.tv_sec)
			rb_raise(rb_eRangeError, "%f out of range", val);
	}
		break;
	default: {
		VALUE f;
		VALUE ary = rb_funcall(num, rb_intern("divmod"), 1, INT2FIX(1));

		Check_Type(ary, T_ARRAY);

		tv.tv_sec = NUM2TIMET(rb_ary_entry(ary, 0));
		f = rb_ary_entry(ary, 1);
		f = rb_funcall(f, '*', 1, INT2FIX(1000000));
		tv.tv_usec = NUM2LONG(f);

		if (tv.tv_sec < 0)
			negative_interval();

	}
	}
	return tv;
}
#define rb_time_interval(v) kgio_time_interval(v)
#endif /* HAVE_RB_TIME_INTERVAL */

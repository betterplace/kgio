/* 1.9.3 uses ppoll() for this */
#ifndef HAVE_RB_WAIT_FOR_SINGLE_FD
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#if defined(HAVE_POLL)
#  include <poll.h>
#  define RB_WAITFD_IN  POLLIN
#  define RB_WAITFD_PRI POLLPRI
#  define RB_WAITFD_OUT POLLOUT
#else
#  define RB_WAITFD_IN  0x001
#  define RB_WAITFD_PRI 0x002
#  define RB_WAITFD_OUT 0x004
#endif

static int kgio_wait_for_single_fd(int fd, int events, struct timeval *tv)
{
	fd_set fds;
	fd_set *rfds;
	fd_set *wfds;
	int r;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (events == RB_WAITFD_IN) {
		rfds = &fds;
		wfds = NULL;
	} else if (events == RB_WAITFD_OUT) {
		rfds = NULL;
		wfds = &fds;
	} else {
		rb_bug("incomplete rb_wait_for_single_fd emulation");
	}

	r = rb_thread_select(fd + 1, rfds, wfds, NULL, tv);
	if (r <= 0)
		return r;
	return events;
	rb_bug("rb_wait_for_single_fd emulation bug");
}
#define rb_wait_for_single_fd(fd,events,tv) \
        kgio_wait_for_single_fd((fd),(events),(tv))
#endif

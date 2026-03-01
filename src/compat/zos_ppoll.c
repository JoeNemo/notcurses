///////////////////////////////////////////////////////////////////////////////
// z/OS USS compatibility shims: ppoll() and pipe2()
//
// These compile to nothing on non-z/OS platforms.
///////////////////////////////////////////////////////////////////////////////
#ifdef __MVS__

#define _OPEN_SYS_EXT 1

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

// ---------------------------------------------------------------------------
// ppoll() - z/OS provides poll() but not ppoll().
//
// Implements ppoll() using sigprocmask() + poll() + sigprocmask().
// The signal-mask swap and poll are NOT atomic (no kernel support on z/OS).
// This matches the glibc fallback when the ppoll syscall is unavailable.
// ---------------------------------------------------------------------------
int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *tmo_p,
          const sigset_t *sigmask)
{
    int timeout_ms;

    if (tmo_p == NULL) {
        timeout_ms = -1;
    } else if (tmo_p->tv_sec < 0 ||
               tmo_p->tv_nsec < 0 ||
               tmo_p->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    } else {
        long long ms = (long long)tmo_p->tv_sec * 1000
                     + (tmo_p->tv_nsec + 999999) / 1000000;
        timeout_ms = (ms > INT_MAX) ? INT_MAX : (int)ms;
    }

    sigset_t orig_mask;
    if (sigmask) {
        if (sigprocmask(SIG_SETMASK, sigmask, &orig_mask) != 0)
            return -1;
    }

    int ret = poll(fds, nfds, timeout_ms);
    int saved_errno = errno;

    if (sigmask)
        sigprocmask(SIG_SETMASK, &orig_mask, NULL);

    errno = saved_errno;
    return ret;
}

// ---------------------------------------------------------------------------
// pipe2() - z/OS provides pipe() but not pipe2().
//
// Creates a pipe and then applies the requested flags (O_CLOEXEC,
// O_NONBLOCK) via fcntl().  Not atomic, but matches the behaviour of
// the glibc emulation on older kernels.
// ---------------------------------------------------------------------------
#ifndef O_CLOEXEC
#define O_CLOEXEC 0x00001000
#endif

int pipe2(int pipefd[2], int flags)
{
    if (pipe(pipefd) != 0)
        return -1;

    /* Apply O_CLOEXEC */
    if (flags & O_CLOEXEC) {
        if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) == -1 ||
            fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1) {
            int e = errno;
            close(pipefd[0]);
            close(pipefd[1]);
            errno = e;
            return -1;
        }
    }

    /* Apply O_NONBLOCK */
    if (flags & O_NONBLOCK) {
        int f0 = fcntl(pipefd[0], F_GETFL);
        int f1 = fcntl(pipefd[1], F_GETFL);
        if (f0 == -1 || f1 == -1 ||
            fcntl(pipefd[0], F_SETFL, f0 | O_NONBLOCK) == -1 ||
            fcntl(pipefd[1], F_SETFL, f1 | O_NONBLOCK) == -1) {
            int e = errno;
            close(pipefd[0]);
            close(pipefd[1]);
            errno = e;
            return -1;
        }
    }

    return 0;
}

#endif /* __MVS__ */

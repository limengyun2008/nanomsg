/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_USOCK_INCLUDED
#define NN_USOCK_INCLUDED

/*  Import the definition of nn_iovec. */
#include "../nn.h"

/*  OS-level sockets. */

/*  Event types generated by nn_usock. */
#define NN_USOCK_CONNECTED 1
#define NN_USOCK_ACCEPTED 2
#define NN_USOCK_SENT 3
#define NN_USOCK_RECEIVED 4
#define NN_USOCK_ERROR 5
#define NN_USOCK_STOPPED 6

/*  Maximum number of iovecs that can be passed to nn_usock_send function. */
#define NN_USOCK_MAX_IOVCNT 3

/*  Size of the buffer used for batch-reads of inbound data. To keep the
    performance optimal make sure that this value is larger than network MTU. */
#define NN_USOCK_BATCH_SIZE 2048

#if defined NN_HAVE_WINDOWS
#include "usock_win.h"
#else
#include "usock_posix.h"
#endif

void nn_usock_init (struct nn_usock *self, struct nn_fsm *owner);
void nn_usock_term (struct nn_usock *self);

int nn_usock_isidle (struct nn_usock *self);
int nn_usock_start (struct nn_usock *self, int domain, int type, int protocol);
void nn_usock_stop (struct nn_usock *self);

struct nn_fsm *nn_usock_swap_owner (struct nn_usock *self,
    struct nn_fsm *newowner);

int nn_usock_setsockopt (struct nn_usock *self, int level, int optname,
    const void *optval, size_t optlen);

int nn_usock_bind (struct nn_usock *self, const struct sockaddr *addr,
    size_t addrlen);
int nn_usock_listen (struct nn_usock *self, int backlog);

/*  Accept a new connection. The 'newsock' is the resulting accepted socket.
    It should be initialised before the call. The call itself will start it. */
void nn_usock_accept (struct nn_usock *self, struct nn_usock *newsock);

/*  When all the tuning is done on the accepted socket, call this function
    to start polling on it. */
void nn_usock_activate (struct nn_usock *self);

void nn_usock_connect (struct nn_usock *self, const struct sockaddr *addr,
    size_t addrlen);

void nn_usock_send (struct nn_usock *self, const struct nn_iovec *iov,
    int iovcnt);
void nn_usock_recv (struct nn_usock *self, void *buf, size_t len);

#endif

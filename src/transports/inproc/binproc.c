/*
    Copyright (c) 2012-2013 250bpm s.r.o.

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

#include "binproc.h"
#include "sinproc.h"
#include "cinproc.h"
#include "inproc.h"

#include "../../utils/err.h"
#include "../../utils/cont.h"
#include "../../utils/fast.h"
#include "../../utils/alloc.h"

#define NN_BINPROC_STATE_IDLE 1
#define NN_BINPROC_STATE_ACTIVE 2
#define NN_BINPROC_STATE_STOPPING 3

#define NN_BINPROC_SRC_SINPROC 1

/*  Implementation of nn_epbase interface. */
static void nn_binproc_stop (struct nn_epbase *self);
static void nn_binproc_destroy (struct nn_epbase *self);
static const struct nn_epbase_vfptr nn_binproc_vfptr = {
    nn_binproc_stop,
    nn_binproc_destroy
};

/*  Private functions. */
static void nn_binproc_handler (struct nn_fsm *self, int src, int type,
    void *srcptr);

struct nn_binproc *nn_binproc_create (void *hint)
{
    struct nn_binproc *self;
    size_t sz;

    self = nn_alloc (sizeof (struct nn_binproc), "binproc");
    alloc_assert (self);

    nn_epbase_init (&self->epbase, &nn_binproc_vfptr, hint);
    nn_fsm_init_root (&self->fsm, nn_binproc_handler,
        nn_epbase_getctx (&self->epbase));
    self->state = NN_BINPROC_STATE_IDLE;
    nn_list_init (&self->sinprocs);
    nn_list_item_init (&self->item);
    sz = sizeof (self->protocol);
    nn_epbase_getopt (&self->epbase, NN_SOL_SOCKET, NN_PROTOCOL,
        &self->protocol, &sz);
    nn_assert (sz == sizeof (self->protocol));
    self->connects = 0;

    /*  Start the state machine. */
    nn_fsm_start (&self->fsm);

    return self;
}

static void nn_binproc_stop (struct nn_epbase *self)
{
    struct nn_binproc *binproc;

    binproc = nn_cont (self, struct nn_binproc, epbase);

    nn_fsm_stop (&binproc->fsm);
}

static void nn_binproc_destroy (struct nn_epbase *self)
{
    struct nn_binproc *binproc;

    binproc = nn_cont (self, struct nn_binproc, epbase);

    nn_list_item_term (&binproc->item);
    nn_list_term (&binproc->sinprocs);
    nn_fsm_term (&binproc->fsm);
    nn_epbase_term (&binproc->epbase);

    nn_free (binproc);
}

const char *nn_binproc_getaddr (struct nn_binproc *self)
{
    return nn_epbase_getaddr (&self->epbase);
}

void nn_binproc_connect (struct nn_binproc *self, struct nn_cinproc *peer)
{
    struct nn_sinproc *sinproc;

    nn_assert (self->state == NN_BINPROC_STATE_ACTIVE);

    sinproc = nn_alloc (sizeof (struct nn_sinproc), "sinproc");
    alloc_assert (sinproc);
    nn_sinproc_init (sinproc, NN_BINPROC_SRC_SINPROC,
        &self->epbase, &self->fsm);
    nn_list_insert (&self->sinprocs, &sinproc->item,
        nn_list_end (&self->sinprocs));
    nn_sinproc_connect (sinproc, &peer->fsm);
}

static void nn_binproc_handler (struct nn_fsm *self, int src, int type,
    void *srcptr)
{
    struct nn_binproc *binproc;
    struct nn_list_item *it;
    struct nn_sinproc *sinproc;
    struct nn_sinproc *peer;

    binproc = nn_cont (self, struct nn_binproc, fsm);

/******************************************************************************/
/*  STOP procedure.                                                           */
/******************************************************************************/
    if (nn_slow (src == NN_FSM_ACTION && type == NN_FSM_STOP)) {

        /*  First, unregister the endpoint from the global repository of inproc
            endpoints. This way, new connections cannot be created anymore. */
        nn_inproc_unbind (binproc);

        /*  Stop the existing connections. */
        for (it = nn_list_begin (&binproc->sinprocs);
              it != nn_list_end (&binproc->sinprocs);
              it = nn_list_next (&binproc->sinprocs, it)) {
            sinproc = nn_cont (it, struct nn_sinproc, item);
            nn_sinproc_stop (sinproc);
        }

        binproc->state = NN_BINPROC_STATE_STOPPING;
        goto finish;
    }
    if (nn_slow (binproc->state == NN_BINPROC_STATE_STOPPING)) {
        nn_assert (src == NN_BINPROC_SRC_SINPROC && type == NN_SINPROC_STOPPED);
        sinproc = (struct nn_sinproc*) srcptr;
        nn_list_erase (&binproc->sinprocs, &sinproc->item);
        nn_sinproc_term (sinproc);
        nn_free (sinproc);
finish:
        if (!nn_list_empty (&binproc->sinprocs))
            return;
        binproc->state = NN_BINPROC_STATE_IDLE;
        nn_fsm_stopped_noevent (&binproc->fsm);
        nn_epbase_stopped (&binproc->epbase);
        return;
    }

    switch (binproc->state) {

/******************************************************************************/
/*  IDLE state.                                                               */
/******************************************************************************/
    case NN_BINPROC_STATE_IDLE:
        switch (src) {

        case NN_FSM_ACTION:
            switch (type) {
            case NN_FSM_START:
                binproc->state = NN_BINPROC_STATE_ACTIVE;
                return;
            default:
                nn_assert (0);
            }

        default:
            nn_assert (0);
        }

/******************************************************************************/
/*  ACTIVE state.                                                             */
/******************************************************************************/
    case NN_BINPROC_STATE_ACTIVE:
        switch (src) {

        case NN_SINPROC_SRC_PEER:
            switch (type) {
            case NN_SINPROC_CONNECT:
                peer = (struct nn_sinproc*) srcptr;
                sinproc = nn_alloc (sizeof (struct nn_sinproc), "sinproc");
                alloc_assert (sinproc);
                nn_sinproc_init (sinproc, NN_BINPROC_SRC_SINPROC,
                    &binproc->epbase, &binproc->fsm);
                nn_list_insert (&binproc->sinprocs, &sinproc->item,
                    nn_list_end (&binproc->sinprocs));
                nn_sinproc_accept (sinproc, peer);
                return;
            default:
                nn_assert (0);
            }

        default:
            nn_assert (0);
        }

/******************************************************************************/
/*  Invalid state.                                                            */
/******************************************************************************/
    default:
        nn_assert (0);
    }
}


#ifndef EVCOMPAT_INTERNAL_H
#define EVCOMPAT_INTERNAL_H

#include <sys/types.h>
#include <sys/event.h>

#ifdef FKQMULTI
    #define MULT_KQ (1)
#else
    #define MULT_KQ (0) 

    #define FKQMULTI (0)
    #define FKQMPRNT (0)
    #define FKQTUNE (0)
    #define EV_REALTIME (0)
    #define KQTUNE_MAKE(obj, val) (0)
    #define KQTUNE_FREQ (0)
    #define KQTUNE_RTSHARE (0)
#endif

#define UNUSED(x) (void)(x)
/* 
 * for event_base->state
 */
#define EVS_ACTIVE 0x1 /* event has triggered and is being handled */
#define EVS_PENDING 0x2 /* event is queued to a event base */


/* 
 * return value
 */
#define CS_OK (0)
#define CS_ERR (-1)

/* 
 * event_del flags
 */
#define EVENT_DEL_BLOCK (0)
#define EVENT_DEL_NOBLOCK (1)

#define NEVENT (128)
#define CV_INIT_MAGIC (0xdeedb33f)


#endif

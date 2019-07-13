#ifndef EVCOMPAT_INTERNAL_H
#define EVCOMPAT_INTERNAL_H

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
#define CS_WARN (1)

/* 
 * event_del flags
 */
#define EVENT_DEL_BLOCK (0)
#define EVENT_DEL_NOBLOCK (1)

#define NEVENT (64)
#define FKQMULTI (0)
#define KQ_SCHED_FLAG (0x3)
#define CV_INIT_MAGIC (0xdeedb33f)

#endif

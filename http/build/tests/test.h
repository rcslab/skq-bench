
#ifndef __CELESTIS_TEST_H__
#define __CELESTIS_TEST_H__

#include <celestis/debug.h>

#define TEST_ASSERT(_x) \
    if (!(_x)) { \
        Debug_Log(LEVEL_SYS, "ASSERT("#_x"): %s %s:%d\n", \
                __FUNCTION__, __FILE__, __LINE__); \
        assert(_x); \
    }

#endif /* __CELESTIS_TEST_H__ */


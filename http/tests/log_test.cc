
#include <errno.h>

#include <string>

#include <celestis/debug.h>

int
main(int argc, const char *argv[])
{
    Debug_OpenLog("test.log");

    SYSERROR("Test syserror message");
    WARNING("Test warning message");
    MSG("Test msg message");
    LOG("Test log message");
    DLOG("Test debug log");
    Debug_Perror("test", ENOSYS);
    Debug_LogBacktrace();
    Debug_PrintBacktrace();
    Debug_PrintHex("01234567890123456789012345678901", 0, 32);

    return 0;
}


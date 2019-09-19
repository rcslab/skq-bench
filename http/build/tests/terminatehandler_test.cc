
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>

#include <string>
#include <exception>

#include <celestis/debug.h>

int
main(int argc, const char *argv[])
{
    int status;

    Debug_OpenLog("test.log");

    pid_t p = fork();
    if (p == 0) {
	throw std::exception();
    } else {
	waitpid(p, &status, 0);
	if (WTERMSIG(status) != SIGABRT) {
		LOG("Process terminated with unexpected signal");
		PANIC();
	}
	LOG("Process terminated with SIGABRT");
    }

    return 0;
}


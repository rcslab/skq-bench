
COMPILER_FEATURES=c++11

PROG_CXX= dismember
LDADD+= -lthr
CFLAGS += -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -I../include
MK_MAN=no

SRCS= dismember.cc util.cc Generator.cc reqgen.cc

.include <bsd.prog.mk>

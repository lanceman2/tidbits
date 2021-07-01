#ifndef __PO_TATO_H__
#define __PO_TATO_H__

#include <unistd.h>
#include <stdbool.h>

// bit mask flags for optflags below
#define PO_OPTION_COPY_LISTENERS     01
#define PO_OPTION_REPLACE_LISTENERS  02
#define PO_OPTION_FORK               04


// *argv points to argv[1] to start with, not argv[0].
// argv must be NULL terminated as it is for main(int argc, char **argv)
extern
bool poTato_doOptions(const char **argv, int optflags,
        bool (*doOtherOpts)(const char *argv1, const char *argv2));

extern void poTato_printOptionUsage(FILE *f);
extern void poTato_printOptionHelp(FILE *f);

// If poTato_doOptions() was called with flag PO_OPTION_FORK this
// will fork the requested number of times, otherwise it will
// not fork.  Returns true on error.
extern
bool poTato_doOptionsFork(void);

// Use without poTato_doOptions().
extern
pid_t poTato_fork(void);

// Use without poTato_doOptions().
extern
bool poTato_signalHandOff(const char *pid, bool shutdownOldServer);

extern // returns status which is sum of return codes. max at 255
int poTato_removeChildren(void);

#endif //#ifndef __PO_TATO_H__

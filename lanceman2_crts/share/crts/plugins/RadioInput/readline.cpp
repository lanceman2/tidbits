#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "crts/RadioInput.hpp"
#include "crts.h" // for:  FILE *crtsOut


class Readline : public CRTSRadioInput
{
    public:

        Readline(int argc, const char **argv);
        ~Readline(void);

        ssize_t read(void *buffer, size_t bufferLen);

    private:

        const char *prompt;
        char *line; // last line read buffer
};

Readline::Readline(int argc, const char **argv): prompt("> "), line(0)
{
    int i;

    i=0;
    while(i<argc)
    {
        // TODO: Keep argument option parsing simple??
        //
        if(!strcmp("--prompt", argv[i]) && i+1 < argc)
        {
            prompt = argv[++i];
            ++i;
            continue;
        }
        fprintf(stderr, "module: %s: unknown option arg=\"%s\"\n",
                __BASE_FILE__, argv[i]);
        ++i;
    }

    // Because libuhd pollutes stdout we must use a different prompt
    // stream:
    rl_outstream = crtsOut;
}

Readline::~Readline(void)
{
    if(line)
    {
        free(line);
        line = 0;
    }

    // TODO: cleanup readline?

}

//#define IS_WHITE(x)  ((x) < '!' || (x) > '~')
#define IS_WHITE(x)  ((x) == '\n')

// TODO: readline makes extra copies of the input data, and then we have
// to copy it yet again to the input buffer.  All we really need it to do
// is write to the buffer pointed to by the pointer passed in the function
// argument, buffer.  Oh well.  It's small beans any way.  Inefficient,
// but small price for lots of functionally that readline provides.  The
// amount of data read by readline is usually small anyway.
//
// We may consider using isatty() to choose between modules readline.cpp
// and fd0.cpp.
//
//
// This call will block until we get input.
//
// We let this code work correctly for the case when buffer (bufferLen) is
// not large enough to hold all the data.
//
ssize_t Readline::read(void *buffer, size_t bufferLen)
{
    while(1)
    {
        // get a line
        line = readline(prompt);
        if(!line) continue;

        //fprintf(stderr, "%s:%d: GOT: \"%s\"  prompt=\"%s\"\n", __BASE_FILE__, __LINE__, line, prompt);

        // Strip off trailing white chars:
        size_t len = strlen(line);
        while(len && IS_WHITE(line[len -1]))
            line[--len] = '\0';

        if(len < 1)
        {
            free(line);
            line = 0;
            continue;
        }

        if(len < bufferLen)
        {
            // Very likely case for a large buffer.  We eat it all at
            // once, null ('\0') terminator and all.
            memcpy(buffer, line, bufferLen = len + 1);
        }
        else
        {
            memcpy(buffer, line, bufferLen -1);
            // We need to add a '\0' terminator.
            ((char *)buffer)[bufferLen - 1] = '\0';
        }

        add_history((char *) buffer);
        free(line);
        line = 0;

        return bufferLen;
    }
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Readline)

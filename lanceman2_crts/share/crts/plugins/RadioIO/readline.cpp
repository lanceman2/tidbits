#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "crts/RadioIO.hpp"
#include "crts.h" // for:  FILE *crtsOut


class Readline : public CRTSRadioIO
{
    public:

        Readline(int argc, const char **argv);
        ~Readline(void);

        // Called in tx thread
        ssize_t read(void *buffer, size_t bufferLen);

        // Called in rx thread
        ssize_t write(const void *buffer, size_t bufferLen);

    private:

        bool reading; // So can we exit do to command from read()
        const char *prompt;
        const char *sendPrefix;
        char *line; // last line read buffer
        size_t sendPrefixLen;
        ssize_t cleanExit(void);
        size_t promptLen;
};


Readline::Readline(int argc, const char **argv): reading(true), prompt("> "),
    sendPrefix(""), line(0)
{
    int i = 0;

    while(i<argc)
    {
        // TODO: Keep argument option parsing simple??
        //
        // argv[0] is the first argument
        //
        if(!strcmp("--prompt", argv[i]) && i+1 < argc)
        {
            prompt = argv[++i];
            ++i;
            continue;
        }
        if(!strcmp("--send-prefix", argv[i]) && i+1 < argc)
        {
            sendPrefix = argv[++i];
            ++i;
            continue;
        }

        fprintf(stderr, "module: %s: unknown option arg=\"%s\"\n",
                __BASE_FILE__, argv[i]);
        ++i;
    }

    promptLen = strlen(prompt);
    
    // Because libuhd pollutes stdout we must use a different readline
    // prompt stream:
    rl_outstream = crtsOut;

    sendPrefixLen = strlen(sendPrefix);
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


// From crts_radio.cpp
extern void crtsExit(void);


ssize_t Readline::cleanExit(void)
{
    crtsExit();
    // To stop the race so read don't go to another blocking
    // readline() call before the mainThread can stop this
    // thread.
    reading = false;
    return 0; 
}


#define IS_WHITE(x)  ((x) < '!' || (x) > '~')
//#define IS_WHITE(x)  ((x) == '\n')

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

    while(reading)
    {
        // get a line
        line = readline(prompt);
        if(!line) return cleanExit();

        //fprintf(stderr, "%s:%d: GOT: \"%s\"  prompt=\"%s\"\n", __BASE_FILE__, __LINE__, line, prompt);

        // Strip off trailing white chars:
        size_t len = strlen(line);
        while(len && IS_WHITE(line[len -1]))
            line[--len] = '\0';

        //fprintf(crtsOut, "\n\nbuffer=\"%s\"\n\n", line);

        if(len < 1)
        {
            free(line);
            line = 0;
            continue;
        }

        // TODO: add tab help, history saving, and other readline user
        // interface stuff.

        if(!strcmp(line, "exit") || !strcmp(line, "quit"))
            return cleanExit();

        // Return char counter:
        ssize_t nRet = 0;


        // TODO: Skip the prefix if it's too long ??
        // Better than dumb segfault, for now...
        if(sendPrefixLen && sendPrefixLen < bufferLen)
        {
            // Put the prefix in the buffer first:
            memcpy(buffer, sendPrefix, sendPrefixLen);
            // Update remaining buffer length and advance the buffer pointer
            bufferLen -= sendPrefixLen;
            buffer = (void *) (((char *) buffer) + sendPrefixLen);
            nRet += sendPrefixLen;
        }

        // We put the '\0' terminator in the buffer too:
        //
        if(len < bufferLen)
        {
            // Very likely case for a large buffer.  We eat it all at
            // once, null ('\0') terminator and all.
            memcpy(buffer, line, len + 1);
            // We don't need to send the '\0'
            nRet += len;
        }
        else
        {
            // In this case we just ignore the extra data.
            memcpy(buffer, line, bufferLen -1);
            // We add a '\0' terminator as the last char
            // in the buffer.
            ((char *)buffer)[bufferLen - 1] = '\0';
            // We don't need to send the '\0'
            nRet += bufferLen;
        }

        // We do not add the sendPrefix to the history.  Note: buffer was
        // '\0' terminated just above, so the history string is cool.
        add_history((char *) buffer);

        free(line); // reset readline().
        line = 0;

        return nRet;
    }

    // We set reading to false and so we just return 0 as a EOF signal.
    return 0;
}


// This runs in another thread from Readline::read()
// We write to crtsOut
ssize_t Readline::write(const void *buffer, size_t bufferLen)
{
    // fwrite() returns the number of items written and we request just one
    // item of size bufferLen to be written.
    //
    // This is the only place crtsOut is used in all the running program.
    //
    fputc('\r', crtsOut);
    size_t nw = fwrite(buffer, bufferLen, 1, crtsOut);

    // TODO: Assuming we have all one space chars (not escape chars):
    if(bufferLen < promptLen)
    {
        int i = promptLen - bufferLen;
        while(i--) putc(' ',  crtsOut);
    }
    putc('\n', crtsOut);

    // Since prompt will never change while this is running, this is
    // thread-safe.  Since we are using streams this is not wasting
    // write() system calls.  Writing to libc streams is thread-safe.
    if(prompt)
        fputs(prompt, crtsOut);

    fflush(crtsOut);

    // We don't report the length associated with writing the prompt
    // because we added that here.
    return bufferLen * nw; // This returns zero or buffLen
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Readline)

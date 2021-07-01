/* This module provides the read() interface, to read fd 0, standard input
 * without a stream buffer like stdin, for the TX (transmitter) thread in
 * the program crts_radio. */

#include <unistd.h>

#define _DEBUG_
#ifdef _DEBUG_
#  include <stdio.h>
#endif

#include "crts/RadioInput.hpp"

class Fd0 : public CRTSRadioInput
{
    public:

        Fd0(int argc, const char **argv);

        ssize_t read(void *buffer, size_t bufferLen);
};

Fd0::Fd0(int argc, const char **argv)
{
#ifdef _DEBUG_ // TODO: remove this DEBUG SPEW
    fprintf(stderr, "%s:%d:%s(): GOT ARGS\n", __FILE__, __LINE__, __func__);
    for(int i=0; i<argc; ++i)
        fprintf(stderr, "ARG[%d]=\"%s\"\n", i, argv[i]);
    fprintf(stderr, "\n");
#endif
}

ssize_t Fd0::read(void *buffer, size_t bufferLen)
{
    // Using the read() call from libc
    return ::read(0, buffer, bufferLen);
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Fd0)

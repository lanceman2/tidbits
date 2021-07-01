#include <stdio.h>
#include <unistd.h>

#include "crts/RadioIO.hpp"
#include "crts.h" // for:  FILE *crtsOut

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class Fd1 : public CRTSRadioOutput
{
    public:

        Fd1(int argc, const char **argv);

        ssize_t write(const void *buffer, size_t bufferLen);
};

Fd1::Fd1(int argc, const char **argv) {DSPEW();}

ssize_t Fd1::write(const void *buffer, size_t bufferLen)
{
    // As it turns out we have to use 3 like it was STDOUT_FILENO because
    // the libuhd library shits to STDOUT_FILENO making that file
    // descriptor useless for applications that wish to use STDOUT_FILENO.
    // The libuhd library is a bad library.  See
    // ../../../../lib/libcrts.cpp, it in effect makes 3 act like
    // STDOUT_FILENO (1), via dup2() and so on (just look and see).
    //
    // Using the libc write().
    return ::write(3, buffer, bufferLen);
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioOutput_MAKE_MODULE(Fd1)

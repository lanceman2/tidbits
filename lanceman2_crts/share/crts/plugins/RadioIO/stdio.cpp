#include <stdio.h>

#include "crts/RadioIO.hpp"
#include "crts.h" // for:  FILE *crtsOut

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class Stdio : public CRTSRadioIO
{
    public:

        Stdio(int argc, const char **argv);

        ssize_t write(const void *buffer, size_t bufferLen);
        ssize_t read(void *buffer, size_t bufferLen);
};

Stdio::Stdio(int argc, const char **argv) {DSPEW();}

ssize_t Stdio::write(const void *buffer, size_t bufferLen)
{
   return (ssize_t) fwrite(buffer, 1, bufferLen, crtsOut);
}

ssize_t Stdio::read(void *buffer, size_t bufferLen)
{
   return (ssize_t) fread(buffer, 1, bufferLen, stdin);
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioIO_MAKE_MODULE(Stdio)

#include <stdio.h>

#include "crts/RadioIO.hpp"
#include "crts.h" // for:  FILE *crtsOut

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class UDPL : public CRTSRadioIO
{
    public:

        UDPL(int argc, const char **argv);
        ~UDPL(void);

        ssize_t write(const void *buffer, size_t bufferLen);
        ssize_t read(void *buffer, size_t bufferLen);
};

UDPL::UDPL(int argc, const char **argv) {DSPEW();}

UDPL::~UDPL(void) {DSPEW();}

ssize_t UDPL::write(const void *buffer, size_t bufferLen)
{
   return (ssize_t) fwrite(buffer, 1, bufferLen, crtsOut);
}

ssize_t UDPL::read(void *buffer, size_t bufferLen)
{
   return (ssize_t) fread(buffer, 1, bufferLen, stdin);
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioIO_MAKE_MODULE(UDPL)

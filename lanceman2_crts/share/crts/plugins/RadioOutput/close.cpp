#include <stdio.h>

#include "crts/RadioIO.hpp"
#include "crts.h" // for:  FILE *crtsOut

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class Close : public CRTSRadioOutput
{
    public:

        Close(int argc, const char **argv);

        ssize_t write(const void *buffer, size_t bufferLen);
};

Close::Close(int argc, const char **argv) {DSPEW();}

ssize_t Close::write(const void *buffer, size_t bufferLen)
{
   return 0; // return End Of File
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioOutput_MAKE_MODULE(Close)

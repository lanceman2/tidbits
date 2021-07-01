#include <stdio.h>

#include "crts/RadioInput.hpp"

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class Close : public CRTSRadioInput
{
    public:

        Close(int argc, const char **argv);

        ssize_t read(void *buffer, size_t bufferLen);
};

Close::Close(int argc, const char **argv) {DSPEW();}

ssize_t Close::read(void *buffer, size_t bufferLen)
{
    return 0; // return the closed input value.
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Close)

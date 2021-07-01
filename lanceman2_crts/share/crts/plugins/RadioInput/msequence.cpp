#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "crts/RadioInput.hpp"

//#define DSPEW() /*empty macro*/
#define DSPEW() fprintf(stderr, "%s:%d:%s()\n", __FILE__, __LINE__, __func__)

class MSequence : public CRTSRadioInput
{
    public:

        MSequence(int argc, const char **argv);

        ssize_t read(void *buffer, size_t bufferLen);
};

MSequence::MSequence(int argc, const char **argv) {
    DSPEW();

    // TODO: write this code using liquid-dsp 

    //msequence_create(m,g,a)
    
    
    
    DSPEW();
}

ssize_t MSequence::read(void *buffer, size_t bufferLen)
{
    // TODO: write this code using liquid-dsp msequence

    memset(buffer, 0xFF, bufferLen); // Whatever!

    return (ssize_t) bufferLen;
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(MSequence)

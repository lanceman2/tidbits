#include <stdio.h>

#include "crts/RadioInput.hpp"

class Stdin : public CRTSRadioInput
{
    public:

        Stdin(int argc, const char **argv);

        ssize_t read(void *buffer, size_t bufferLen);
};

Stdin::Stdin(int argc, const char **argv)
{
#if 1 // TODO: remove this DEBUG SPEW
    fprintf(stderr, "%s:%d:%s(): GOT ARGS\n", __FILE__, __LINE__, __func__);
    for(int i=0; i<argc; ++i)
        fprintf(stderr, "ARG[%d]=\"%s\"\n", i, argv[i]);
    fprintf(stderr, "\n");
#endif
}

ssize_t Stdin::read(void *buffer, size_t bufferLen)
{
    // TODO: if stdin is a tty we can add the use of GNU readline,
    // then we could make this into the end of a telnet-like thing.

    return (ssize_t) fread(buffer, 1, bufferLen, stdin);
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Stdin)

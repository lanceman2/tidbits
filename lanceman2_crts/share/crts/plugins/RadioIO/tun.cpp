#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "crts/RadioIO.hpp"

#include "getTunViaUSocket.hpp"


class Tun : public CRTSRadioIO
{
    public:

        Tun(int argc, const char **argv);
        ~Tun(void);

        // Called in tx thread
        ssize_t read(void *buffer, size_t bufferLen);

        // Called in rx thread
        ssize_t write(const void *buffer, size_t bufferLen);

    private:

        int fd; // File descriptor to the TUN
};


Tun::Tun(int argc, const char **argv): fd(-1)
{
    int i = 0;
    const char *subnet = "10.0.0.0/30"; // default

    while(i<argc)
    {
        // TODO: Keep argument option parsing simple??
        //
        // argv[0] is the first argument
        //
        if(!strcmp("--subnet", argv[i]) && i+1 < argc)
        {
            subnet = argv[++i];
            ++i;
            continue;
        }

        fprintf(stderr, "module: %s: unknown option arg=\"%s\"\n",
                __BASE_FILE__, argv[i]);
        ++i;
    }

    fd = getTunViaUSocket(subnet);

    if(fd < 0)
        throw "File:" __FILE__ " This code is not done yet";

}

Tun::~Tun(void)
{
    if(fd >= 0)
    {
        close(fd);
        fd = -1;
    }
}

ssize_t Tun::read(void *buffer, size_t bufferLen)
{
    return 0;
}


// This runs in another thread from Tun::read()
// We write to crtsOut
ssize_t Tun::write(const void *buffer, size_t bufferLen)
{
    return 0;
}


// Define the module loader stuff to make one of these class objects.
CRTSRadioInput_MAKE_MODULE(Tun)

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <inttypes.h>
#include <string>
#include <atomic>
#include <stdlib.h>
#include <sys/types.h>
#include <uhd/usrp/multi_usrp.hpp>

#include "liquid.h"
#include "crts/RadioInput.hpp"
#include "crts/RadioOutput.hpp"
#include "crts/RadioIO.hpp"

#include "debug.h"

#include "get_opt.hpp"
#include "ModuleLoader.hpp"
#include "module_util.hpp"
#include "usrp_set_parameters.hpp" // UHD usrp wrappers
#include "pthread_wrappers.h" // some pthread_*() wrappers
#include "threadShared.hpp"



// Shared in threadShared.hpp
//
// For thread safety of lib FFTW-3 ofdmflexframe create and destroy
// functions.  See additional comments in threadShared.hpp
pthread_mutex_t fftw3_mutex = PTHREAD_MUTEX_INITIALIZER;
//
// Wup, we got a startup/shutdown barrier thingy.
pthread_barrier_t barrier;


///////////////////////////////////////////////////////////////////////////
// USRP DEFAULT INITIAL SETTINGS
//
// These settings are not so hard coded, these are just the default
// initial (startup) values, that are the values before command-line
// arguments are parsed.  These USPR UHD parameters may then change again
// as the program runs.
//
#define RX_FREQ  (914.5) // in MHz = 1e6 Hz
#define RX_RATE  (1.0)   // millions of samples per sec (bandWidth related)
#define RX_GAIN  (0.0)   // 0 is the minimum
//
#define TX_FREQ  (915.5) // in MHz = 1e6 Hz
#define TX_RATE  (1.0)   // millions of samples per sec (bandWidth related)
#define TX_GAIN  (0.0)   // 0 is the minimum
///////////////////////////////////////////////////////////////////////////


// We can add signals to this list that is 0 terminated.  Signals that we
// use to gracefully exit with, that is catch the signal and then set the
// atomic flags and wait for the threads to finish the last loop, if they
// have not already.
//
//   SIGINT is from Ctrl-C in a terminal.
//
static const int exitSignals[] = { SIGINT, 0 };


static pthread_t _mainThread = pthread_self();


// This is a module user interface
//
// Try to gracefully exit.
void crtsExit(void)
{
    errno = 0;
    INFO("Sending signal %d to main thread", exitSignals[0]);
    errno = pthread_kill(_mainThread, exitSignals[0]);
    // All we could do is try and report.
    WARN("Signal %d sent to main thread", exitSignals[0]);
}


// These destructors do nothing, but are required to exist so that modules
// objects may inherit these classes, and deleting objects of these class
// types works.
//
// This is the base class destructor for the TX input modules
CRTSRadioInput::~CRTSRadioInput(void) {DSPEW();}
//
// This is the base class destructor for the RX output modules
CRTSRadioOutput::~CRTSRadioOutput(void) {DSPEW();}
//
// This is the base class destructor for the RX and TX I/O modules
// which is a CRTSRadioInput and a CRTSRadioOutput.
CRTSRadioIO::~CRTSRadioIO(void) {DSPEW();}



static void badSigCatcher(int sig)
{
    ASSERT(0, "caught signal %d", sig);
}


static int usage(const char *argv0, const char *uopt=0)
{
    // Keep this function consistent with the argument parsing:

    if(uopt)
        printf("\n Unknown option: %s\n\n\n", uopt);


    // TODO: Fix the module options example to be a real working example...

    printf(
        "\n"
        "  Usage: %s [OPTIONS]\n"
        "\n"
        "    Run the Cognitive Radio Test System (CRTS) transmitter/receiver program.\n"
        "\n"
        "    This will load only one IO_MODULE module, or one I_MODULE and one O_MODULE.\n"
        "\n"
        "    You may pass argument options to the module constructors by adding arguments that are\n"
        "  just after the module name argument that are encased with opening ' [ ' and closing ' ] '\n"
        "  square brackets, where each enclosed argument is separated by at least one space, and the\n"
        "  added module arguments must be separated from the opening and closing square brackets.\n"
        "  For example:\n"
        "\n"
        "        %s -i stdin [ --buffer-size 100 ]\n"
        "\n"
        "   Note: if you are writing modules the argument count passed to your module does not count\n"
        "   your module name in the argument count, and argv[0] in the stdin module points to\n"
        "   \"--buffer-size\" in this above example.\n"
        "\n"
        "\n"
        "  ----------------------------------------------------------------------------------------\n"
        "                           OPTIONS\n"
        "  ----------------------------------------------------------------------------------------\n"
        "\n"
        "   -A | --uhd-args UHDARGS   set the arguments to give to the uhd::usrp constructor.\n"
        "\n"
        "                                 Example: %s --uhd-args addr=192.168.10.3\n"
        "\n"
        "                             will use the USRP (Universal Software Radio Peripheral)\n"
        "                             which is accessible at Ethernet IP4 address 192.168.10.3\n"
        "\n"
        "   -f | --rx-freq FREQ       set the initial receiver frequency to FREQ MHz.  The default\n"
        "                             initial receiver frequency is %g MHz.\n"
        "\n"
        "   -F | --tx-freq FREQ       set the initial transmission frequency to FREQ MHz.  The\n"
        "                             default initial transmission frequency is %g MHz.\n"
        "\n"
        "   -g | --rx-gain GAIN       set the initial receiver gain to GAIN.  The default initial\n"
        "                             receiver gain is %g.\n"
        "\n"
        "   -G | --tx-gain GAIN       set the initial transmission gain to GAIN.  The default\n"
        "                             initial transmission gain is %g.\n"
        "\n"
        "   -r | --rx-rate RATE       set the initial receiver sample rate to RATE million samples\n"
        "                             per second.  The default initial receiver rate is %g million\n"
        "                             samples per second.\n"
        "\n"
        "   -R | --tx-rate RATE       set the initial transmission sample rate to RATE million samples\n"
        "                             per second.  The default initial transmission rate is %g million\n"
        "                             samples per second.\n"
        "\n"
        "   -h | --help               print this help and exit with status 1\n"
        "\n"
        "   -i | --input I_MODULE     load the radio input module file I_MODULE.  The input read\n"
        "                             will be transmitted using the radio transmitter.  The special\n"
        "                             input module named \"null\" may be used to not read input\n"
        "                             or transmit\n"
        "\n"
        "   -o | --output O_MODULE    load the radio output module file O_MODULE.  The data received\n"
        "                             by the radio receiver will be written to this module.  The\n"
        "                             special output module named \"null\" may be used to not receive\n"
        "                             or write output\n"
        "\n"
        "   -p | --io IO_MODULE       load the radio input and output module file IO_MODULE\n"
        "\n"
        "\n", argv0, argv0, argv0, RX_FREQ, TX_FREQ, RX_GAIN, TX_GAIN, RX_RATE, TX_RATE);

    return 1; // return error status
}



static void signalExitProgramCatcher(int sig)
{
    DSPEW("Caught signal %d", sig);

    // Tell the RX and TX threads to finish.
    isTXLooping = false;
    isRXLooping = false;
}



int main(int argc, const char **argv)
{
    // This will hang the process or thread if we catch the following
    // signals, so we can debug it and see what was wrong if we're
    // lucky.
    ASSERT(signal(SIGSEGV, badSigCatcher) == 0, "");
    ASSERT(signal(SIGABRT, badSigCatcher) == 0, "");
    ASSERT(signal(SIGFPE,  badSigCatcher) == 0, "");

    {
        // Setup the exit signal catcher.
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = signalExitProgramCatcher;
        act.sa_flags = SA_RESETHAND;
        errno = 0;
        for(int i=0; exitSignals[i]; ++i)
            ASSERT(sigaction(exitSignals[i], &act, 0) == 0, "");
    }

    {
        // We must not let the threads created by the UHD API catch the
        // exit signals, so they will inherit the blocking of the exit
        // signals after we set them here in the main thread.
        sigset_t sigSet;
        sigemptyset(&sigSet);
        for(int i=0; exitSignals[i]; ++i)
            sigaddset(&sigSet, exitSignals[i]);
        errno = pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
        ASSERT(errno == 0, "pthread_sigmask() failed");
    }

    int i = 1;
    std::string uhd_args; // default is empty string


    // We collect pointers to argument strings for the plugin modules that
    // we load so we can load a module like it has a constructor that has
    // arguments like Foo::Foo(int argc, const char **argv) hence we have
    // the module name, and the two arguments: argc and argv.
    //
    std::string iMod = "fd0";
    int iModArgc = 0;
    const char **iModArgv = 0;
    //
    std::string oMod = "fd1";
    int oModArgc = 0;
    const char **oModArgv = 0;
    //
    std::string ioMod;
    int ioModArgc = 0;
    const char **ioModArgv = 0;


    // Initial UHD USRP settings will be overridden from the command-line
    // and then may be changed as the program runs:
    double rx_freq = RX_FREQ, rx_rate = RX_RATE, rx_gain = RX_GAIN,
           tx_freq = TX_FREQ, tx_rate = TX_RATE, tx_gain = TX_GAIN;

    /////////////////////////////////////////////////////////////////////
    // Parsing command line arguments
    /////////////////////////////////////////////////////////////////////
    while(i < argc)
    {
        if(get_opt(uhd_args, "-A", "--uhd-args", argc, argv, i))
            continue;
        if(get_opt(iMod, iModArgc, iModArgv, "-i", "--input", argc, argv, i))
            continue;
        if(get_opt(oMod, oModArgc, oModArgv,"-o", "--output", argc, argv, i))
            continue;
        if(get_opt(ioMod, ioModArgc, ioModArgv, "-p", "--io", argc, argv, i))
            continue;

        if(get_opt_double(rx_freq, "-f", "--rx-freq", argc, argv, i))
            continue;
        if(get_opt_double(rx_rate, "-r", "--rx-rate", argc, argv, i))
            continue;
        if(get_opt_double(rx_gain, "-g", "--rx-gain", argc, argv, i))
            continue;
        if(get_opt_double(tx_freq, "-F", "--tx-freq", argc, argv, i))
            continue;
        if(get_opt_double(tx_rate, "-R", "--tx-rate", argc, argv, i))
            continue;
        if(get_opt_double(tx_gain, "-G", "--tx-gain", argc, argv, i))
            continue;

        if(!strcmp("-h", argv[i]) || !strcmp("--help", argv[i]))
            return usage(argv[0]);

        return usage(argv[0], argv[i]);

        //++i;
    }
    DSPEW("uhd_args=\"%s\"", uhd_args.c_str());
    /////////////////////////////////////////////////////////////////////
    // DONE Parsing command line arguments
    /////////////////////////////////////////////////////////////////////


    // The frequencies and rates are given in units of MHz and millions of
    // samples per second, but the UHD API wants them in units of Hz and
    // samples per second, so now we convert them here:
    //
    rx_freq *= 1.0e6; rx_rate *= 1.0e6;
    tx_freq *= 1.0e6; tx_rate *= 1.0e6;


    struct RX rx;
    memset(&rx, 0, sizeof(rx));
    struct TX tx;
    memset(&tx, 0, sizeof(tx));
    CRTSRadioIO *io = 0;
    void *(*destroyInput)(CRTSRadioInput *) = 0;
    void *(*destroyOutput)(CRTSRadioOutput *) = 0;
    void *(*destroyIO)(CRTSRadioIO *) = 0;


    if(ioMod.length() > 0)
    {
        // The I/O module (CRTSRadioIO) provides both read input and write
        // output.

        if(strcmp(ioMod.c_str(), "null") && strcmp(ioMod.c_str(), "null.so"))
        {
            io = LoadModule<CRTSRadioIO>(
                    ioMod.c_str(), "RadioIO"/*module category*/,
                    ioModArgc, ioModArgv, destroyIO);
            if(!io) return 1; // fail

            tx.input = dynamic_cast<CRTSRadioInput *>(io);
            ASSERT(tx.input, "");
            if(!tx.input) return 1; // fail
            rx.output = dynamic_cast<CRTSRadioOutput *>(io);
            ASSERT(rx.output, "");
            if(!rx.output) return 1; // fail
        }
    }
    else
    {
        // We have separated read and write module plugins.

        if(strcmp(iMod.c_str(), "null") &&
                strcmp(iMod.c_str(), "null.so"))
        {
            tx.input = LoadModule<CRTSRadioInput>(
                    iMod.c_str(), "RadioInput"/*module category*/,
                    iModArgc, iModArgv, destroyInput);
            if(!tx.input) return 1; // fail
        }

        if(strcmp(oMod.c_str(), "null") &&
                strcmp(oMod.c_str(), "null.so"))
        {
            rx.output = LoadModule<CRTSRadioOutput>(
                    oMod.c_str(), "RadioOutput"/*module category*/,
                    oModArgc, oModArgv, destroyOutput);
            if(!rx.output) return 1; // fail
        }
    }

    // RANT:
    //
    // It'd be real nice if the UHD API would document what is thread-safe
    // and what is not for all the API.  We can only guess how to use this
    // stupid UHD API by looking at example codes.  The structure of the
    // UHD API implies that you should be able to use a single
    // uhd::usrp::multi_usrp::sptr to do both transmission and receiving
    // but none of the example do that, the examples imply that you must
    // make two uhd::usrp::multi_usrp::sptr objects one for (TX)
    // transmission and one for (RX) receiving.  
    
    // register UHD message handler
    // Let it use stdout, or stderr by default???
    //uhd::msg::register_handler(&uhd_msg_handler);

    // We do not know where UHD is thread safe, so, for now, we do this
    // before we make threads.  The UHD examples do it this way too.
    // We set up the usrp (RX and TX) objects in the main thread here:

    // Testing segfault catcher by setting bad memory address
    // to an int value.
    //void *tret
    //*(int *) (((uintptr_t) tret) + 0xfffffffffffffff8) = 1;


    errno = pthread_barrier_init(&barrier, 0,
                  (rx.output?1:0)+(tx.input?1:0)+1/*3 or 2 or 1 thread barrier*/);
    ASSERT(errno == 0, "pthread_barrier_init(,0,) failed");

    pthread_t rxThread, txThread;


    // UHD BUG WORKAROUND:
    //
    // We must make the two multi_usrp objects before we configure them by
    // setting frequency, rate (bandWidth), and gain; otherwise the
    // process exits with status 0.  And it looks like you can use the
    // same object for both receiving (RX) and transmitting (TX).
    // The UHD API seems to be a piece of shit in general.  Here we
    // are keeping a list of stupid shit it does, and a good API will
    // never do:
    //
    //    - calls exit; instead of throwing an exception
    //
    //    - spawns threads and does not tell you it does in the
    //      documentation
    //
    //    - spews to stdout (we made a work-around for this)
    //
    //    - catches signals
    //
    //
    // It may be libBOOST doing this shit...  so another thing
    // to add to the bad things list:
    //
    //   - links with BOOST
    //
    // We sometimes get
    // Floating point exception
    // and the program exits

    if(rx.output)
        rx.usrp = uhd::usrp::multi_usrp::make(uhd_args);
#if 0
    if(tx.input && rx.output)
        // It looks like we can use one object for both RX and TX
        tx.usrp = rx.usrp;
    else
#endif

#if 0
    {
        if(uhd_args.substr(uhd_args.length()-1) == "2")
        {
            DSPEW("uhd_args.c_str()=%s", uhd_args.c_str());
            uhd_args = "addr=192.168.10.4";
            DSPEW("uhd_args.c_str()=%s", uhd_args.c_str());
        }
        else
        {
            DSPEW("uhd_args.c_str()=%s", uhd_args.c_str());
            uhd_args = "addr=192.168.10.2";
            DSPEW("uhd_args.c_str()=%s", uhd_args.c_str());
        }
    }
#endif

    if(tx.input)
        tx.usrp = uhd::usrp::multi_usrp::make(uhd_args);


    // TODO: Now we can start a cognitive engine thread here or later or
    // before.  It may need to join the BARRIER too.


    if(rx.output)
    {
        ASSERT(rx.usrp, "");
        // TODO: is this next line needed?
        //rx.usrp->set_rx_antenna("RX2", 0);

        crts_usrp_rx_set(rx.usrp, rx_freq, rx_rate, rx_gain);
        DSPEW("rx.usrp->get_pp_string()=\n%s", rx.usrp->get_pp_string().c_str());

        ASSERT((errno = pthread_create(&rxThread, 0,
                    (void* (*)(void*)) RXThreadCallback, &rx)) == 0, "");
        // From here until pthread_join(rxThread,) we cannot read or write
        // to struct tx without great care!
    }
    else
        NOTICE("No (RX) receiving and writing thread");

    if(tx.input)
    {
        ASSERT(tx.usrp, "");
        // TODO: is this next line needed?
        //tx.usrp->set_tx_antenna("TX/RX", 0);

        crts_usrp_tx_set(tx.usrp, tx_freq, tx_rate, tx_gain);
        DSPEW("tx.usrp->get_pp_string()=\n%s", tx.usrp->get_pp_string().c_str());

        ASSERT((errno = pthread_create(&txThread, 0, 
                    (void* (*)(void*)) TXThreadCallback, &tx)) == 0, "");
        // From here until pthread_join(txThread,) we cannot read or write
        // to struct tx without great care!
    }
    else
        NOTICE("No reading and (TX) transmitting thread");


    // TODO: Now we can start a cognitive engine thread here or later or
    // before.  It may need to join the BARRIER too.

    {
        // Now that all the threads are launched we can let this main
        // thread catch the exit signals.
        sigset_t sigSet;
        sigemptyset(&sigSet);
        for(int i=0; exitSignals[i]; ++i)
            sigaddset(&sigSet, exitSignals[i]);
        errno = pthread_sigmask(SIG_UNBLOCK, &sigSet, NULL);
        ASSERT(errno == 0, "pthread_sigmask() failed");
    }

    // TODO: Now we can start a cognitive engine thread here or ...

    // Main loop.
    // TODO: this barrier may not be needed:
    BARRIER_WAIT(&barrier);

    // We are now sure that the RX and TX threads have started, they may
    // not be still running but they have ran or are running now.

    // TODO: Now we can start a cognitive engine thread here or before


    // Now finish up and exit.

    // This will let us knew if there are hung threads.  Just returning
    // (like calling exit()) will cause all threads to terminate, not
    // letting you know if anything bad happened.

    // Wait for threads to return.

    if(rx.output)
        ASSERT((errno = pthread_join(rxThread, 0)) == 0, "");

    if(tx.input)
        ASSERT((errno = pthread_join(txThread, 0)) == 0, "");


    // Cleanup

    if(destroyInput)
    {
        DSPEW();
        DASSERT(destroyIO == 0, "");
        destroyInput(tx.input);
    }

    if(destroyOutput)
    {
        DSPEW();
        DASSERT(destroyIO == 0, "");
        destroyOutput(rx.output);
    }

    if(destroyIO)
    {
        DSPEW();
        DASSERT(destroyInput == 0 && destroyOutput == 0, "");
        destroyIO(io);
    }

    // TODO: How do I cleanup the usrp objects?
    //
    // UHD is a half-ass API: their main objects do not have a cleanup
    // destructor.  Their examples do not cleanup the usrp objects.
    //
    //delete tx.usrp;
    //delete rx.usrp;
    DSPEW("FINISHED");

    return 0;
}

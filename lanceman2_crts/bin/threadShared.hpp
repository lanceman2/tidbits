// This file contains only variables shared between the files:
//
//    rxThread.cpp,
//
//    txThread.cpp, 
//
//    ceThread.cpp (TODO), and
//
//    crts_radio.cpp (main).
//
// So we can see and understand what data needs thread protection.
// Keep in mind there are variables passed in the stack in the
// pthread_create() calls in crts_radio.cpp.
//
// Also we declare some common interfaces, that are used in the above file
// list, in here.
//



// Looks like lib FFTW-3 (libfftw3f.so.3) is not thread safe in general.
//
//  See      http://www.fftw.org/fftw3_doc/Thread-safety.html
// 
// Looks like the transit fftw functions like functions that create,
// initialize and destroy are the functions that are not thread safe.
//
// The ofdmflexframe functions use lib FFTW-3.   This is a stab in the
// dark at making the ofdmflexframe create and destroy functions thread
// safe by wrapping them with mutex lock and unlock calls.  Having this
// below seems to fix the memory corruption problem I was seeing more than
// 1 in 10 runs.  Testing shows this looks like a good fix...
extern pthread_mutex_t fftw3_mutex;
#  define FFTW_LOCK()     MUTEX_LOCK(&fftw3_mutex)
#  define FFTW_UNLOCK()   MUTEX_UNLOCK(&fftw3_mutex)


extern pthread_barrier_t barrier;


// The read then transmit (TX) loop flag.
extern std::atomic<bool> isTXLooping;

/// The receive (RX) then write loop flags.
extern std::atomic<bool> isRXLooping;


// For crts_radio.cpp to call; declared in txThread.cpp
extern void *TXThreadCallback(struct TX *tx);


// For crts_radio.cpp to call; declared in rxThread.cpp
extern void *RXThreadCallback(struct RX *rx);



////////////////////////////////////////////////////////////////////////
// No variables just interface declarations below here
////////////////////////////////////////////////////////////////////////

// TODO: Why the hell do we need to pass the usrp object to the rx and tx
// threads; it would be better to have the threads make those objects.  It
// turns out that they are the same object, because the UHD library is
// kind-of a pile of shit.  We tried many variations and this seem to
// crash the least.  I expect that the UHD library is loaded with race
// conditions.  I'd expect we must be careful if we ever call rx methods
// in the tx thread and vise versa (rx -> tx, tx -> rx).


struct TX { // passed to tx thread

    uhd::usrp::multi_usrp::sptr usrp;

    CRTSRadioInput *input;
};


struct RX { // passed to rx thread

    uhd::usrp::multi_usrp::sptr usrp;

    uint64_t totalBytesRecv;
    
    CRTSRadioOutput *output;
};

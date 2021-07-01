#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <inttypes.h>
#include <string>
#include <atomic>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <uhd/usrp/multi_usrp.hpp>

#include "liquid.h"

#include "crts/RadioInput.hpp"
#include "crts/RadioOutput.hpp"
#include "crts/RadioIO.hpp"

#include "debug.h"
#include "pthread_wrappers.h" // some pthread_*() wrappers
#include "threadShared.hpp"

#include "timer.hpp"


// The read then transmit (TX) loop flag.
// Note: the isTXLooping variable is shared between threads hence we
// define it as atomic for easy thread safe access.
// isTXLooping is shared in threadShared.hpp
std::atomic<bool> isTXLooping(true);


// ofdmflexframe parameters for TX
static const unsigned int numSubcarriers = 32;
static const unsigned int cp_len = 16;
static const unsigned int taper_len = 4;


// TODO: check for error here:
static inline void usrpSend(
        uhd::device::sptr device,
        const void *buffer,
        size_t len,
        uhd::tx_metadata_t &metadata)
{
    device->send(buffer, len, metadata,
            uhd::io_type_t::COMPLEX_FLOAT32,
            uhd::device::SEND_MODE_FULL_BUFF);
}


// Returns true to keep looping
static inline bool TXReadTransmitLoop(struct TX * &tx,
        uhd::device::sptr &device, uhd::tx_metadata_t &metadata_tx,
        ofdmflexframegen &fg, std::complex<float> * fg_buffer, const size_t &fg_bufferLen,
        float &tx_gain_soft, unsigned char * buffer, const size_t &bufferLen,
        uint64_t &tx_header, uint64_t &totalBytesRead)
{
    // The read() may block if there is no data ready and returning 0
    // meets it is done, and returning less than 0 means that we are
    // done with an error.
    ssize_t nread = tx->input->read(buffer, bufferLen);
    if(nread <= 0)
    {
        if(nread < 0)
            WARN("CRTSRadioInput read error");
        else
            // nread == 0
            INFO("CRTSRadioInput done reading");
        return false;
    }
    //DSPEW("read (%zd):", nread);

    totalBytesRead += nread;

    tx_header += 1; // frame counter

    // assemble frame using liquid-dsp
    // Enter the raw data that we just read:
    ofdmflexframegen_assemble(
            fg, (unsigned char *) &tx_header, buffer, nread);

    bool last_symbol = false;
    unsigned int i;
    while (!last_symbol)
    {
        // generate symbol
        last_symbol = ofdmflexframegen_write(fg,
                fg_buffer, fg_bufferLen);

        // Apply the gain
        for (i=0; i<fg_bufferLen; ++i)
            fg_buffer[i] *= tx_gain_soft;

        usrpSend(device, fg_buffer, fg_bufferLen, metadata_tx);
    }

    // send a few extra samples to the device
    // NOTE: this seems necessary to preserve last OFDM symbol in
    //       frame from corruption (from old CRTS)
    usrpSend(device, fg_buffer, fg_bufferLen, metadata_tx);

    return true;
}


void *TXThreadCallback(struct TX *tx)
{
    DSPEW("Starting TX thread...");

    unsigned char subcarrierAlloc[numSubcarriers];
    ofdmframe_init_default_sctype(numSubcarriers, subcarrierAlloc);
 
    ofdmflexframegenprops_s fgprops; // frame generator properties
    ofdmflexframegenprops_init_default(&fgprops);
    fgprops.check = LIQUID_CRC_32;
    fgprops.fec0 = LIQUID_FEC_HAMMING128;
    fgprops.fec1 = LIQUID_FEC_NONE;
    fgprops.mod_scheme = LIQUID_MODEM_QAM4;

    FFTW_LOCK();
    ofdmflexframegen fg =
        ofdmflexframegen_create(numSubcarriers, cp_len,
                taper_len, subcarrierAlloc, &fgprops);
    FFTW_UNLOCK();
    // fg is a pointer to malloc() memory  ofdmflexframegen_create()
    // never checks it, likely we'd segfault before now if it failed.
    ASSERT(fg, "");

    uint64_t tx_header = 0;

    unsigned int fg_bufferLen = numSubcarriers + cp_len;
    std::complex<float> fg_buffer[fg_bufferLen];

    const size_t bufferLen = 1024;
    unsigned char buffer[bufferLen];

    uint64_t totalBytesRead = 0;

    float tx_gain_soft = powf(10.0F, -12.0F/*gain_soft*// 20.0F);

    //struct timespec t { 0, 40000000/*nano seconds*/ };

    // set up the metadata flags
    uhd::tx_metadata_t metadata_tx;

    metadata_tx.start_of_burst = true; // never SOB when continuous
    metadata_tx.end_of_burst = false;  //
    metadata_tx.has_time_spec = false; // set to false to send immediately

    uhd::device::sptr device = tx->usrp->get_device();

    BARRIER_WAIT(&barrier);
    DSPEW("TX Looping");

    CRTSTimer t;


    if(isTXLooping &&
            TXReadTransmitLoop(tx, device, metadata_tx,
                    fg, fg_buffer, fg_bufferLen,
                    tx_gain_soft, buffer, bufferLen,
                    tx_header, totalBytesRead))
    {
        // never SOB when continuous
        metadata_tx.start_of_burst = false;

        while(isTXLooping &&
                TXReadTransmitLoop(tx, device, metadata_tx,
                        fg, fg_buffer, fg_bufferLen,
                        tx_gain_soft, buffer, bufferLen,
                        tx_header, totalBytesRead))
            /*empty while() block*/;
    }

    if(totalBytesRead > 0)
    {
        // send a mini EOB packet
        metadata_tx.end_of_burst = true;
        usrpSend(device, "", 0, metadata_tx);
    }

    double dt = t.GetDouble();
    SPEW("TX thread read and transmitted %" PRIu64
            " bytes in %g seconds ->  %g K bytes/second",
            totalBytesRead, dt, totalBytesRead/(1024 *dt));

    FFTW_LOCK();
    ofdmflexframegen_destroy(fg);
    FFTW_UNLOCK();

    DSPEW("TX thread returning");
    return 0;
}

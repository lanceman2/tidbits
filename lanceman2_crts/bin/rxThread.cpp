#include <stdio.h>
#include <pthread.h>
#include <time.h>
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
#include "timer.hpp"
#include "pthread_wrappers.h" // some pthread_*() wrappers
#include "threadShared.hpp"



/// The receive (RX) then write loop flags.
// Note: the isRXLooping variable is shared between threads hence we
// define it as atomic for easy thread safe access.
// isRXLooping is shared in threadShared.hpp
std::atomic<bool> isRXLooping(true);


// ofdmflexframe parameters for RX
static const unsigned int numSubcarriers = 32;
static const unsigned int cp_len = 16;
static const unsigned int taper_len = 4;


// TODO: pass in just the write function???
//
static int rxFrameSyncCallback(unsigned char *header, int header_valid,
               unsigned char *payload, unsigned int payload_len,
               int payload_valid, framesyncstats_s stats, struct RX *rx) 
{
    //DSPEW("payload_valid=%d  header[0]=%d  len=%u", payload_valid, header[0], payload_len);

    if(header_valid)
    {
        rx->totalBytesRecv += payload_len;

        // TODO: remove this print and add an error checker!!!!!!!!
        DSPEW("recieved header sequence=%" PRIu64 , *((uint64_t *) header));

        DASSERT(payload_len > 0, "");
        ssize_t nwrite = rx->output->write(payload, payload_len);
        DASSERT(nwrite > 0, "");
        DASSERT((unsigned int) nwrite == payload_len, "");
    }

    return 0;
}


static inline bool RXRecvWriteLoop(struct RX *rx,
        uhd::device::sptr device, ofdmflexframesync fs,
        std::complex<float> *buffer, size_t numComplexFloats)
{
    uhd::rx_metadata_t metadata; // set by recv();

    size_t numSamples = device->recv(
            (unsigned char *)buffer, numComplexFloats, metadata,
            uhd::io_type_t::COMPLEX_FLOAT32,
            uhd::device::RECV_MODE_ONE_PACKET,
            // TODO: fix this timeout ??
            1.0/*timeout double seconds*/);

    if(numSamples != numComplexFloats)
        DSPEW("RX recv metadata.error_code=%d numSamples = %zu",
            metadata.error_code, numSamples);

    if(metadata.error_code && metadata.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
    {    
        DSPEW("RX recv metadata.error_code=%d numSamples = %zu",
            metadata.error_code, numSamples);
        // For error codes see:
        // https://files.ettus.com/manual/structuhd_1_1rx__metadata__t.html#ae3a42ad2414c4f44119157693fe27639
        DSPEW("uhd::rx_metadata_t::ERROR_CODE_NONE=%d",
            uhd::rx_metadata_t::ERROR_CODE_NONE);
        DSPEW("uhd::rx_metadata_t::ERROR_CODE_TIMEOUT=%d",
            uhd::rx_metadata_t::ERROR_CODE_TIMEOUT);
    }

    DASSERT(!(metadata.error_code && numSamples), "");

    if(numSamples)
        ofdmflexframesync_execute(fs, buffer, numSamples);

    return true; // repeat
}


void *RXThreadCallback(struct RX *rx)
{
    DSPEW("Starting RX thread...");

    unsigned char subcarrierAlloc[numSubcarriers];
    ofdmframe_init_default_sctype(numSubcarriers, subcarrierAlloc);

    FFTW_LOCK();
    ofdmflexframesync fs =
        ofdmflexframesync_create(numSubcarriers, cp_len,
                taper_len, subcarrierAlloc,
                (framesync_callback) rxFrameSyncCallback, rx/*callback data*/);
    FFTW_UNLOCK();
    DSPEW();
    ASSERT(fs, "");

    uhd::device::sptr device = rx->usrp->get_device();

    const size_t numComplexFloats = device->get_max_recv_samps_per_packet();
    DSPEW("RX numComplexFloats = %zu", numComplexFloats);
    std::complex<float> buffer[numComplexFloats];

    BARRIER_WAIT(&barrier);
    DSPEW("RX Looping");

    CRTSTimer t;

    //setup streaming. Whatever that means.
    // TODO: What does rx->usrp->issue_stream_cmd() return:
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);

    if(isRXLooping)
        rx->usrp->issue_stream_cmd(stream_cmd);

    while(isRXLooping &&
            RXRecvWriteLoop(rx, device, fs,
                buffer, numComplexFloats))
                /*empty while() block*/;


    // TODO: What does this return:
    rx->usrp->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);

    double dt = t.GetDouble();
    SPEW("RX thread received and wrote %" PRIu64
            " bytes in %g seconds ->  %g K bytes/second",
            rx->totalBytesRecv, dt, rx->totalBytesRecv/(1024 *dt));

    FFTW_LOCK();
    ofdmflexframesync_destroy(fs);
    FFTW_UNLOCK();


    DSPEW("RX thread returning");
    return 0;
}

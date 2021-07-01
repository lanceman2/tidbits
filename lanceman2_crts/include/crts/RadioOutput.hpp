#ifndef __CRTS_RadioOutput__H__
#define __CRTS_RadioOutput__H__

#include <crts/MakeModule.hpp>


// This is the base class to a CRTS radio output module

class CRTSRadioOutput
{
    public:

        virtual ~CRTSRadioOutput(void);

        // This write can block if it needs to.
        virtual ssize_t write(const void *buffer, size_t bufferLen) = 0;
};



// CPP Macro Module Loader Function
//
// See MakeModule.hpp for details.
//
#define CRTSRadioOutput_MAKE_MODULE(derived_class_name) \
    CRTS_MAKE_MODULE(CRTSRadioOutput, derived_class_name)

#endif // #ifndef __CRTS_RadioOutput__H__

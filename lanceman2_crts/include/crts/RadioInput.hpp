#ifndef __CRTS_RadioInput__H__
#define __CRTS_RadioInput__H__

#include <crts/MakeModule.hpp>


// This is the base class to a CRTS radio input module

class CRTSRadioInput
{
    public:

        virtual ~CRTSRadioInput(void);

        // This read can block until data is available.
        virtual ssize_t read(void *buffer, size_t bufferLen) = 0;
};


// CPP Macro Module Loader Function
//
// See MakeModule.hpp for details.
//
#define CRTSRadioInput_MAKE_MODULE(derived_class_name) \
    CRTS_MAKE_MODULE(CRTSRadioInput, derived_class_name)

#endif // #ifndef __CRTS_RadioInput__H__

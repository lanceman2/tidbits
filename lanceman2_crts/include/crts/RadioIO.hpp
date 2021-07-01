#ifndef __CRTS_RadioIO__H__
#define __CRTS_RadioIO__H__

#include <crts/MakeModule.hpp>
#include <crts/RadioInput.hpp>
#include <crts/RadioOutput.hpp>


// This is the base class to a CRTS radio input/output module

class CRTSRadioIO : public CRTSRadioInput, public CRTSRadioOutput
{
    public:

        virtual ~CRTSRadioIO(void);
};



// CPP Macro Module Loader Function
//
// See MakeModule.hpp for details.
//
#define CRTSRadioIO_MAKE_MODULE(derived_class_name) \
    CRTS_MAKE_MODULE(CRTSRadioIO, derived_class_name)

#endif // #ifndef __CRTS_RadioIO__H__

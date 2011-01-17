#include "mitkPointSetInteractor.h"
#include "mitkCSwigMacros.h"

#ifdef CABLE_CONFIGURATION

namespace _cable_
{
     const char* const group="PointSetInteractor";
     namespace wrappers
     {
         MITK_WRAP_OBJECT(PointSetInteractor)
     }
}

#endif


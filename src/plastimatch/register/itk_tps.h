/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _itk_tps_h_
#define _itk_tps_h_

#include "plmregister_config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "itkImage.h"

/// This workaround prevents a compilation fail with ITK-5.3
/// Something defines POSIX in preprocessor, and it conflicts
/// with ITK KWSys POSIX enumeration constant
#if (ITK_VERSION_MAJOR == 5) && (ITK_VERSION_MINOR > 2) && defined(POSIX)
#define PLMPOSIX_TMP (POSIX)
#undef POSIX
#endif

#include "itkImageFileReader.h"

#ifdef PLMPOSIX_TMP
#define POSIX (PLMPOSIX_TMP)
#undef PLMPOSIX_TMP
#endif

#include "itkImageFileWriter.h"
#include "itkPoint.h"
#include "itkPointSet.h"
#include "itkThinPlateSplineKernelTransform.h"


class Landmark_warp;

class TPS_parms {
public:
    char* reference;
    char* target;
    char* fixed;
    char* moving;
    char* warped;
    char* vf;
};

template<class T>
PLMREGISTER_API void do_tps (TPS_parms* parms,typename itk::Image<T,3>::Pointer img_fixed, typename itk::Image<T,3>::Pointer img_moving,T);
PLMREGISTER_API void itk_tps_warp (Landmark_warp *lw);

#endif

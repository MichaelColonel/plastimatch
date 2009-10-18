/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef __plm_config_h__
#define __plm_config_h__

#define PLASTIMATCH_BUILD_NUMBER "@PLASTIMATCH_SVN_VERSION@"

#cmakedefine HAVE_SYS_STAT 1
#cmakedefine HAVE_BROOK 1
#cmakedefine HAVE_CUDA 1
#cmakedefine FORTRAN_COMPILER_FOUND 1
#cmakedefine HAVE_F2C_LIBRARY 1
#cmakedefine HAVE_GETOPT_LONG 1
#cmakedefine HAVE_PANTHEIOS 1
#cmakedefine HAVE_FFTW 1

#cmakedefine PLM_BUILD_SHARED_LIBS 1
#cmakedefine OPENMP_FOUND 1
#cmakedefine BUILD_BSPLINE_BROOK 1
#cmakedefine DCMTK_HAVE_CONFIG_H 1

#define CMAKE_SIZEOF_UINT @CMAKE_SIZEOF_UINT@
#define CMAKE_SIZEOF_ULONG @CMAKE_SIZEOF_ULONG@

#cmakedefine PLASTIMATCH_EXPERIMENTAL 1

#cmakedefine HAVE_ITK 1
#if (HAVE_ITK && !PLM_CUDA_COMPILE)
#include "itkConfigure.h"
#if (ITK_VERSION_MAJOR >=3) && (ITK_VERSION_MINOR >= 10) && defined (ITK_USE_ORIENTED_IMAGE_DIRECTION)
#define PLM_ITK_ORIENTED_IMAGES 1
#endif
#endif

/* Make Microsoft compiler less whiny */
#if _MSC_VER >= 1400
/* 4244 warnings == ? */
/* 4819 warnings generated by itk headers */
/* 4996 warnings generated when using ISO C library functions 
   (i.e. not using MSVC extensions) */
#pragma warning( disable : 4996 4244 4819 )
#endif

/* Make GCC compiler less whiny */
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 2)
/* -Wwrite-strings warnings generated when passing literal strings as 
   (non-const?) function parameters */
#pragma GCC diagnostic ignored "-Wwrite-strings"
/* -Wdepricated warnings generated by itk headers */
#pragma GCC diagnostic ignored "-Wdeprecated"
/* -Wno-sign-conversion warnings are generated when comparing signed 
   and unsigned integers -- these should be revisited occasionally 
   to check for actual bugs. */
#pragma GCC diagnostic ignored "-Wno-sign-conversion"
#endif

#if _MSC_VER
#define inline __inline
#endif

#if defined(__BORLANDC__) || defined(_MSC_VER)
#define snprintf _snprintf
#endif

/* This code is for exporting symbols when building DLLs on windows */
#if (defined(_WIN32) || defined(WIN32)) && (defined (BUILD_SHARED_LIBS) || defined (PLM_BUILD_SHARED_LIBS))
# ifdef plastimatch1_EXPORTS
#  define plastimatch1_EXPORT __declspec(dllexport)
# else
#  define plastimatch1_EXPORT __declspec(dllimport)
# endif
# ifdef gpuit_EXPORTS
#  define gpuit_EXPORT __declspec(dllexport)
# else
#  define gpuit_EXPORT __declspec(dllimport)
# endif
#else
/* unix needs nothing */
#define plastimatch1_EXPORT 
#define gpuit_EXPORT 
#endif

#endif /* __plm_config_h__ */

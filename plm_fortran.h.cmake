/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _plm_fortran_h_
#define _plm_fortran_h_

#include "plm_config.h"

#if defined (_WIN32)
  #include "plm_f2c_win32.h"

#else /* UNIX */

  #if defined (HAVE_F2C_LIBRARY)
     /* If you try to compile and link against a self-built f2c library 
        without installing, you can't include the f2c directory because 
        f2c includes a broken "ctype.h" which conflicts with the system one. 
	Therefore, we prefer to include the system f2c.h if it exists.
	*/
    #include "@F2C_INCLUDE_DIR@/f2c.h"
  #else
    /* If there is no system built f2c, we still need a mapping 
       from integer to int, etc.  Just include the 32-bit windows 
       version, and hope for the best.
       */
    #include "plm_f2c_win32.h"
  #endif
#endif

#if (FORTRAN_COMPILER_FOUND || HAVE_F2C_LIBRARY)
  #define FORTRAN_FOUND 1
#endif

#endif

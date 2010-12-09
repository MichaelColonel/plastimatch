/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include "delayload.h"
#include <stdio.h>
#include <string.h>
#if defined (_WIN32)
#include <windows.h>	// for LoadLibrary()
#include <direct.h>     // for _getcwd()
#define get_cwd _getcwd
#else
#include <dlfcn.h>      // for dlopen()
#include <unistd.h>     // for getcwd()
#define get_cwd getcwd
#endif

/* Note:
 * Any utility that plans to use GPU functions should call the proper
 * delayload_X function to check if the CUDA or OpenCL runtimes are available.
 *
 * Return 1 if runtime found, 0 if runtime not found.
 */


// This makes things read easier and also
// gives me an entry point later if I need
// to do something fancy.
int find_lib (char* lib)
{
#if defined (_WIN32)
    if (LoadLibrary (lib) != NULL) {
        return 1;   /* Success */
    } else {
        return 0;   /* Failure */
    }
#else
    if (dlopen_ex (lib) != NULL) {
        return 1;   /* Success */
    } else {
        return 0;   /* Failure */
    }
#endif
}


// This is a small helper function that will search around
// for the desired libraries... this is to help out users
// that don't configure their system paths correctly.
// Checks:
//   * 1st: System Path / ld cache / etc
//   * 2nd: Current Working Directory
//   * 3rd: Default CUDA lib paths
//
//   Note: For Windows we ONLY check the current
//         path since we are at the mercy of the
//         link.exe /DELAYLOAD switch.  Libraries
//         for Windows must, therefore, be found
//         at compile time... which makes the
//         Windows aspect of this a job for CMake.
#if !defined (_WIN32)
void* dlopen_ex (char* lib)
{
    char cwd[FILENAME_MAX];
    char cudalib32[FILENAME_MAX];
    char cudalib64[FILENAME_MAX];
    char testing[8];

    /* current working directory */
    if (!get_cwd(cwd, sizeof(cwd))) {
        cwd[0] = '\0';
    }

    strcat (cwd, "/");
    strcat (cwd, lib);
    cwd[FILENAME_MAX - 1] = '\0';

    /* default cuda lib paths */
    strcpy (cudalib32, "/usr/local/cuda/lib/");
    strcat (cudalib32, lib);
    strcpy (cudalib64, "/usr/local/cuda/lib64/");
    strcat (cudalib64, lib);

    /* Search in order */
    if (dlopen (lib, RTLD_LAZY) != NULL) {
        return dlopen (lib, RTLD_LAZY);
    }
    else if (dlopen (cwd, RTLD_LAZY) != NULL){
        return dlopen (cwd, RTLD_LAZY);
    }
    else if (dlopen (cudalib32, RTLD_LAZY) != NULL){
        return dlopen (cudalib32, RTLD_LAZY);
    }
    else if (dlopen (cudalib64, RTLD_LAZY) != NULL){
        return dlopen (cudalib64, RTLD_LAZY);
    }
    else {
        return NULL;   /* Failure */
    }
    
}
#endif


// Note: We need special cases for Windows and POSIX compliant OSes
int 
delayload_cuda (void)
{
#if defined (_WIN32)
    // Windows
    // For Windows we try to load the CUDA drivers:
    // * If they don't exist -> we should exit upon returning from this function.
    // * If they do exist    -> windows has been told to delay load plmcuda.dll
    //                          when a cuda function is encountered, we will
    //                          load plmcuda.dll automagically, which we know
    //                          will work as intended since we have 1st checked
    //                          that the cuda drivers are installed on the
    //                          users system (nvcuda.dll).  (See also
    //                          CMakeLists.txt for /DELAYLOAD configuration)

    // Because good ol' Windows can't do symlinks, there is no version safe
    // way to check for the runtime... the name of the file changes with every
    // new version release of the CUDA Toolkit.  Users will just need to read
    // the documentation and install the version of the toolkit that was used
    // to build the plastimatch CUDA plugin (plmcuda.dll) OR compile from
    // source.
    if (   !find_lib ("nvcuda.dll")     /* CUDA Driver */
#if defined (PLM_USE_GPU_PLUGINS)
        || !find_lib ("plmcuda.dll")    /* PLM CUDA Plugin */
#endif
       ) {
        printf ("Failed to load CUDA runtime!\n");
        printf ("For GPU acceleration, please install:\n");
        printf ("* the plastimatch GPU plugin\n");
        printf ("* the CUDA Toolkit version needed by the GPU plugin\n\n");
        printf ("Visit http://www.plastimatch.org/contents.html for more information.\n\n");
        return 0;
    } else {
        // success
        return 1;
    }
#else
    // NOT Windows (most likely a POSIX compliant OS though...)
    //
    // Check for *both* the CUDA runtime & CUDA driver
    //
    // I think this is version safe due to the way nvidia does symlinks.
    // For example, on my system:
    //
    // libcuda.so -> libcuda.so.195.36.24
    // *and*
    // libcudart.so -> libcudart.so.3
    // libcudart.so.3 -> libcudart.so.3.0.14
    //
    if (    !find_lib ("libcuda.so")         /* CUDA Driver */
         || !find_lib ("libcudart.so")       /* CUDA RunTime */
#if defined (PLM_USE_GPU_PLUGINS)
         || !find_lib ("libplmcuda.so")      /* PLM CUDA Plugin */
#endif
       ) {
        printf ("Failed to load CUDA runtime!\n");
        printf ("For GPU acceleration, please install:\n");
        printf ("* the plastimatch GPU plugin\n");
        printf ("* the CUDA Toolkit version needed by the GPU plugin\n\n");
        printf ("Visit http://www.plastimatch.org/contents.html for more information.\n\n");
        return 0;
    } else {
        // success
        return 1;
    }
#endif
}

int 
delayload_opencl (void)
{
#if defined (_WIN32)
    if (LoadLibrary ("opencl.dll") == NULL) {
        printf ("opencl.dll not found!  Please install an OpenCL runtime.\n");
        return 0;
    } else {
        return 1;
    }
#else
    // Assume linux users are compiling from source
    // and won't attempt to run features they don't
    // or can't utilize.
    return 1;
#endif
}

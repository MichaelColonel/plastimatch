/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _FDK_OPENCL_H_
#define _FDK_OPENCL_H_

#include "plm_config.h"
#include "plmbase.h"
#include "fdk_opts.h"
#include "proj_image_dir.h"
#include "delayload.h"

#if defined __cplusplus
extern "C" {
#endif

gpuit_EXPORT
void opencl_reconstruct_conebeam (
    Volume *vol, 
    Proj_image_dir *proj_dir, 
    Fdk_options *options
);

#if defined __cplusplus
}
#endif

#endif

/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _gdcm_rtss_h_
#define _gdcm_rtss_h_

#include "plm_config.h"
#include "readcxt.h"

#if defined __cplusplus
extern "C" {
#endif

plastimatch1_EXPORT
void
gdcm_rtss_load (Cxt_structure_list *structures, char *rtss_fn, char *dicom_dir);
plastimatch1_EXPORT
void
gdcm_rtss_save (Cxt_structure_list *structures, char *rtss_fn);

#if defined __cplusplus
}
#endif

#endif

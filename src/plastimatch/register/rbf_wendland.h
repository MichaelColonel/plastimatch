/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _rbf_wendland_h_
#define _rbf_wendland_h_

#include "plmregister_config.h"
#include "landmark_warp.h"

#if defined __cplusplus
extern "C" {
#endif

plastimatch1_EXPORT
void bspline_rbf_wendland_find_coeffs (Volume *vector_field, Bspline_parms *parms);

plastimatch1_EXPORT
void bspline_rbf_wendland_update_vector_field (Volume *vector_field, Bspline_parms *parms);

plastimatch1_EXPORT
void
rbf_wendland_warp (Landmark_warp *lw);

#if defined __cplusplus
}
#endif

#endif

/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _resample_image_h_
#define _resample_image_h_

#include "plmbase_config.h"
#include "plm_image.h"
#include "plm_image_header.h"

template <class T, class U>
T
vector_resample_image (T& vf_image, U& ref_image);
template <class T>
T
vector_resample_image (T& image, float x_spacing,
			float y_spacing, float z_spacing);
template <class T>
T
vector_resample_image (T& vf_image, Plm_image_header* pih);

template <class T>
T
resample_image (T& image, Plm_image_header* pih, float default_val, int interp_lin);

template <class T>
T
subsample_image (T& image, int x_sampling_rate,
	        int y_sampling_rate, int z_sampling_rate,
		float default_val);

#endif
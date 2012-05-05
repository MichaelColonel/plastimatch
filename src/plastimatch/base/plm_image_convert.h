/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _itk_image_convert_h_
#define _itk_image_convert_h_

#include "plmbase_config.h"

class Plm_image;

template<class T, class U> API T plm_image_convert_gpuit_to_itk (
        Plm_image* pli,
        T itk_img, U
);
template<class T, class U> API void plm_image_convert_itk_to_gpuit (
        Plm_image* pli,
        T img,
        U
);
template<class T> API void plm_image_convert_itk_to_gpuit_float (
        Plm_image* pli,
        T img
);

API UCharVecImageType::Pointer
plm_image_convert_itk_uchar_to_itk_uchar_vec (UCharImageType::Pointer im_in);

API UCharVecImageType::Pointer 
plm_image_convert_itk_uint32_to_itk_uchar_vec (UInt32ImageType::Pointer im_in);

API UCharVecImageType::Pointer
plm_image_convert_gpuit_uint32_to_itk_uchar_vec (Plm_image* pli);

API UCharVecImageType::Pointer
plm_image_convert_gpuit_uchar_vec_to_itk_uchar_vec (Plm_image* pli);

API void
plm_image_convert_itk_uchar_vec_to_gpuit_uchar_vec (Plm_image* pli, 
    UCharVecImageType::Pointer itk_img);

#endif

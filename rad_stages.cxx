/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "rad_registration.h"
#include "rad_image.h"
#include "itk_registration.h"
#include "itk_optim.h"
#include "resample_mha.h"
#include "itk_warp.h"
#include "itk_demons.h"
#include "gpuit_bspline.h"
#include "gpuit_demons.h"
#include "xform.h"


static RadImage::RadImageType
choose_image_type (int xform_type, int optim_type, int impl_type)
{
    switch (impl_type) {
	case IMPLEMENTATION_GPUIT_CPU:
	case IMPLEMENTATION_GPUIT_BROOK:
	    return RadImage::TYPE_GPUIT_FLOAT;
	default:
	    return RadImage::TYPE_ITK_FLOAT;
    }
}

void
save_warped_img (Registration_Data* regd,
		 DeformationFieldType::Pointer vf,
		 char* fn)
{
    FloatImageType::Pointer im_warped = FloatImageType::New();

    printf ("Converting image 1...\n");
    FloatImageType::Pointer i1 = regd->moving_image->itk_float();
    printf ("Converting image 2...\n");
    FloatImageType::Pointer i2 = regd->fixed_image->itk_float();

    printf ("Warping image...\n");
    im_warped = itk_warp_image (regd->moving_image->itk_float(), 
		    regd->fixed_image->itk_float(), vf, 0.0);
    printf ("Saving image...\n");
    save_short (im_warped, fn);
}

void
save_stage_output (Registration_Data* regd, Xform *xf_out, Stage_Parms* stage)
{
    Xform xf_tmp;

    if (stage->img_out_fn[0] || stage->vf_out_fn[0]) {
	/* Convert xform to vf */
	printf ("Converting xf to vector field ...\n");
	xform_to_itk_vf (&xf_tmp, xf_out, regd->fixed_image->itk_float());
	/* Save warped image */
	if (stage->img_out_fn[0]) {
	    printf ("Saving warped image ...\n");
	    save_warped_img (regd, xf_tmp.get_itk_vf(), stage->img_out_fn);
	}
	/* Save deformation field */
	if (stage->vf_out_fn[0]) {
	    printf ("Writing vector field ...\n");
	    save_image (xf_tmp.get_itk_vf(), stage->vf_out_fn);
	}
    }

    if (stage->xf_out_fn[0]) {
	printf ("Writing deformation parameters ...\n");
	save_xform (xf_out, stage->xf_out_fn);
    }
}

void
save_regp_output (Registration_Data* regd, Xform *xf_out, Registration_Parms* regp)
{
    Xform xf_tmp;

    if (regp->img_out_fn[0] || regp->vf_out_fn[0]) {
	/* Convert xform to vf */
	printf ("Converting xf to vector field ...\n");
	xform_to_itk_vf (&xf_tmp, xf_out, regd->fixed_image->itk_float());

	/* Save warped image */
	if (regp->img_out_fn[0]) {
	    save_warped_img (regd, xf_tmp.get_itk_vf(), regp->img_out_fn);
	}
	/* Save deformation field */
	if (regp->vf_out_fn[0]) {
	    printf ("Writing vector field ...\n");
	    save_image (xf_tmp.get_itk_vf(), regp->vf_out_fn);
	}
    }

    if (regp->xf_out_fn[0]) {
	printf ("Writing deformation parameters ...\n");
	save_xform (xf_out, regp->xf_out_fn);
    }
}

void
do_registration_stage (Registration_Data* regd, Xform *xf_out, Xform *xf_in, 
		       Stage_Parms* stage)
{
    /* Convert image types */
    RadImage::RadImageType image_type = choose_image_type (stage->xform_type, stage->optim_type, stage->impl_type);

    printf ("xf_in->m_type = %d, xf_out->m_type = %d\n", xf_in->m_type, xf_out->m_type);

    /* Run registration */
    if (stage->optim_type == OPTIMIZATION_DEMONS) {
	if (stage->impl_type == IMPLEMENTATION_ITK) {
	    do_demons_stage (regd, xf_out, xf_in, stage);
	} else {
	    do_gpuit_demons_stage (regd, xf_out, xf_in, stage);
	}
    }
    else if (stage->xform_type == STAGE_TRANSFORM_BSPLINE) {
	if (stage->impl_type == IMPLEMENTATION_ITK) {
	    do_itk_stage (regd, xf_out, xf_in, stage);
	} else {
	    do_gpuit_bspline_stage (regd, xf_out, xf_in, stage);
	}
    }
    else {
	do_itk_stage (regd, xf_out, xf_in, stage);
    }


    printf ("xf_out->m_type = %d, xf_in->m_type = %d\n", xf_out->m_type, xf_in->m_type);

    /* Save intermediate output */
    save_stage_output (regd, xf_out, stage);
}

void
load_input_files (Registration_Data* regd, Registration_Parms* regp)
{
    RadImage::RadImageType image_type = RadImage::TYPE_ITK_FLOAT;

    /* Load the appropriate image type for the first stage */
    if (regp->num_stages > 0) {
	image_type = choose_image_type (regp->stages[0]->xform_type,
	    regp->stages[0]->optim_type, regp->stages[0]->impl_type);
    }

    printf ("fixed image=%s\n", regp->fixed_fn);
    printf ("Loading fixed image...");
    //regd->fixed_image = load_float (regp->fixed_fn);
    regd->fixed_image = rad_image_load (regp->fixed_fn, image_type);
    fflush (stdout);
    printf ("done!\n");

    printf ("moving image=%s\n", regp->moving_fn);
    printf ("Loading moving image...");
    fflush (stdout);
    regd->moving_image = rad_image_load (regp->moving_fn, image_type);
    printf ("done!\n");

    if (regp->fixed_mask_fn[0]) {
	printf ("Loading fixed mask...");
	regd->fixed_mask = load_uchar (regp->fixed_mask_fn);
	printf ("done!\n");
    } else {
	regd->fixed_mask = 0;
    }
    if (regp->moving_mask_fn[0]) {
	printf ("Loading moving mask...");
	regd->moving_mask = load_uchar (regp->moving_mask_fn);
	printf ("done!\n");
    } else {
	regd->moving_mask = 0;
    }
}

void
do_registration (Registration_Parms* regp)
{
    int i;
    Registration_Data regd;
    Xform xf1, xf2;
    Xform *xf_in, *xf_out, *xf_tmp;

    xf_in = &xf1;
    xf_out = &xf2;

    /* Load images */
    load_input_files (&regd, regp);

    /* Load initial guess of xform */
    if (regp->xf_in_fn[0]) {
	load_xform (xf_out, regp->xf_in_fn);
    }

    for (i = 0; i < regp->num_stages; i++) {
	/* Swap xf_in and xf_out */
	xf_tmp = xf_out; xf_out = xf_in; xf_in = xf_tmp;
	/* Run registation, results are stored in xf_out */
	do_registration_stage (&regd, xf_out, xf_in, regp->stages[i]);
    }

    /* RMK: If no stages, we still generate output (same as input) */

    printf("writing final output...\n");
    save_regp_output (&regd, xf_out, regp);
}

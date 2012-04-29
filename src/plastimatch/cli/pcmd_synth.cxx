/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmcli_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "itk_image.h"
#include "itk_image_save.h"
#include "plm_math.h"
#include "plm_clp.h"
#include "pstring.h"
#include "rtds.h"
#include "rtss.h"
#include "synthetic_mha.h"

typedef struct synthetic_mha_main_parms Synthetic_mha_main_parms;
struct synthetic_mha_main_parms {
    Pstring output_fn;
    Pstring output_dose_img_fn;
    Pstring output_ss_img_fn;
    Pstring output_dicom;
    Synthetic_mha_parms sm_parms;
};

void
do_synthetic_mha (Synthetic_mha_main_parms *parms)
{
    Synthetic_mha_parms *sm_parms = &parms->sm_parms;

    /* Create image */
    Rtds rtds;
    if (parms->output_dicom.not_empty() 
	|| parms->output_ss_img_fn.not_empty())
    {
	sm_parms->m_want_ss_img = true;
    }
    if (parms->output_dicom.not_empty() 
	|| parms->output_dose_img_fn.not_empty())
    {
	sm_parms->m_want_dose_img = true;
    }
    synthetic_mha (&rtds, sm_parms);

    /* Save to file */
    FloatImageType::Pointer img = rtds.m_img->itk_float();
    if (parms->output_fn.not_empty()) {
	switch (sm_parms->output_type) {
	case PLM_IMG_TYPE_ITK_UCHAR:
	    itk_image_save_uchar (img, (const char*) parms->output_fn);
	    break;
	case PLM_IMG_TYPE_ITK_SHORT:
	    itk_image_save_short (img, (const char*) parms->output_fn);
	    break;
	case PLM_IMG_TYPE_ITK_USHORT:
	    itk_image_save_ushort (img, (const char*) parms->output_fn);
	    break;
	case PLM_IMG_TYPE_ITK_ULONG:
	    itk_image_save_uint32 (img, (const char*) parms->output_fn);
	    break;
	case PLM_IMG_TYPE_ITK_FLOAT:
	    itk_image_save_float (img, (const char*) parms->output_fn);
	    break;
	case PLM_IMG_TYPE_ITK_DOUBLE:
	    itk_image_save_double (img, (const char*) parms->output_fn);
	    break;
	}
    }

    /* ss_img */
    if (parms->output_ss_img_fn.not_empty()) {
#if (PLM_USE_SS_IMAGE_VEC)
        rtds.m_rtss->convert_to_uchar_vec ();
#endif
	rtds.m_rtss->save_ss_image (parms->output_ss_img_fn);
    }

    /* dose_img */
    if (parms->output_dose_img_fn.not_empty()) {
	rtds.m_dose->save_image (parms->output_dose_img_fn);
    }

#if defined (commentout)
    /* -- list of structure names -- */
    if (parms->output_ss_list_fn.not_empty()) {
	printf ("save_ss_img: save_ss_list\n");
	rtds->m_rtss->save_ss_list (parms->output_ss_list_fn);
    }
#endif

    if (parms->output_dicom.not_empty()) {
	rtds.m_rtss->convert_ss_img_to_cxt ();
	rtds.save_dicom ((const char*) parms->output_dicom);
    }
}

static void
usage_fn (dlib::Plm_clp* parser, int argc, char *argv[])
{
    std::cout << "Usage: plastimatch synth [options]\n";
    parser->print_options (std::cout);
    std::cout << std::endl;
}

static void
parse_fn (
    Synthetic_mha_main_parms *parms, 
    dlib::Plm_clp *parser, 
    int argc, 
    char* argv[]
)
{
    /* Add --help, --version */
    parser->add_default_options ();

    /* Output files */
    parser->add_long_option ("", "output", 
	"output filename", 1, "");
    parser->add_long_option ("", "output-dicom", 
	"output dicom directory", 1, "");
    parser->add_long_option ("", "output-dose-img", 
	"filename for output dose image", 1, "");
    parser->add_long_option ("", "output-ss-img", 
	"filename for output structure set image", 1, "");
    parser->add_long_option ("", "output-type", 
	"data type for output image: {uchar,short,ushort, ulong,float},"
	" default is float", 
	1, "float");

    /* Main pattern */
    parser->add_long_option ("", "pattern",
	"synthetic pattern to create: {"
	"donut, enclosed_rect, gauss, grid, lung, osd, rect, sphere"
	"}, default is gauss", 
	1, "gauss");

    /* Image size */
    parser->add_long_option ("", "origin", 
	"location of first image voxel in mm \"x y z\"", 1, "0 0 0");
    parser->add_long_option ("", "dim", 
	"size of output image in voxels \"x [y z]\"", 1, "100");
    parser->add_long_option ("", "spacing", 
	"voxel spacing in mm \"x [y z]\"", 1, "5");
    parser->add_long_option ("", "direction-cosines", 
	"oriention of x, y, and z axes; Specify either preset value,"
	" {identity,rotated-{1,2,3},sheared},"
	" or 9 digit matrix string \"a b c d e f g h i\"", 1, "");
    parser->add_long_option ("", "volume-size", 
	"size of output image in mm \"x [y z]\"", 1, "500");

    /* Image intensities */
    parser->add_long_option ("", "background", 
	"intensity of background region", 1, "-1000");
    parser->add_long_option ("", "foreground", 
	"intensity of foreground region", 1, "0");

    /* Donut options */
    parser->add_long_option ("", "donut-center", 
	"location of donut center in mm \"x [y z]\"", 1, "0 0 0");
    parser->add_long_option ("", "donut-radius", 
	"size of donut in mm \"x [y z]\"", 1, "50 50 20");
    parser->add_long_option ("", "donut-rings", 
	"number of donut rings (2 rings for traditional donut)", 1, "2");
	
    /* Gaussian options */
    parser->add_long_option ("", "gauss-center", 
	"location of Gaussian center in mm \"x [y z]\"", 1, "0 0 0");
    parser->add_long_option ("", "gauss-std", 
	"width of Gaussian in mm \"x [y z]\"", 1, "100");

    /* Rect options */
    parser->add_long_option ("", "rect-size", 
	"width of rectangle in mm \"x [y z]\","
	" or locations of rectangle corners in mm"
	" \"x1 x2 y1 y2 z1 z2\"", 1, "-50 50 -50 50 -50 50");

    /* Sphere options */
    parser->add_long_option ("", "sphere-center", 
	"location of sphere center in mm \"x y z\"", 1, "0 0 0");
    parser->add_long_option ("", "sphere-radius", 
	"radius of sphere in mm \"x [y z]\"", 1, "50");

    /* Grid pattern options */
    parser->add_long_option ("", "grid-pattern", 
	"grid pattern spacing in voxels \"x [y z]\"", 1, "10");

    /* Lung options */
    parser->add_long_option ("", "lung-tumor-pos", 
	"position of tumor in mm \"z\" or \"x y z\"", 1, "0");

    /* Parse the command line arguments */
    parser->parse (argc,argv);

    /* Handle --help, --version */
    parser->check_default_options ();

    /* Check that an output file was given */
    if (!parser->option("output") && !parser->option("output-dicom")) {
	throw dlib::error (
	    "Error, you must specify either --output or --output-dicom.\n"
	);
    }

    /* Copy values into output struct */
    Synthetic_mha_parms *sm_parms = &parms->sm_parms;

    /* Basic options */
    parms->output_fn = parser->get_string("output").c_str();
    parms->output_dicom = parser->get_string("output-dicom").c_str();
    parms->output_dose_img_fn = parser->get_string("output-dose-img").c_str();
    parms->output_ss_img_fn = parser->get_string("output-ss-img").c_str();
    sm_parms->output_type = plm_image_type_parse (
	parser->get_string("output-type").c_str());
    if (sm_parms->output_type == PLM_IMG_TYPE_UNDEFINED) {
	throw dlib::error ("Error, unknown output-type\n");
    }

    /* Main pattern */
    std::string arg = parser->get_string ("pattern");
    if (arg == "gauss") {
	sm_parms->pattern = PATTERN_GAUSS;
    }
    else if (arg == "rect") {
	sm_parms->pattern = PATTERN_RECT;
    }
    else if (arg == "sphere") {
	sm_parms->pattern = PATTERN_SPHERE;
    }
    else if (arg == "multi-sphere") {
	sm_parms->pattern = PATTERN_MULTI_SPHERE;
    }
    else if (arg == "osd") {
	sm_parms->pattern = PATTERN_OBJSTRUCTDOSE;
    }
    else if (arg == "enclosed_rect") {
	sm_parms->pattern = PATTERN_ENCLOSED_RECT;
    }
    else if (arg == "donut") {
	sm_parms->pattern = PATTERN_DONUT;
    }
    else if (arg == "grid") {
	sm_parms->pattern = PATTERN_GRID;
    }
    else if (arg == "lung") {
	sm_parms->pattern = PATTERN_LUNG;
    }
    else {
	throw (dlib::error ("Error. Unknown --pattern argument: " + arg));
    }

    /* Image size */
    parser->assign_int13 (sm_parms->dim, "dim");

    /* Direction cosines */
    if (parser->option ("direction-cosines")) {
	std::string arg = parser->get_string("direction-cosines");
	if (!sm_parms->dc.set_from_string (arg)) {
	    throw (dlib::error ("Error parsing --direction-cosines "
		    "(should have nine numbers)\n"));
	}
    }

    /* If origin not specified, volume is centered about size */
    float volume_size[3];
    parser->assign_float13 (volume_size, "volume-size");
    if (parser->option ("origin")) {
	parser->assign_float13 (sm_parms->origin, "origin");
    } else {
	for (int d = 0; d < 3; d++) {
	    /* GCS FIX: This should include direction cosines */
	    sm_parms->origin[d] = - 0.5 * volume_size[d] 
		+ 0.5 * volume_size[d] / sm_parms->dim[d];
	}
    }

    /* If spacing not specified, set spacing from size and resolution */
    if (parser->option ("spacing")) {
	parser->assign_float13 (sm_parms->spacing, "spacing");
    } else {
	for (int d = 0; d < 3; d++) {
	    sm_parms->spacing[d] 
		= volume_size[d] / ((float) sm_parms->dim[d]);
	}
    }

    /* Image intensities */
    sm_parms->background = parser->get_float ("background");
    sm_parms->foreground = parser->get_float ("foreground");

    /* Donut options */
    parser->assign_float13 (sm_parms->donut_center, "donut-center");
    parser->assign_float13 (sm_parms->donut_radius, "donut-radius");
    sm_parms->donut_rings = parser->get_int ("donut-rings");

    /* Gaussian options */
    parser->assign_float13 (sm_parms->gauss_center, "gauss-center");
    parser->assign_float13 (sm_parms->gauss_std, "gauss-std");

    /* Rect options */
    int rc = sscanf (parser->get_string("rect-size").c_str(), 
	"%g %g %g %g %g %g", 
	&(sm_parms->rect_size[0]), 
	&(sm_parms->rect_size[1]), 
	&(sm_parms->rect_size[2]), 
	&(sm_parms->rect_size[3]), 
	&(sm_parms->rect_size[4]), 
	&(sm_parms->rect_size[5]));
    if (rc == 1) {
	sm_parms->rect_size[0] = - 0.5 * sm_parms->rect_size[0];
	sm_parms->rect_size[1] = - sm_parms->rect_size[0];
	sm_parms->rect_size[2] = + sm_parms->rect_size[0];
	sm_parms->rect_size[3] = - sm_parms->rect_size[0];
	sm_parms->rect_size[4] = + sm_parms->rect_size[0];
	sm_parms->rect_size[5] = - sm_parms->rect_size[0];
    }
    else if (rc == 3) {
	sm_parms->rect_size[4] = - 0.5 * sm_parms->rect_size[2];
	sm_parms->rect_size[2] = - 0.5 * sm_parms->rect_size[1];
	sm_parms->rect_size[0] = - 0.5 * sm_parms->rect_size[0];
	sm_parms->rect_size[1] = - sm_parms->rect_size[0];
	sm_parms->rect_size[3] = - sm_parms->rect_size[2];
	sm_parms->rect_size[5] = - sm_parms->rect_size[4];
    }
    else if (rc != 6) {
	throw (dlib::error ("Error. Option --rect_size must have "
		"one, three, or six arguments\n"));
    }

    /* Sphere options */
    parser->assign_float13 (sm_parms->sphere_center, "sphere-center");
    parser->assign_float13 (sm_parms->sphere_radius, "sphere-radius");

    /* Grid pattern options */
    parser->assign_int13 (sm_parms->grid_spacing, "grid-pattern");

    /* Lung options */
    rc = sscanf (parser->get_string("lung-tumor-pos").c_str(), 
	"%g %g %g", 
	&(sm_parms->lung_tumor_pos[0]), 
	&(sm_parms->lung_tumor_pos[1]), 
	&(sm_parms->lung_tumor_pos[2]));
    if (rc == 1) {
	sm_parms->lung_tumor_pos[2] = sm_parms->lung_tumor_pos[0];
	sm_parms->lung_tumor_pos[0] = 0;
	sm_parms->lung_tumor_pos[1] = 0;
    }
    else if (rc != 3) {
	throw (dlib::error ("Error. Option --lung-tumor-pos must have "
		"one or three arguments\n"));
    }
}

void
do_command_synth (int argc, char* argv[])
{
    Synthetic_mha_main_parms parms;

    plm_clp_parse (&parms, &parse_fn, &usage_fn, argc, argv, 1);

    do_synthetic_mha (&parms);
}
/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include "bstring_util.h"
#include "cxt_extract.h"
#include "itk_image_save.h"
#include "file_util.h"
#if GDCM_VERSION_1
#include "gdcm_rtss.h"
#endif
#include "plm_warp.h"
#include "rasterizer.h"
#include "referenced_dicom_dir.h"
#include "rtds.h"
#include "rtss.h"
#include "rtss_polyline_set.h"
#include "rtss_structure.h"
#include "ss_img_extract.h"
#include "ss_img_stats.h"
#include "ss_list_io.h"
#include "xio_structures.h"

static void
compose_prefix_fn (
    CBString *fn, 
    const CBString &output_prefix, 
    const CBString &structure_name
)
{
    fn->format ("%s/%s.%s", 
	(const char*) output_prefix, 
	(const char*) structure_name, 
	"mha");
}

Rtss::Rtss (Rtds *rtds) {
    m_ss_list = 0;
    m_cxt = 0;
    m_ss_img = 0;
    m_labelmap = 0;
    m_img_metadata.set_parent (&rtds->m_img_metadata);
}

Rtss::~Rtss () {
    clear ();
}

void
Rtss::clear () {
    if (this->m_ss_list) {
	delete this->m_ss_list;
	this->m_ss_list = 0;
    }
    if (this->m_cxt) {
	delete this->m_cxt;
	this->m_cxt = 0;
    }
    if (this->m_ss_img) {
	delete this->m_ss_img;
	this->m_ss_img = 0;
    }
    if (this->m_labelmap) {
	delete this->m_labelmap;
	this->m_labelmap = 0;
    }
}

void
Rtss::load (const char *ss_img, const char *ss_list)
{
    /* Load ss_img */
    if (m_ss_img) {
	delete this->m_ss_img;
    }
    if (ss_img && file_exists (ss_img)) {
	this->m_ss_img = plm_image_load_native (ss_img);
    }

    /* Load ss_list */
    if (this->m_ss_list) {
	delete this->m_ss_list;
    }
    if (ss_list && file_exists (ss_list)) {
	printf ("Trying to load ss_list: %s\n", ss_list);
	this->m_ss_list = ss_list_load (0, ss_list);
    }
}

void
Rtss::load_cxt (const CBString &input_fn, Referenced_dicom_dir *rdd)
{
    this->m_cxt = new Rtss_polyline_set;
    cxt_load (this, rdd, (const char*) input_fn);
}

void
Rtss::load_gdcm_rtss (const char *input_fn, Referenced_dicom_dir *rdd)
{
#if GDCM_VERSION_1
    this->m_cxt = new Rtss_polyline_set;
    gdcm_rtss_load (this, rdd, &this->m_img_metadata, input_fn);
#endif
}

void
Rtss::load_xio (char *input_dir)
{
    this->m_cxt = new Rtss_polyline_set;
    printf ("calling xio_structures_load\n");
    xio_structures_load (this->m_cxt, input_dir);
}

void
Rtss::save_colormap (const CBString &colormap_fn)
{
    ss_list_save_colormap (this->m_cxt, (const char*) colormap_fn);
}

void
Rtss::save_cxt (
    Referenced_dicom_dir *rdd, 
    const CBString &cxt_fn, 
    bool prune_empty
)
{
    cxt_save (this, rdd, (const char*) cxt_fn, prune_empty);
}

void
Rtss::save_gdcm_rtss (
    const char *output_dir, 
    Referenced_dicom_dir *rdd
)
{
    char fn[_MAX_PATH];

    this->m_cxt->adjust_structure_names ();

    if (rdd) {
	this->apply_dicom_dir (rdd);
    }

    snprintf (fn, _MAX_PATH, "%s/%s", output_dir, "ss.dcm");

#if GDCM_VERSION_1
    gdcm_rtss_save (this, rdd, fn);
#endif
}

void
Rtss::save_ss_image (const CBString &ss_img_fn)
{
    if (!this->m_ss_img) {
	print_and_exit (
	    "Error: save_ss_image() tried to write a non-existant file");
    }
#if (PLM_USE_SS_IMAGE_VEC)
    /* Image type must be uchar vector */
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_UCHAR_VEC);
#else
    /* Image type must be uint32_t */
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_ULONG);
#endif
    this->m_ss_img->save_image ((const char*) ss_img_fn);
}

void
Rtss::save_labelmap (const CBString &labelmap_fn)
{
    this->m_labelmap->save_image ((const char*) labelmap_fn);
}

void
Rtss::save_prefix (const CBString &output_prefix)
{
    int i;

    if (!m_ss_img) {
	return;
    }

    if (!m_ss_list) {
	printf ("WTF???\n");
    }

    for (i = 0; i < m_ss_list->num_structures; i++)
    {
	CBString fn;
	Rtss_structure *curr_structure = m_ss_list->slist[i];
	int bit = curr_structure->bit;

	if (bit == -1) continue;
#if (PLM_USE_SS_IMAGE_VEC)
	m_ss_img->convert (PLM_IMG_TYPE_ITK_UCHAR_VEC);
	UCharImageType::Pointer prefix_img = ss_img_extract_bit (
	    m_ss_img->m_itk_uchar_vec, bit);
#else
	m_ss_img->convert (PLM_IMG_TYPE_ITK_ULONG);
	UCharImageType::Pointer prefix_img = ss_img_extract_bit (
	    m_ss_img->m_itk_uint32, bit);
#endif
	compose_prefix_fn (&fn, output_prefix, curr_structure->name);
	itk_image_save (prefix_img, (const char*) fn);
    }
}

void
Rtss::save_ss_list (const CBString &ss_list_fn)
{
    ss_list_save (this->m_cxt, (const char*) ss_list_fn);
}

void
Rtss::save_xio (Xio_ct_transform *xio_transform, Xio_version xio_version, 
    const CBString &output_dir)
{
    xio_structures_save (this->m_cxt, xio_transform,
	xio_version, (const char*) output_dir);
}

UInt32ImageType::Pointer
Rtss::get_ss_img (void)
{
    if (!this->m_ss_img) {
	print_and_exit ("Sorry, can't get_ss_img()\n");
    }
    return this->m_ss_img->m_itk_uint32;
}

Rtss_polyline_set*
Rtss::get_ss_list (void)
{
    if (!this->m_ss_list) {
	print_and_exit ("Sorry, can't get_ss_list()\n");
    }
    return this->m_ss_list;
}

void
Rtss::apply_dicom_dir (const Referenced_dicom_dir *rdd)
{
    if (!this->m_cxt) {
	return;
    }

    if (!rdd || !rdd->m_loaded) {
	return;
    }

    /* Geometry */
    for (int d = 0; d < 3; d++) {
	this->m_cxt->m_offset[d] = rdd->m_pih.m_origin[d];
	this->m_cxt->m_dim[d] = rdd->m_pih.Size(d);
	this->m_cxt->m_spacing[d] = rdd->m_pih.m_spacing[d];
    }

    /* Slice numbers and slice uids */
    for (int i = 0; i < this->m_cxt->num_structures; i++) {
	Rtss_structure *curr_structure = this->m_cxt->slist[i];
	for (int j = 0; j < curr_structure->num_contours; j++) {
	    Rtss_polyline *curr_polyline = curr_structure->pslist[j];
	    if (curr_polyline->num_vertices <= 0) {
		continue;
	    }
	    rdd->get_slice_info (
		&curr_polyline->slice_no,
		&curr_polyline->ct_slice_uid,
		curr_polyline->z[0]);
	}
    }
}

void
Rtss::convert_ss_img_to_cxt (void)
{
    int num_structs = -1;

    /* Only convert if ss_img found */
    if (!this->m_ss_img) {
	return;
    }

    /* Allocate memory for cxt */
    if (this->m_cxt) {
	delete this->m_cxt;
    }
    this->m_cxt = new Rtss_polyline_set;

    /* Copy geometry from ss_img to cxt */
    this->m_cxt->set_geometry_from_plm_image (
	this->m_ss_img);

    /* Extract polylines */
    printf ("Running marching squares\n");
    num_structs = this->m_ss_list->num_structures;

#if (PLM_USE_SS_IMAGE_VEC)
    /* Image type must be uchar vector */
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_UCHAR_VEC);

    /* Do extraction */
    if (this->m_ss_list) {
	this->m_cxt = Rtss_polyline_set::clone_empty (
	    this->m_cxt, this->m_ss_list);
	cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uchar_vec, -1, true);
    } else {
	cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uchar_vec, -1, false);
    }

#else
    /* Image type must be uint32_t */
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_ULONG);

    /* Do extraction */
    if (this->m_ss_list) {
	this->m_cxt = Rtss_polyline_set::clone_empty (
	    this->m_cxt, this->m_ss_list);
	cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uint32, -1, true);
    } else {
	cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uint32, -1, false);
    }
#endif
}

void
Rtss::cxt_re_extract (void)
{
    this->m_cxt->free_all_polylines ();
#if (PLM_USE_SS_IMAGE_VEC)
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_UCHAR_VEC);
    cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uchar_vec, 
	this->m_cxt->num_structures, true);
#else
    this->m_ss_img->convert (PLM_IMG_TYPE_ITK_ULONG);
    cxt_extract (this->m_cxt, this->m_ss_img->m_itk_uint32, 
	this->m_cxt->num_structures, true);
#endif
}

void
Rtss::prune_empty (void)
{
    if (this->m_cxt) {
	this->m_cxt->prune_empty ();
    }
}

void
Rtss::rasterize (
    Plm_image_header *pih,
    bool want_labelmap
)
{
    /* Rasterize structure sets */
    Rasterizer rasterizer;

    printf ("Rasterizing...\n");
    rasterizer.rasterize (this->m_cxt, pih, false, want_labelmap, true);

    /* Convert rasterized structure sets from vol to plm_image */
    printf ("Converting...\n");
    if (want_labelmap) {
	this->m_labelmap = new Plm_image;
	this->m_labelmap->set_gpuit (rasterizer.labelmap_vol);
	rasterizer.labelmap_vol = 0;
    }
    if (this->m_ss_img) {
	delete this->m_ss_img;
    }
    this->m_ss_img = new Plm_image;

#if (PLM_USE_SS_IMAGE_VEC)
    this->m_ss_img->set_itk (rasterizer.m_ss_img);
#else
    this->m_ss_img->set_gpuit (rasterizer.ss_img_vol);
    rasterizer.ss_img_vol = 0;
#endif

    /* Clone the set of names */
    this->m_ss_list = Rtss_polyline_set::clone_empty (
	this->m_ss_list, this->m_cxt);

    printf ("Finished rasterization.\n");
}

void
Rtss::set_geometry_from_plm_image_header (Plm_image_header *pih)
{
    if (this->m_cxt) {
	this->m_cxt->set_geometry_from_plm_image_header (pih);
    }
}

void
Rtss::find_rasterization_geometry (Plm_image_header *pih)
{
    if (this->m_cxt) {
	this->m_cxt->find_rasterization_geometry (pih);
    }
}

void
Rtss::warp (
    Xform *xf, 
    Plm_image_header *pih, 
    Warp_parms *parms)
{
    Plm_image *tmp;

    if (this->m_labelmap) {
	printf ("Warping labelmap.\n");
	tmp = new Plm_image;
	plm_warp (tmp, 0, xf, pih, this->m_labelmap, 0, parms->use_itk, 0);
	delete this->m_labelmap;
	this->m_labelmap = tmp;
	this->m_labelmap->convert (PLM_IMG_TYPE_ITK_ULONG);
    }

    if (this->m_ss_img) {
	printf ("Warping ss_img.\n");
	tmp = new Plm_image;
	plm_warp (tmp, 0, xf, pih, this->m_ss_img, 0, parms->use_itk, 0);
	delete this->m_ss_img;
	this->m_ss_img = tmp;
#if (!PLM_USE_SS_IMAGE_VEC)
	this->m_ss_img->convert (PLM_IMG_TYPE_ITK_ULONG);
#endif
    }

    /* The cxt polylines are now obsolete, but we can't delete it because 
       it contains our "bits", used e.g. by prefix extraction.  */
}

/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _segmentation_h_
#define _segmentation_h_

#include "plmbase_config.h"

#include "itk_image_type.h"
#include "metadata.h"
#include "pstring.h"
#include "rtss.h"
#include "xio_studyset.h"  /* enum Xio_version */

class Plm_image;
class Plm_image_header;
class Rt_study;
class Segmentation_private;
class Rtss_roi;
class Slice_index;
class Xform;
class Xio_ct_transform;
class Warp_parms;

class PLMBASE_API Segmentation {
public:
    SMART_POINTER_SUPPORT (Segmentation);
public:
    Segmentation_private *d_ptr;

public:
    Segmentation (Rt_study *rtds = 0);
    ~Segmentation ();

    void clear ();
    void load (const char *ss_img, const char *ss_list);
    void load_cxt (const Pstring &input_fn, Rt_study_metadata *rsm);
    void load_prefix (const char *prefix_dir);
    void load_prefix (const Pstring &prefix_dir);
    void load_xio (const Xio_studyset& xio_studyset);
    void load_gdcm_rtss (const char *input_fn, Rt_study_metadata *rsm);

    size_t get_num_structures ();
    std::string get_structure_name (size_t index);
    UCharImageType::Pointer get_structure_image (int index);

    void save_colormap (const Pstring &colormap_fn);
    void save_cxt (const Rt_study_metadata::Pointer& rsm, 
        const Pstring &cxt_fn, bool prune_empty);
    void save_gdcm_rtss (const char *output_dir, 
        const Rt_study_metadata::Pointer& rsm);
    void save_fcsv (const Rtss_roi *curr_structure, const Pstring& fn);
    void save_prefix_fcsv (const Pstring &output_prefix);
    void save_ss_image (const Pstring &ss_img_fn);
    void save_labelmap (const Pstring &labelmap_fn);
    void save_prefix (const std::string& output_prefix, 
        const std::string& extension = "mha");
    void save_prefix (const char *output_prefix);
    void save_prefix (const Pstring &output_prefix);
    void save_ss_list (const Pstring &ss_list_fn);
    void save_xio (Xio_ct_transform *xio_transform, Xio_version xio_version, 
        const Pstring &output_dir);
    UInt32ImageType::Pointer get_ss_img_uint32 (void);
    UCharVecImageType::Pointer get_ss_img_uchar_vec (void);

    void apply_dicom_dir (const Rt_study_metadata::Pointer& rsm);
    void convert_ss_img_to_cxt (void);
    void convert_to_uchar_vec (void);
    void cxt_extract (void);
    void cxt_re_extract (void);
    void prune_empty (void);
    void rasterize (Plm_image_header *pih, bool want_labelmap, 
        bool xor_overlapping);
    void set_geometry (const Plm_image_header *pih);
    void find_rasterization_geometry (Plm_image_header *pih);
    Segmentation::Pointer warp_nondestructive (
        Xform *xf, Plm_image_header *pih, bool use_itk = false) const;
    void warp (Xform *xf, Plm_image_header *pih, bool use_itk = false);
    void warp (Xform *xf, Plm_image_header *pih, Warp_parms *parms);

    void add_structure (
        UCharImageType::Pointer itk_image, 
        const char *structure_name = 0,
        const char *structure_color = 0);

    bool have_ss_img ();
    void set_ss_img (UCharImageType::Pointer ss_img);
    Plm_image* get_ss_img ();

    bool have_structure_set ();
    Rtss::Pointer& get_structure_set ();
    Rtss* get_structure_set_raw ();
    void set_structure_set (Rtss::Pointer& rtss_ss);
    void set_structure_set (Rtss *rtss_ss);

    void set_structure_image (
        UCharImageType::Pointer uchar_img, 
        unsigned int bit
    );

    void resample (float spacing[3]);

protected:
    void initialize_ss_image (
        const Plm_image_header& pih, int vector_length);
    void broaden_ss_image (
        int new_vector_length);

};

#endif
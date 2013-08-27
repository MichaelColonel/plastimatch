/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _rpl_volume_h_
#define _rpl_volume_h_

#include "plmbase_config.h"
#include <string>
#include "aperture.h"
#include "ion_plan.h"
#include "plm_image.h"


class Proj_volume;
class Ray_data;
class Rpl_volume_private;
class Volume;
class Volume_limit;

class PLMBASE_API Rpl_volume 
{
public:
    Rpl_volume ();
    ~Rpl_volume ();
public:
    Rpl_volume_private *d_ptr;
public:
    void set_geometry (
        const double src[3],           // position of source (mm)
        const double iso[3],           // position of isocenter (mm)
        const double vup[3],           // dir to "top" of projection plane
        double sid,                    // dist from proj plane to source (mm)
        const int image_dim[2],        // resolution of image
        const double image_center[2],  // image center (pixels)
        const double image_spacing[2], // pixel size (mm)
        const double step_length       // spacing between planes
    );

    void set_ct_volume (Plm_image::Pointer& ct_volume);

    Aperture::Pointer& get_aperture ();
    void set_aperture (Aperture::Pointer& ap);

    Volume* get_vol ();
    Proj_volume *get_proj_volume ();

    double get_rgdepth (int ap_ij[2], double dist);
    double get_rgdepth (double ap_ij[2], double dist);
    double get_rgdepth (const double *xyz);

    double get_max_wed ();
    double get_min_wed ();

    void compute_rpl ();
    void compute (Volume *ct_vol);
    Volume* create_wed_volume (Ion_plan *scene);
    void compute_wed_volume (Volume *wed_vol, Volume *in_vol, float background);
    Volume* create_dew_volume (Ion_plan *scene);
    void compute_dew_volume (Volume *wed_vol, Volume *dew_vol, float background);
    void compute_beam_modifiers (Volume *seg_vol, float background);
    void compute_aperture (Volume *tgt_vol, float background);

    void apply_beam_modifiers ();

    void save (const std::string& filename);
    void save (const char* filename);

protected:
    void compute_ray_data ();

    void aprc_ray_trace (
        Volume *tgt_vol,             /* I: CT volume */
        Ray_data *ray_data,          /* I: Pre-computed data for this ray */
        Volume_limit *vol_limit,     /* I: CT bounding region */
        const double *src,           /* I: @ source */
        double rc_thk,               /* I: range compensator thickness */
        int* ires                    /* I: ray cast resolution */
    );
    void rpl_ray_trace (
        Volume *ct_vol,              /* I: CT volume */
        Ray_data *ray_data,          /* I: Pre-computed data for this ray */
        Volume_limit *vol_limit,     /* I: CT bounding region */
        const double *src,           /* I: @ source */
        double rc_thk,               /* I: range compensator thickness */
        int* ires                    /* I: ray cast resolution */
    );
};

#endif

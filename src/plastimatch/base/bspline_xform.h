/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _bspline_xform_h_
#define _bspline_xform_h_

#include "plmbase_config.h"
#include "volume.h"

class Volume_header;

class XAPI Bspline_xform {
  public:
    float img_origin[3];         /* Image origin (in mm) */
    float img_spacing[3];        /* Image spacing (in mm) */
    plm_long img_dim[3];           /* Image size (in vox) */
    Direction_cosines dc;        /* Image direction cosines */
    plm_long roi_offset[3];	 /* Position of first vox in ROI (in vox) */
    plm_long roi_dim[3];		 /* Dimension of ROI (in vox) */
    plm_long vox_per_rgn[3];	 /* Knot spacing (in vox) */
    float grid_spac[3];          /* Knot spacing (in mm) */
    plm_long rdims[3];             /* # of regions in (x,y,z) */
    plm_long cdims[3];             /* # of knots in (x,y,z) */
    int num_knots;               /* Total number of knots (= product(cdims)) */
    int num_coeff;               /* Total number of coefficents (= product(cdims) * 3) */
    float* coeff;                /* Coefficients.  Vector directions interleaved. */

    /* Aligned grid (3D) LUTs */
    plm_long* cidx_lut;            /* Lookup volume for region number */
    plm_long* c_lut;               /* Lookup table for control point indices */
    plm_long* qidx_lut;            /* Lookup volume for region offset */
    float* q_lut;                /* Lookup table for influence multipliers */

    /* Non-aligned grid (1D) LUTs */
    float *bx_lut;               /* LUT for influence multiplier in x dir */
    float *by_lut;               /* LUT for influence multiplier in y dir */
    float *bz_lut;               /* LUT for influence multiplier in z dir */

    /* Used by bspline_regularize.c */
    float* q_dxdyz_lut;          /* LUT for influence of dN1/dx*dN2/dy*N3 */
    float* q_xdydz_lut;          /* LUT for influence of N1*dN2/dy*dN3/dz */
    float* q_dxydz_lut;          /* LUT for influence of dN1/dx*N2*dN3/dz */
    float* q_d2xyz_lut;          /* LUT for influence of (d2N1/dx2)*N2*N3 */
    float* q_xd2yz_lut;          /* LUT for influence of N1*(d2N2/dy2)*N3 */
    float* q_xyd2z_lut;          /* LUT for influence of N1*N2*(d2N3/dz2) */

public:
    void get_volume_header (Volume_header *vh);
};

/* -----------------------------------------------------------------------
   Function declarations
   ----------------------------------------------------------------------- */



void
bspline_interp_pix (float out[3], const Bspline_xform* bxf, 
    plm_long p[3], plm_long qidx);
void
bspline_interp_pix_b (
    float out[3], 
    Bspline_xform* bxf, 
    plm_long pidx, 
    plm_long qidx
);

void
bspline_interp_pix_c (
    float out[3], 
    Bspline_xform* bxf, 
    plm_long pidx, 
    plm_long *q
);

#endif
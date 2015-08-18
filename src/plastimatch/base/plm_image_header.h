/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _plm_image_header_h_
#define _plm_image_header_h_

#include "plmbase_config.h"
#include "direction_cosines.h"
#include "itk_image.h"
#include "itk_image_region.h"
#include "itk_image_origin.h"
#include "plm_image.h"

class Bspline_xform;
class Plm_image_header_private;
class Volume;
class Volume_header;

/*! \brief 
 * The Plm_image_header class defines the geometry of an image.  
 * It defines image origin, spacing, dimensions, and direction cosines, 
 * but does not contain image voxels.
 */
class PLMBASE_API Plm_image_header {
public:
    Plm_image_header_private *d_ptr;
    OriginType m_origin;
    SpacingType m_spacing;
    RegionType m_region;
    DirectionType m_direction;

public:
    Plm_image_header ();
    Plm_image_header (plm_long dim[3], float origin[3], float spacing[3]);
    Plm_image_header (plm_long dim[3], float origin[3], float spacing[3],
        float direction_cosines[9]);
    Plm_image_header (Plm_image *pli);
    Plm_image_header (const Plm_image& pli);
    Plm_image_header (const Plm_image::Pointer& pli);
    Plm_image_header (const Volume_header& vh);
    Plm_image_header (const Volume& vol);
    Plm_image_header (const Volume* vol);
    Plm_image_header (Volume* vol);
    template<class T> Plm_image_header (T image);
    Plm_image_header (const Plm_image_header&);
    ~Plm_image_header ();

public:
    const Plm_image_header& operator= (const Plm_image_header&);

public:
    int Size (int d) const { return m_region.GetSize()[d]; }
    const SizeType& GetSize (void) const { return m_region.GetSize (); }
public:
    /*! \brief Return true if the two headers are the same. 
      Tolerance on origin and spacing can be specified 
      using the threshold parameter */
    static bool compare (Plm_image_header *pli1, Plm_image_header *pli2, 
        float threshold = 1e-5);

    int dim (int d) const { return m_region.GetSize()[d]; }
    float origin (int d) const { return m_origin[d]; }
    float spacing (int d) const { return m_spacing[d]; }

public:
    void set_dim (const plm_long dim[3]);
    void set_origin (const float origin[3]);
    void set_spacing (const float spacing[3]);
    void set_direction_cosines (
        const float direction_cosines[9]);
    void set_direction_cosines (
        const Direction_cosines& dc);
    void set (const Plm_image_header& src);
    void set (
        const plm_long dim[3],
        const float origin[3],
        const float spacing[3],
        const Direction_cosines& dc);
    void set (
        const plm_long dim[3],
        const float origin[3],
        const float spacing[3],
        const float direction_cosines[9]);
    void set_from_gpuit (
        const plm_long dim[3],
        const float origin[3],
        const float spacing[3],
        const float direction_cosines[9]);
    void set_from_gpuit_bspline (Bspline_xform *bxf);
    void set_from_plm_image (const Plm_image *pli);
    void set_from_plm_image (const Plm_image& pli);
    void set_from_plm_image (const Plm_image::Pointer& pli);
    void set_from_volume_header (const Volume_header& vh);
    void set (const Volume_header& vh);
    void set (const Volume& vol);
    void set (const Volume* vol);

    /*! \brief Expand existing geometry to contain the 
      specified point.  Only origin and dimensions can change, 
      spacing and direction cosines will stay the same. */
    void expand_to_contain (const FloatPoint3DType& position);

    /*! \brief Create a new geometry that can contain both 
      the reference and compare image, with direction cosines 
      and voxel spacing of the reference image */
    void set_geometry_to_contain (
        const Plm_image_header& reference_pih,
        const Plm_image_header& compare_pih);

    template<class T> 
        void set_from_itk_image (T image) {
        m_origin = itk_image_origin (image);
        m_spacing = image->GetSpacing ();
        m_region = itk_image_region (image);
        m_direction = image->GetDirection ();
    }
    static void clone (Plm_image_header *dest, const Plm_image_header *src) {
        dest->m_origin = src->m_origin;
        dest->m_spacing = src->m_spacing;
        dest->m_region = src->m_region;
        dest->m_direction = src->m_direction;
    }

    void get_volume_header (Volume_header *vh) const;
    void get_origin (float origin[3]) const;
    void get_spacing (float spacing[3]) const;
    void get_dim (plm_long dim[3]) const;
    void get_direction_cosines (float direction_cosines[9]) const;

    const OriginType& GetOrigin () const {
        return m_origin;
    }
    const SpacingType& GetSpacing () const {
        return m_spacing;
    }
    const RegionType& GetLargestPossibleRegion () const {
        return m_region;
    }
    const DirectionType& GetDirection () const {
        return m_direction;
    }

    void print (void) const;

    FloatPoint3DType get_index (const FloatPoint3DType& pos) const;
    FloatPoint3DType get_position (const float index[3]) const;
    void get_image_center (float center[3]) const;

    /*! \brief Get the number of voxels in the image */
    plm_long get_num_voxels () const;
    /*! \brief Get the physical extent (size) of the image, from 
      first voxel center to last voxel center.  Extent is zero 
      if only one voxel. */
    void get_image_extent (float extent[3]) const;
};

/* -----------------------------------------------------------------------
   Global functions
   ----------------------------------------------------------------------- */
void
direction_cosines_from_itk (
    float direction_cosines[9],
    DirectionType* itk_direction
);

#endif

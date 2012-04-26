/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "itkImageRegionIterator.h"

#include "plmsys.h"

#include "bspline_xform.h"
#include "itk_directions.h"
#include "plm_image.h"
#include "plm_image_header.h"
#include "volume.h"
#include "volume_header.h"

/* -----------------------------------------------------------------------
   functions
   ----------------------------------------------------------------------- */
void
Plm_image_header::set_dim (const plm_long dim[3])
{
    ImageRegionType::SizeType itk_size;
    ImageRegionType::IndexType itk_index;
    for (unsigned int d = 0; d < 3; d++) {
	itk_index[d] = 0;
	itk_size[d] = dim[d];
    }
    m_region.SetSize (itk_size);
    m_region.SetIndex (itk_index);
}

void
Plm_image_header::set_origin (const float origin[3])
{
    for (unsigned int d = 0; d < 3; d++) {
	this->m_origin[d] = origin[d];
    }
}

void
Plm_image_header::set_spacing (const float spacing[3])
{
    for (unsigned int d = 0; d < 3; d++) {
	this->m_spacing[d] = spacing[d];
    }
}

void
Plm_image_header::set_direction_cosines (const float direction_cosines[9])
{
    if (direction_cosines) {
	itk_direction_from_dc (&m_direction, direction_cosines);
    } else {
	itk_direction_set_identity (&m_direction);
    }
}

void
Plm_image_header::set_direction_cosines (const Direction_cosines& dc)
{
    itk_direction_from_dc (&m_direction, dc.m_direction_cosines);
}

void
Plm_image_header::set (
    const plm_long dim[3],
    const float origin[3],
    const float spacing[3],
    const float direction_cosines[9])
{
    this->set_dim (dim);
    this->set_origin (origin);
    this->set_spacing (spacing);
    this->set_direction_cosines (direction_cosines);
}

void
Plm_image_header::set (
    const plm_long dim[3],
    const float origin[3],
    const float spacing[3],
    const Direction_cosines& dc)
{
    this->set (dim, origin, spacing, dc.m_direction_cosines);
}

void
Plm_image_header::set_from_gpuit (
    const plm_long dim[3],
    const float origin[3],
    const float spacing[3],
    const float direction_cosines[9])
{
    this->set (dim, origin, spacing, direction_cosines);
}

void
Plm_image_header::set_from_gpuit_bspline (Bspline_xform *bxf)
{
    this->set_from_gpuit (
	bxf->img_dim,
	bxf->img_origin,
	bxf->img_spacing,
	(bxf->dc).m_direction_cosines);
}

void
Plm_image_header::set_from_plm_image (Plm_image *pli)
{
    switch (pli->m_type) {
    case PLM_IMG_TYPE_ITK_UCHAR:
	this->set_from_itk_image (pli->m_itk_uchar);
	break;
    case PLM_IMG_TYPE_ITK_SHORT:
	this->set_from_itk_image (pli->m_itk_short);
	break;
    case PLM_IMG_TYPE_ITK_USHORT:
	this->set_from_itk_image (pli->m_itk_ushort);
	break;
    case PLM_IMG_TYPE_ITK_LONG:
	this->set_from_itk_image (pli->m_itk_int32);
	break;
    case PLM_IMG_TYPE_ITK_ULONG:
	this->set_from_itk_image (pli->m_itk_uint32);
	break;
    case PLM_IMG_TYPE_ITK_FLOAT:
	this->set_from_itk_image (pli->m_itk_float);
	break;
    case PLM_IMG_TYPE_ITK_DOUBLE:
	this->set_from_itk_image (pli->m_itk_double);
	break;
    case PLM_IMG_TYPE_GPUIT_UCHAR:
    case PLM_IMG_TYPE_GPUIT_SHORT:
    case PLM_IMG_TYPE_GPUIT_UINT32:
    case PLM_IMG_TYPE_GPUIT_FLOAT:
    case PLM_IMG_TYPE_GPUIT_FLOAT_FIELD:
    {
	Volume* vol = (Volume*) pli->m_gpuit;
	set_from_gpuit (vol->dim, vol->offset, vol->spacing,
	    vol->direction_cosines);
	break;
    }
    case PLM_IMG_TYPE_ITK_UCHAR_VEC:
	this->set_from_itk_image (pli->m_itk_uchar_vec);
	break;
    case PLM_IMG_TYPE_ITK_FLOAT_FIELD:
    case PLM_IMG_TYPE_ITK_CHAR:
    default:
	print_and_exit ("Unhandled image type (%s) in set_from_plm_image\n",
	    plm_image_type_string (pli->m_type));
	break;
    }
}

void
Plm_image_header::set (const Volume_header& vh)
{
    this->set_from_gpuit (vh.m_dim, vh.m_origin, 
	vh.m_spacing, vh.m_direction_cosines);
}

void
Plm_image_header::set_from_volume_header (const Volume_header& vh)
{
    this->set (vh);
}

void
Plm_image_header::get_volume_header (Volume_header *vh) const
{
    this->get_origin (vh->m_origin);
    this->get_dim (vh->m_dim);
    this->get_spacing (vh->m_spacing);
    this->get_direction_cosines (vh->m_direction_cosines);
}

Volume_header
Plm_image_header::get_volume_header () const
{
    Volume_header vh;
    this->get_origin (vh.m_origin);
    this->get_dim (vh.m_dim);
    this->get_spacing (vh.m_spacing);
    this->get_direction_cosines (vh.m_direction_cosines);
    return vh;
}

void 
Plm_image_header::get_origin (float origin[3]) const
{
    for (unsigned int d = 0; d < 3; d++) {
	origin[d] = m_origin[d];
    }
}

void 
Plm_image_header::get_spacing (float spacing[3]) const
{
    for (unsigned int d = 0; d < 3; d++) {
	spacing[d] = m_spacing[d];
    }
}

void 
Plm_image_header::get_dim (plm_long dim[3]) const
{
    ImageRegionType::SizeType itk_size = m_region.GetSize ();
    for (unsigned int d = 0; d < 3; d++) {
	dim[d] = itk_size[d];
    }
}

void 
Plm_image_header::get_direction_cosines (float direction_cosines[9]) const
{
    dc_from_itk_direction (direction_cosines, &m_direction);
}

void
Plm_image_header::print (void) const
{
    ImageRegionType::SizeType itk_size;
    itk_size = m_region.GetSize ();

    printf ("Origin =");
    for (unsigned int d = 0; d < 3; d++) {
	printf (" %g", m_origin[d]);
    }
    printf ("\nSize =");
    for (unsigned int d = 0; d < 3; d++) {
	printf (" %lu", itk_size[d]);
    }
    printf ("\nSpacing =");
    for (unsigned int d = 0; d < 3; d++) {
	printf (" %g", m_spacing[d]);
    }
    printf ("\nDirection =");
    for (unsigned int d1 = 0; d1 < 3; d1++) {
	for (unsigned int d2 = 0; d2 < 3; d2++) {
#if defined (PLM_CONFIG_ALT_DCOS)
	    printf (" %g", m_direction[d2][d1]);
#else
	    printf (" %g", m_direction[d1][d2]);
#endif
	}
    }

    printf ("\n");
}

void
itk_roi_from_gpuit (
    ImageRegionType* roi,
    plm_long roi_offset[3], plm_long roi_dim[3])
{
    ImageRegionType::SizeType itk_size;
    ImageRegionType::IndexType itk_index;

    for (unsigned int d = 0; d < 3; d++) {
	itk_index[d] = roi_offset[d];
	itk_size[d] = roi_dim[d];
    }
    (*roi).SetSize (itk_size);
    (*roi).SetIndex (itk_index);
}

void 
Plm_image_header::get_image_center (float center[3]) const
{
    int d;
    for (d = 0; d < 3; d++) {
	center[d] = this->m_origin[d] 
	    + this->m_spacing[d] * (this->Size(d) - 1) / 2;
    }
}

/* Return 1 if the two headers are the same */
int
Plm_image_header::compare (Plm_image_header *pli1, Plm_image_header *pli2)
{
    int d;
    for (d = 0; d < 3; d++) {
	if (pli1->m_origin[d] != pli2->m_origin[d]) return 0;
	if (pli1->m_spacing[d] != pli2->m_spacing[d]) return 0;
	if (pli1->Size(d) != pli2->Size(d)) return 0;
    }

    /* GCS FIX: check direction cosines */

    return 1;
}
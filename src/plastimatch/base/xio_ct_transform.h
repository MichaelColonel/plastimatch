/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _xio_ct_transform_h_
#define _xio_ct_transform_h_

#include "plmbase_config.h"

class Metadata;
class Plm_image;
class Slice_index;

class PLMBASE_API Xio_ct_transform {
public:
    float direction_cosines[9];
    float x_offset;
    float y_offset;
public:
    Xio_ct_transform ();
    Xio_ct_transform (const Metadata* meta);
public:
    void set (const Metadata* meta);
    void set (const char* ppos);
    void set_from_rdd (
        Plm_image *pli,
        Metadata *meta,
        Slice_index *rdd);
};

#endif

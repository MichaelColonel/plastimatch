/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmregister_config.h"

#include "shared_parms.h"

Shared_parms::Shared_parms ()
{
    this->fixed_roi_enable = true;
    this->moving_roi_enable = true;
    this->fixed_roi_fn = "";
    this->moving_roi_fn = "";
    this->legacy_subsampling = false;
}

Shared_parms::Shared_parms (const Shared_parms& s)
{
    this->fixed_roi_enable = s.fixed_roi_enable;
    this->moving_roi_enable = s.moving_roi_enable;
    this->legacy_subsampling = s.legacy_subsampling;

    /* filenames do not propagate when copied */
}

Shared_parms::~Shared_parms ()
{
}

void
Shared_parms::copy (const Shared_parms *s)
{
    this->fixed_roi_enable = s->fixed_roi_enable;
    this->moving_roi_enable = s->moving_roi_enable;
    this->legacy_subsampling = s->legacy_subsampling;

    /* filenames do not propagate when copied */
}

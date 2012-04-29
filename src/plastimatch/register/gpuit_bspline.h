/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _gpuit_bspline_h_
#define _gpuit_bspline_h_

class Registration_parms;
class Registration_data;
class Xform;
class Stage_parms;

void
do_gpuit_bspline_stage (
    Registration_parms* regp, 
    Registration_data* regd,
    Xform *xf_out,
    Xform *xf_in,
    Stage_parms* stage);

#endif
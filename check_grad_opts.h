/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _check_grad_opts_h_
#define _check_grad_opts_h_

#include "bspline.h"

#define CHECK_GRAD_PROCESS_FWD        1
#define CHECK_GRAD_PROCESS_BKD        2
#define CHECK_GRAD_PROCESS_CTR        3
#define CHECK_GRAD_PROCESS_LINE       4


typedef struct check_grad_opts Check_grad_opts;
struct check_grad_opts {
    char* fixed_fn;
    char* moving_fn;
    char* output_fn;
    float factr;
    float pgtol;
    float step_size;
    int line_range[2];
    int vox_per_rgn[3];
    int process;
    BSPLINE_Parms parms;
};

gpuit_EXPORT
void
check_grad_opts_parse_args (Check_grad_opts* options, 
    int argc, char* argv[]);

#endif

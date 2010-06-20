/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <iostream>
#include <vector>
#include "plastimatch-slicerCLP.h"
#include "plm_registration.h"
#include "plm_stages.h"

int 
main (int argc, char * argv [])
{
    PARSE_ARGS;

#if defined (_WIN32)
    char* parms_fn = "C:/tmp/plastimatch-slicer-parms.txt";
#else
    char* parms_fn = "/tmp/plastimatch-slicer-parms.txt";
#endif

    FILE* fp = fopen (parms_fn, "w+");
    //FILE* fp = tmpfile ();

    fprintf (fp,
	"[GLOBAL]\n"
	"fixed=%s\n"
	"moving=%s\n"
	"img_out=%s\n\n",
	plmslc_fixed_volume.c_str(),
	plmslc_moving_volume.c_str(),
	plmslc_warped_volume.c_str());

    if (enable_stage_0) {
	fprintf (fp,
	    "[STAGE]\n"
	    "metric=%s\n"
	    "xform=%s\n"
	    "optim=%s\n"
	    "impl=itk\n"
	    "max_its=%d\n"
	    "convergence_tol=5\n"
	    "grad_tol=1.5\n"
	    "res=%d %d %d\n",
	    metric.c_str(),
	    "translation",
	    "rsg",
	    stage_0_its,
	    stage_0_resolution[0],
	    stage_0_resolution[1],
	    stage_0_resolution[2]
	);
    }

    /* Stage 1 */
    fprintf (fp,
	"[STAGE]\n"
	"metric=%s\n"
	"xform=bspline\n"	
	"optim=lbfgsb\n"
	"impl=plastimatch\n"
	"threading=%s\n"
	"max_its=%d\n"
	"convergence_tol=5\n"
	"grad_tol=1.5\n"
	"res=%d %d %d\n"
	"grid_spac=%g %g %g\n",
	/* Stage 1 */
	metric.c_str(),
	strcmp (hardware.c_str(),"CPU") ? "cuda" : "openmp",
	stage_1_its,
	stage_1_resolution[0],
	stage_1_resolution[1],
	stage_1_resolution[2],
	stage_1_grid_size,
	stage_1_grid_size,
	stage_1_grid_size
    );

    if (enable_stage_2) {
	fprintf (fp, 
	    "[STAGE]\n"
	    "xform=bspline\n"
	    "optim=lbfgsb\n"
	    "impl=plastimatch\n"
	    "max_its=%d\n"
	    "convergence_tol=5\n"
	    "grad_tol=1.5\n"
	    "res=%d %d %d\n"
	    "grid_spac=%g %g %g\n",
	    /* Stage 2 */
	    stage_2_its,
	    stage_2_resolution[0],
	    stage_2_resolution[1],
	    stage_2_resolution[2],
	    stage_2_grid_size,
	    stage_2_grid_size,
	    stage_2_grid_size
	);
    }

    /* Go back to beginning of file */
    fseek (fp, SEEK_SET, 0);

    /* Go back to beginning of file */
    Registration_Parms regp;
    if (plm_parms_process_command_file (&regp, fp) < 0) {
	fclose (fp);
	return EXIT_FAILURE;
    }

    fclose (fp);

    do_registration (&regp);
    return EXIT_SUCCESS;
}

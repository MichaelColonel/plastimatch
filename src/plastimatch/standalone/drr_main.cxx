/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#if (OPENMP_FOUND)
#include <omp.h>
#endif

#include "plmbase.h"
#include "plmsys.h"

#include "drr.h"
#include "drr_cuda.h"
#include "drr_opencl.h"
#include "drr_opts.h"
#include "drr_trilin.h"
#include "plm_math.h"
#include "proj_image.h"
#include "proj_matrix.h"
#include "delayload.h"
#include "volume.h"

static void*
allocate_gpu_memory (
    Proj_image *proj, 
    Volume *vol,
    Drr_options *options
)
{
#if CUDA_FOUND || OPENCL_FOUND
    void* tmp;
#endif

    switch (options->threading) {
#if CUDA_FOUND
    case THREADING_CUDA: {
        LOAD_LIBRARY_SAFE (libplmcuda);
        LOAD_SYMBOL (drr_cuda_state_create, libplmcuda);
        tmp = drr_cuda_state_create (proj, vol, options);
        UNLOAD_LIBRARY (libplmcuda);
        return tmp;
    }
#endif
#if OPENCL_FOUND
    case THREADING_OPENCL:
        tmp = drr_opencl_state_create (proj, vol, options);
        return tmp;
#endif
    case THREADING_CPU_SINGLE:
    case THREADING_CPU_OPENMP:
    default:
        return 0;
    }
}

static void
free_gpu_memory (
    void *dev_state,
    Drr_options *options
)
{

    switch (options->threading) {
#if CUDA_FOUND
    case THREADING_CUDA: {
        LOAD_LIBRARY_SAFE (libplmcuda);
        LOAD_SYMBOL (drr_cuda_state_destroy, libplmcuda);
	if (dev_state) {
	    drr_cuda_state_destroy (dev_state);
	}
	UNLOAD_LIBRARY (libplmcuda);
	return;
    }
#endif
#if OPENCL_FOUND
    case THREADING_OPENCL:
	if (dev_state) {
	    drr_opencl_state_destroy (dev_state);
	}
	return;
#endif
    case THREADING_CPU_SINGLE:
    case THREADING_CPU_OPENMP:
    default:
	return;
    }
}

static void
create_matrix_and_drr (
    Volume* vol, 
    Proj_image *proj,
    double cam[3],
    double tgt[3],
    double nrm[3],
    int a, 
    void *dev_state, 
    Drr_options* options
)
{
    char mat_fn[256];
    char img_fn[256];
    char multispectral_fn[256];
    Proj_matrix *pmat = proj->pmat;
    double vup[3] = {
	options->vup[0],
	options->vup[1],
	options->vup[2] };
    double sid = options->sid;
    Plm_timer* timer = plm_timer_create ();

    /* Set ic = image center (in pixels), and ps = pixel size (in mm)
       Note: pixels are numbered from 0 to ires-1 */
    double ic[2] = { options->image_center[0],
		     options->image_center[1] };

    /* Set image resolution */
    int ires[2] = { options->image_resolution[0],
		    options->image_resolution[1] };

    /* Set physical size of imager in mm */
    int isize[2] = { options->image_size[0],
		     options->image_size[1] };

    /* Set pixel size in mm */
    double ps[2] = { (double)isize[0]/(double)ires[0], 
		     (double)isize[1]/(double)ires[1] };

    /* Create projection matrix */
    sprintf (mat_fn, "%s%04d.txt", options->output_prefix, a);
    proj_matrix_set (pmat, cam, tgt, vup, sid, ic, ps, ires);

    if (options->output_format == OUTPUT_FORMAT_PFM) {
	sprintf (img_fn, "%s%04d.pfm", options->output_prefix, a);
    } else if (options->output_format == OUTPUT_FORMAT_PGM) {
	sprintf (img_fn, "%s%04d.pgm", options->output_prefix, a);
    } else {
	sprintf (img_fn, "%s%04d.raw", options->output_prefix, a);
    }
    sprintf (multispectral_fn, "%s%04d.msd", options->output_prefix, a);

    if (options->geometry_only) {
	proj_image_save (proj, 0, mat_fn);
    } else {
	drr_render_volume_perspective (proj, vol, ps, dev_state, options);
	plm_timer_start (timer);
	proj_image_save (proj, img_fn, mat_fn);
	printf ("I/O time: %f sec\n", plm_timer_report (timer));
    }

    plm_timer_destroy (timer);
}

/* All distances in mm */
void
drr_render_volume (Volume* vol, Drr_options* options)
{
    Proj_image *proj;
    int a;
    void *dev_state = 0;

    /* tgt is isocenter */
    double tgt[3] = {
	options->isocenter[0],
	options->isocenter[1],
	options->isocenter[2] };

    Plm_timer* timer = plm_timer_create ();
    plm_timer_start (timer);

    /* Allocate data for image and matrix */
    proj = new Proj_image;
    proj_image_create_pmat (proj);
    proj_image_create_img (proj, options->image_resolution);

    /* Allocate memory on the gpu device */
    dev_state = allocate_gpu_memory (proj, vol, options);

    /* If nrm was specified, only create a single image */
    if (options->have_nrm) {
	double cam[3];
	double nrm[3] = {
	    options->nrm[0],
	    options->nrm[1],
	    options->nrm[2] };

	/* Make sure nrm is normal */
	vec3_normalize1 (nrm);

	/* Place camera at distance "sad" from the volume isocenter */
	cam[0] = tgt[0] + options->sad * nrm[0];
	cam[1] = tgt[1] + options->sad * nrm[1];
	cam[2] = tgt[2] + options->sad * nrm[2];

	create_matrix_and_drr (vol, proj, cam, tgt, nrm, 0, 
	    dev_state, options);
    }

    /* Otherwise, loop through camera angles */
    else {
	for (a = 0; a < options->num_angles; a++) {
	    double angle = a * options->angle_diff;
	    double cam[3];
	    double nrm[3];

	    printf ("Rendering DRR %d\n", a);

	    /* Place camera at distance "sad" from the volume isocenter */
	    cam[0] = tgt[0] + options->sad * cos(angle);
	    cam[1] = tgt[1] - options->sad * sin(angle);
	    cam[2] = tgt[2];
	
	    /* Compute normal vector */
	    vec3_sub3 (nrm, tgt, cam);
	    vec3_normalize1 (nrm);

	    create_matrix_and_drr (vol, proj, cam, tgt, nrm, a, 
		dev_state, options);
	}
    }
    delete proj;

    free_gpu_memory (dev_state, options);

    printf ("Total time: %g secs\n", plm_timer_report (timer));

    plm_timer_destroy (timer);
}

void
set_isocenter (Volume* vol, Drr_options* options)
{
    vol->offset[0] -= options->isocenter[0];
    vol->offset[1] -= options->isocenter[1];
    vol->offset[2] -= options->isocenter[2];
}

int
main (int argc, char* argv[])
{
    Volume* vol = 0;
    Drr_options options;

    parse_args (&options, argc, argv);

    if (options.geometry_only) {
	options.threading = THREADING_CPU_SINGLE;
    }
    else {
	vol = read_mha (options.input_file);
	if (!vol) return -1;
	volume_convert_to_float (vol);
    }

#if defined (DRR_PREPROCESS_ATTENUATION)
    if (!options.geometry_only) {
	drr_preprocess_attenuation (vol);
    }
#endif

    drr_render_volume (vol, &options);

    if (!options.geometry_only) {
	delete vol;
    }
    printf ("Done.\n");
    return 0;
}
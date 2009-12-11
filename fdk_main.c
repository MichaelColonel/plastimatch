/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "fdk_brook.h"
#include "fdk_cuda.h"
#include "fdk_opts.h"
#include "fdk_utils.h"
#include "mathutil.h"
#include "print_and_exit.h"
#include "proj_image.h"
#include "proj_image_dir.h"
#include "readmha.h"

/* get_pixel_value_c seems to be no faster than get_pixel_value_b, 
   despite having two fewer compares. */
inline float
get_pixel_value_c (Proj_image* cbi, double r, double c)
{
    int rr, cc;

    r += 0.5;
    if (r < 0) return 0.0;
    rr = (int) r;
    if (rr >= cbi->dim[1]) return 0.0;

    c += 0.5;
    if (c < 0) return 0.0;
    cc = (int) c;
    if (cc >= cbi->dim[0]) return 0.0;

    return cbi->img[rr*cbi->dim[0] + cc];
}

inline float
get_pixel_value_b (Proj_image* cbi, double r, double c)
{
    int rr, cc;

    rr = ROUND_INT(r);
    if (rr < 0 || rr >= cbi->dim[1]) return 0.0;
    cc = ROUND_INT(c);
    if (cc < 0 || cc >= cbi->dim[0]) return 0.0;
    return cbi->img[rr*cbi->dim[0] + cc];
}

#if defined (commentout)
inline float
get_pixel_value_a (Proj_image* cbi, double r, double c)
{
    int rr, cc;

    rr = round_int (r);
    if (rr < 0 || rr >= cbi->dim[1]) return 0.0;
    cc = round_int (c);
    if (cc < 0 || cc >= cbi->dim[0]) return 0.0;
    return cbi->img[rr*cbi->dim[0] + cc];
}
#endif

/* This version folds ic & wip into zip, as well as using faster 
   nearest neighbor macro. */
void
project_volume_onto_image_c (Volume* vol, Proj_image* cbi, float scale)
{
    int i, j, k, p;
    float* img = (float*) vol->img;
    double *xip, *yip, *zip;
    double acc2[3],acc3[3];
    double dw;
    double sad_sid_2;
    Proj_matrix *pmat = cbi->pmat;

    /* Rescale image (destructive rescaling) */
    sad_sid_2 = (pmat->sad * pmat->sad) / (pmat->sid * pmat->sid);
    for (i = 0; i < cbi->dim[0]*cbi->dim[1]; i++) {
	cbi->img[i] *= sad_sid_2;	// Speedup trick re: Kachelsreiss
	cbi->img[i] *= scale;		// User scaling
    }

    xip = (double*) malloc (3*vol->dim[0]*sizeof(double));
    yip = (double*) malloc (3*vol->dim[1]*sizeof(double));
    zip = (double*) malloc (3*vol->dim[2]*sizeof(double));

    /* Precompute partial projections here */
    for (i = 0; i < vol->dim[0]; i++) {
	double x = (double) (vol->offset[0] + i * vol->pix_spacing[0]);
	xip[i*3+0] = x * (pmat->matrix[0] + pmat->ic[0] * pmat->matrix[8]);
	xip[i*3+1] = x * (pmat->matrix[4] + pmat->ic[1] * pmat->matrix[8]);
	xip[i*3+2] = x * pmat->matrix[8];
    }
    for (j = 0; j < vol->dim[1]; j++) {
	double y = (double) (vol->offset[1] + j * vol->pix_spacing[1]);
	yip[j*3+0] = y * (pmat->matrix[1] + pmat->ic[0] * pmat->matrix[9]);
	yip[j*3+1] = y * (pmat->matrix[5] + pmat->ic[1] * pmat->matrix[9]);
	yip[j*3+2] = y * pmat->matrix[9];
    }
    for (k = 0; k < vol->dim[2]; k++) {
	double z = (double) (vol->offset[2] + k * vol->pix_spacing[2]);
	zip[k*3+0] = z * (pmat->matrix[2] + pmat->ic[0] * pmat->matrix[10]) 
		+ pmat->ic[0] * pmat->matrix[11] + pmat->matrix[3];
	zip[k*3+1] = z * (pmat->matrix[6] + pmat->ic[1] * pmat->matrix[10]) 
		+ pmat->ic[1] * pmat->matrix[11] + pmat->matrix[7];
	zip[k*3+2] = z * pmat->matrix[10] + pmat->matrix[11];
    }
    
    /* Main loop */
    p = 0;
    for (k = 0; k < vol->dim[2]; k++) {
	for (j = 0; j < vol->dim[1]; j++) {
	    vec3_add3 (acc2, &zip[3*k], &yip[3*j]);
	    for (i = 0; i < vol->dim[0]; i++) {
		vec3_add3 (acc3, acc2, &xip[3*i]);
		dw = 1 / acc3[2];
		acc3[0] = acc3[0] * dw;
		acc3[1] = acc3[1] * dw;
		img[p++] += dw * dw * get_pixel_value_c (cbi, acc3[0], acc3[1]);
	    }
	}
    }
    free (xip);
    free (yip);
    free (zip);
}

void
project_volume_onto_image_b (Volume* vol, Proj_image* cbi, float scale)
{
    int i, j, k, p;
    float* img = (float*) vol->img;
    double wip[3];
    double *xip, *yip, *zip;
    double acc1[3],acc2[3],acc3[3];
    double *x, *y, *z;
    double dw;
    double sad_sid_2;
    Proj_matrix *pmat = cbi->pmat;

    /* Rescale image (destructive rescaling) */
    sad_sid_2 = (pmat->sad * pmat->sad) / (pmat->sid * pmat->sid);
    for (i = 0; i < cbi->dim[0]*cbi->dim[1]; i++) {
	cbi->img[i] *= sad_sid_2;	// Speedup trick re: Kachelsreiss
	cbi->img[i] *= scale;		// User scaling
    }

    x = (double*) malloc (vol->dim[0]*sizeof(double));
    y = (double*) malloc (vol->dim[1]*sizeof(double));
    z = (double*) malloc (vol->dim[2]*sizeof(double));
    xip = (double*) malloc (3*vol->dim[0]*sizeof(double));
    yip = (double*) malloc (3*vol->dim[1]*sizeof(double));
    zip = (double*) malloc (3*vol->dim[2]*sizeof(double));

    /* Precompute partial projections here */
    for (i = 0; i < vol->dim[0]; i++) {
	x[i] = (double) (vol->offset[0] + i * vol->pix_spacing[0]);
	xip[i*3+0] = x[i] * pmat->matrix[0];
	xip[i*3+1] = x[i] * pmat->matrix[4];
	xip[i*3+2] = x[i] * pmat->matrix[8];
	x[i] *= pmat->nrm[0];
    }
    for (j = 0; j < vol->dim[1]; j++) {
	y[j] = (double) (vol->offset[1] + j * vol->pix_spacing[1]);
	yip[j*3+0] = y[j] * pmat->matrix[1];
	yip[j*3+1] = y[j] * pmat->matrix[5];
	yip[j*3+2] = y[j] * pmat->matrix[9];
	y[j] *= pmat->nrm[1];
    }
    for (k = 0; k < vol->dim[2]; k++) {
	z[k] = (double) (vol->offset[2] + k * vol->pix_spacing[2]);
	zip[k*3+0] = z[k] * pmat->matrix[2];
	zip[k*3+1] = z[k] * pmat->matrix[6];
	zip[k*3+2] = z[k] * pmat->matrix[10];
	z[k] *= pmat->nrm[2];
	z[k] = pmat->sad - z[k];
    }
    wip[0] = pmat->matrix[3];
    wip[1] = pmat->matrix[7];
    wip[2] = pmat->matrix[11];
    
    /* Main loop */
    p = 0;
    for (k = 0; k < vol->dim[2]; k++) {
	vec3_add3 (acc1, wip, &zip[3*k]);
	for (j = 0; j < vol->dim[1]; j++) {
	    vec3_add3 (acc2, acc1, &yip[3*j]);
	    for (i = 0; i < vol->dim[0]; i++) {
		vec3_add3 (acc3, acc2, &xip[3*i]);
		dw = 1 / acc3[2];
		acc3[0] = pmat->ic[0] + acc3[0] * dw;
		acc3[1] = pmat->ic[1] + acc3[1] * dw;
		img[p++] += dw * dw * get_pixel_value_c (cbi, acc3[0], acc3[1]);
	    }
	}
    }
    free (x);
    free (y);
    free (z);
    free (xip);
    free (yip);
    free (zip);
}

void
project_volume_onto_image_a (Volume* vol, Proj_image* cbi, float scale)
{
    int i, j, k, p;
    float* img = (float*) vol->img;
    double wip[3];
    double *xip, *yip, *zip;
    double acc1[3],acc2[3],acc3[3];
    double *x, *y, *z;
    double s1, s, sad2;
    Proj_matrix *pmat = cbi->pmat;

    /* Rescale image (destructive rescaling) */
    for (i = 0; i < cbi->dim[0]*cbi->dim[1]; i++) {
	cbi->img[i] *= scale;
    }

    x = (double*) malloc (vol->dim[0]*sizeof(double));
    y = (double*) malloc (vol->dim[1]*sizeof(double));
    z = (double*) malloc (vol->dim[2]*sizeof(double));
    xip = (double*) malloc (3*vol->dim[0]*sizeof(double));
    yip = (double*) malloc (3*vol->dim[1]*sizeof(double));
    zip = (double*) malloc (3*vol->dim[2]*sizeof(double));

    /* Precompute partial projections here */
    for (i = 0; i < vol->dim[0]; i++) {
	x[i] = (double) (vol->offset[0] + i * vol->pix_spacing[0]);
	xip[i*3+0] = x[i] * pmat->matrix[0];
	xip[i*3+1] = x[i] * pmat->matrix[4];
	xip[i*3+2] = x[i] * pmat->matrix[8];
	x[i] *= pmat->nrm[0];
    }
    for (j = 0; j < vol->dim[1]; j++) {
	y[j] = (double) (vol->offset[1] + j * vol->pix_spacing[1]);
	yip[j*3+0] = y[j] * pmat->matrix[1];
	yip[j*3+1] = y[j] * pmat->matrix[5];
	yip[j*3+2] = y[j] * pmat->matrix[9];
	y[j] *= pmat->nrm[1];
    }
    for (k = 0; k < vol->dim[2]; k++) {
	z[k] = (double) (vol->offset[2] + k * vol->pix_spacing[2]);
	zip[k*3+0] = z[k] * pmat->matrix[2];
	zip[k*3+1] = z[k] * pmat->matrix[6];
	zip[k*3+2] = z[k] * pmat->matrix[10];
	z[k] *= pmat->nrm[2];
	z[k] = pmat->sad - z[k];
    }
    wip[0] = pmat->matrix[3];
    wip[1] = pmat->matrix[7];
    wip[2] = pmat->matrix[11];
    sad2 = pmat->sad * pmat->sad;
    
    /* Main loop */
    p = 0;
    for (k = 0; k < vol->dim[2]; k++) {
	vec3_add3 (acc1, wip, &zip[3*k]);
	s = z[k];
	for (j = 0; j < vol->dim[1]; j++) {
	    vec3_add3 (acc2, acc1, &yip[3*j]);
	    s1 = z[k] - y[j];
	    for (i = 0; i < vol->dim[0]; i++) {
		s = s1 - x[i];
		//printf ("%10.10g ", s);
		s = sad2 / (s * s);
		vec3_add3 (acc3, acc2, &xip[3*i]);
		//printf ("%10.10g\n", acc3[2]);
		acc3[0] = pmat->ic[0] + acc3[0] / acc3[2];
		acc3[1] = pmat->ic[1] + acc3[1] / acc3[2];
		img[p++] += s * get_pixel_value_b (cbi, acc3[0], acc3[1]);
	    }
	}
    }
    free (x);
    free (y);
    free (z);
    free (xip);
    free (yip);
    free (zip);
}

void
project_volume_onto_image_reference (Volume* vol, Proj_image* cbi, float scale)
{
    int i, j, k, p;
    double vp[4];   /* vp = voxel position */
    float* img = (float*) vol->img;
    Proj_matrix *pmat = cbi->pmat;
    
    p = 0;
    vp[3] = 1.0;
    for (k = 0; k < vol->dim[2]; k++) {
	vp[2] = (double) (vol->offset[2] + k * vol->pix_spacing[2]);
	for (j = 0; j < vol->dim[1]; j++) {
	    vp[1] = (double) (vol->offset[1] + j * vol->pix_spacing[1]);
	    for (i = 0; i < vol->dim[0]; i++) {
		double ip[3];        /* ip = image position */
		double s;            /* s = projection of vp onto s axis */
		vp[0] = (double) (vol->offset[0] + i * vol->pix_spacing[0]);
		mat43_mult_vec3 (ip, pmat->matrix, vp);
		ip[0] = pmat->ic[0] + ip[0] / ip[2];
		ip[1] = pmat->ic[1] + ip[1] / ip[2];
		/* Distance on axis from ctr to source */
		s = vec3_dot (pmat->nrm, vp);
		/* Conebeam weighting factor */
		s = pmat->sad - s;
		s = pmat->sad * pmat->sad / (s * s);
		img[p++] += scale * s * get_pixel_value_b (cbi, ip[0], ip[1]);
	    }
	}
    }
}

void
reconstruct_conebeam (
    Volume* vol, 
    Proj_image_dir *proj_dir, 
    Fdk_options* options
)
{
    int i;
    int num_imgs;
    float scale;

    num_imgs = 1 + (options->last_img - options->first_img)
	/ options->skip_img;

    scale = (float) (sqrt(3) / (double) num_imgs);
    scale = scale * options->scale;

    for (i = options->first_img; 
	 i <= options->last_img; 
	 i += options->skip_img)
    {
	Proj_image* cbi;
	printf ("Loading image %d\n", i);
	cbi = proj_image_dir_load_image (proj_dir, i);
	printf ("done.\n");
	
	// printf ("Projecting Image %d\n", i);
	// project_volume_onto_image_reference (vol, cbi, scale);
	// project_volume_onto_image_a (vol, cbi, scale);
	// project_volume_onto_image_b (vol, cbi, scale);
	project_volume_onto_image_c (vol, cbi, scale);
	proj_image_free (cbi);
    }
}

int 
main (int argc, char* argv[])
{
    Fdk_options options;
    Volume* vol;
    Proj_image_dir *proj_dir;
    
    /* Parse command line arguments */
    fdk_parse_args (&options, argc, argv);

    /* Look for input files */
    proj_dir = proj_image_dir_create (options.input_dir);
    if (!proj_dir) {
	print_and_exit ("Error: couldn't find input files in directory %s\n",
	    options.input_dir);
    }

    /* Allocate memory */
    vol = my_create_volume (&options);

    printf ("Reconstructing...\n");
    switch (options.threading) {
#if (BROOK_FOUND)
    case THREADING_BROOK:
	fdk_brook_c (vol, proj_dir, &options);
	break;
#endif
#if (CUDA_FOUND)
    case THREADING_CUDA:
	CUDA_reconstruct_conebeam (vol, proj_dir, &options);
	break;
#endif
    case THREADING_CPU:
    default:
	reconstruct_conebeam (vol, proj_dir, &options);
    }

    /* Free memory */
    proj_image_dir_destroy (proj_dir);

    /* Prepare HU values in output volume */
    convert_to_hu (vol, &options);

    /* Write output */
    printf ("Writing output volume(s)...\n");
    write_mha (options.output_file, vol);
    write_coronal_sagittal (&options, vol);

    /* Free memory */
    volume_free (vol);

    printf(" done.\n\n");

    return 0;
}

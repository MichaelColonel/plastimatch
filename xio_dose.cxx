/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

#include <itksys/SystemTools.hxx>
#include <itksys/Directory.hxx>
#include <itksys/RegularExpression.hxx>
#include "itkDirectory.h"
#include "itkRegularExpressionSeriesFileNames.h"

#include "cxt.h"
#include "file_util.h"
#include "plm_image.h"
#include "plm_image_type.h"
#include "plm_image_patient_position.h"
#include "print_and_exit.h"
#include "xio_ct.h"
#include "xio_dose.h"
#include "xio_io.h"
#include "xio_structures.h"

#define XIO_DATATYPE_UINT32 5

typedef struct xio_dose_header Xio_dose_header;
struct xio_dose_header {
    int dim[3];
    float offset[3];
    float spacing[3];
    double dose_scale_factor;
    double dose_weight;
    int header_size;
    int header_pos_start_geometry;
    int header_pos_end_geometry;
};

static void
xio_dose_load_header (Xio_dose_header *xdh, const char *filename)
{
    FILE *fp;

    int i;
    int rc;
    int dummy;
    /* Header info */
    Xio_version xio_dose_version;
    int xio_dose_datatype;
    int xio_sources;
    double xio_dose_scalefactor, xio_dose_weight;
    /* Dose cube definition */
    double rx; double ry; double rz;
    double ox; double oy; double oz;
    int nx; int ny; int nz;
    /* Element spacing */
    double dx; double dy; double dz;
    /* Offset */
    double topx; double topy; double topz;

    char line1[1024];

    fp = fopen (filename, "rb");
    if (!fp) {
	print_and_exit ("Error opening file %s for read\n", filename);
    }

    /* XiO file format version */
    fgets (line1, sizeof(line1), fp);

    if (!strncmp (line1, "006d101e", strlen ("006d101e"))) {
        xio_dose_version = XIO_VERSION_4_5_0;
    } else if (!strncmp (line1, "006a101e", strlen ("006a101e"))) {
	/* ?? */
        xio_dose_version = XIO_VERSION_4_33_02;
    } else if (!strncmp (line1, "0062101e", strlen ("0062101e"))) {
        xio_dose_version = XIO_VERSION_4_33_02;
    } else if (!strncmp (line1, "004f101e", strlen ("004f101e"))) {
        xio_dose_version = XIO_VERSION_4_2_1;
    } else {
        xio_dose_version = XIO_VERSION_UNKNOWN;
    }

    if (xio_dose_version == XIO_VERSION_UNKNOWN) {
        print_and_exit ("Error. Unknown XiO file format version: %s\n", line1);
    }

    /* Skip line */
    fgets (line1, sizeof(line1), fp);

    if (xio_dose_version == XIO_VERSION_4_33_02 || xio_dose_version == XIO_VERSION_4_5_0)
    {
	/* Skip line */
	fgets (line1, sizeof(line1), fp);
    }

    /* Number of subplans or beams */
    fgets (line1, sizeof(line1), fp);
    rc = sscanf (line1, "%d", &xio_sources);

    if (rc != 1) {
        print_and_exit ("Error. Cannot parse sources/subplans: %s\n", line1);
    }

    printf ("Dose file is a sum of %d sources/subplans:\n", xio_sources);

    /* One line for each source/subplan */
    for (i = 1; i <= xio_sources; i++) {
        fgets (line1, sizeof(line1), fp);
        printf ("Source/subplan %d: %s", i, line1);
    }

    /* Dose normalization info */
    fgets (line1, sizeof(line1), fp);
    rc = sscanf (line1, "%lf,%lf", &xio_dose_scalefactor, &xio_dose_weight);

    if (rc != 2) {
        print_and_exit ("Error. Cannot parse dose normalization: %s\n", line1);
    }

    printf ("Dose scale factor = %f\n", xio_dose_scalefactor);
    printf ("Dose weight = %f\n", xio_dose_weight);

    /* Skip line */
    fgets (line1, sizeof(line1), fp);

    /* Data type */
    fgets (line1, sizeof(line1), fp);
    rc = sscanf (line1, "%1d", &xio_dose_datatype);

    if (rc != 1) {
        print_and_exit ("Error. Cannot parse datatype: %s\n", line1);
    }

    if (xio_dose_datatype != XIO_DATATYPE_UINT32) {
        print_and_exit ("Error. Only unsigned 32-bit integer data is currently supported: %s\n", line1);
    }

    /* Set start position of geometry */
    xdh->header_pos_start_geometry = ftell(fp);

    /* Dose cube definition */
    fgets (line1, sizeof(line1), fp);

    rc = sscanf (line1, "%d,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d,%d",
                 &dummy, &rx, &rz, &ry, &ox, &oz, &oy, &nx, &nz, &ny);

    if (rc != 10) {
        print_and_exit ("Error. Cannot parse dose cube definition: %s\n", line1);
    }

    printf ("rx = %lf, ry = %lf, rz = %lf\n", rx, ry, rz);
    printf ("ox = %lf, oy = %lf, oz = %lf\n", ox, oy, oz);
    printf ("nx = %d, ny = %d, nz = %d\n", nx, ny, nz);

    /* Set end position of geometry */
    xdh->header_pos_end_geometry = ftell(fp);

    /* Calculate element spacing */
    dx = rx / (nx - 1);
    dy = ry / (ny - 1);
    dz = rz / (nz - 1);

    /* Calculate offset */
    topx = ox - (rx / 2);
    topy = oy - (ry / 2);
    topz = -oz - (rz / 2);

    /* Put info into header */
    xdh->dim[0] = nx;
    xdh->dim[1] = nz;
    xdh->dim[2] = ny;

    xdh->spacing[0] = dx;
    xdh->spacing[1] = dz;
    xdh->spacing[2] = dy;

    xdh->offset[0] = topx;
    xdh->offset[1] = topz;
    xdh->offset[2] = topy;

    xdh->dose_scale_factor = xio_dose_scalefactor;
    xdh->dose_weight = xio_dose_weight;

    /* Get size of full header */
    rc = fseek(fp, - nx * ny * nz * sizeof(unsigned int), SEEK_END);
    if (rc == -1) {
	print_and_exit ("Error seeking backward when reading XiO dose header\n");
    }
    xdh->header_size = ftell(fp);

    fclose (fp);
}

static void
xio_dose_load_cube (
    Plm_image *pli,
    Xio_dose_header *xdh,
    const char *filename
)
{
    FILE *fp;
    Volume *v;
    unsigned int *cube_img_read;
    int i, j, k, rc;

    v = (Volume*) pli->m_gpuit;
    cube_img_read = (unsigned int*) v->img;

    fp = fopen (filename, "rb");
    if (!fp) {
	print_and_exit ("Error opening file %s for read\n", filename);
    }

    /* Read dose cube */
    rc = fseek (fp, - v->dim[0] * v->dim[1] * v->dim[2] * sizeof(unsigned int), SEEK_END);
    if (rc == -1) {
	print_and_exit ("Error seeking backward when reading image file\n");
    }
    rc = fread (cube_img_read, sizeof(unsigned int), v->dim[0] * v->dim[1] * v->dim[2], fp);
    if (rc != v->dim[0] * v->dim[1] * v->dim[2]) {
	perror ("File error: ");
	print_and_exit (
	    "Error reading xio dose cube (%s)\n"
	    "  rc = %d, ferror = %d\n", 
	    filename, rc, ferror (fp));
    }

    /* Switch big-endian to little-endian */
    for (i = 0; i < v->dim[0] * v->dim[1] * v->dim[2]; i++) {
	char lenbuf[4];
	char tmpc;
	memcpy (lenbuf, (char*) &cube_img_read[i], 4);
	tmpc = lenbuf[0]; lenbuf[0] = lenbuf[3]; lenbuf[3] = tmpc;
	tmpc = lenbuf[1]; lenbuf[1] = lenbuf[2]; lenbuf[2] = tmpc;
	memcpy ((char*) &cube_img_read[i], lenbuf, 4);
    }

    /* Flip XiO Z axis */
    Volume* vflip;
    vflip = volume_create (v->dim, v->offset, v->pix_spacing, v->pix_type, v->direction_cosines, 0);

    for (k=0;k<v->dim[2];k++) {
	for (j=0;j<v->dim[1];j++) {
	    for (i=0;i<v->dim[0];i++) {
		memcpy ((float*)vflip->img
		    + volume_index (vflip->dim, i, (vflip->dim[1]-1-j), k), 
		    (float*)v->img 
		    + volume_index (v->dim, i, j, k), v->pix_size);
	    }
	}
    }

    pli->set_gpuit (vflip);
    volume_destroy (v);

    /* Convert volume to float for more accurate normalization */
    pli->convert (PLM_IMG_TYPE_GPUIT_FLOAT);

    /* Normalize dose. Factor 0.01 is to convert from cGy to Gy */
    volume_scale (vflip, xdh->dose_weight * xdh->dose_scale_factor * 0.01);

    fclose (fp);
}

static void
xio_dose_create_volume (
    Plm_image *pli, 
    Xio_dose_header *xdh
)
{
    Volume *v;

    v = volume_create (xdh->dim, xdh->offset, xdh->spacing, PT_UINT32, 0, 0);
    pli->set_gpuit (v);

    printf ("img: %p\n", v->img);
    printf ("Image dim: %d %d %d\n", v->dim[0], v->dim[1], v->dim[2]);
}

void
xio_dose_load (Plm_image *pli, const char *filename)
{
    Xio_dose_header xdh;
    
    xio_dose_load_header(&xdh, filename);
    xio_dose_create_volume(pli, &xdh);
    xio_dose_load_cube(pli, &xdh, filename);
}

void
xio_dose_save (
    Plm_image *pli,  
    Xio_ct_transform *transform,
    const char *filename,
    const char *filename_template
)
{
    /* Because XiO dose files can contain huge amounts of information,
       most of which is not used by plastimatch, the only feasible
       saving method is to copy over the header from the input dose file.
       This means that saving XiO dose is only possible when the input
       is also XiO dose.

       The line in the dose header that defines is geometry will be
       adjusted to the new geometry */

    FILE *fp, *fpt;

    int i, j, k;

    char header;
    size_t result;

    Xio_dose_header xdh;

    Volume *v;
    v = (Volume*) pli->m_gpuit;

    /* Dose cube definition */
    double rx; double ry; double rz;
    double ox; double oy; double oz;
    int nx; int ny; int nz;

    make_directory_recursive (filename);
    fp = fopen (filename, "wb");
    if (!fp) {
	print_and_exit ("Error opening file %s for write\n", filename);
    }

    fpt = fopen (filename_template, "rb");
    if (!fpt) {
	print_and_exit ("Error opening file %s for read\n", filename_template);
    }

    xio_dose_load_header(&xdh, filename_template);

    /* Write first part of header */
    for (i = 0; i < xdh.header_size; i++) {
        result = fread (&header, sizeof(header), 1, fpt);
        if (result != 1) {
            print_and_exit ("Error. Cannot read dose template header (1).\n");
        }
        fwrite (&header, sizeof(header), 1, fp);
    }

    /* Write dose cube definition */
    rx = v->pix_spacing[0] * (v->dim[0] - 1);
    ry = v->pix_spacing[2] * (v->dim[2] - 1);
    rz = v->pix_spacing[1] * (v->dim[1] - 1);

    ox = (v->offset[0] + (rx / 2)) - transform->x_offset;
    oy = (v->offset[2] + (ry / 2)) - transform->y_offset;
    oz = - (v->offset[1] + (rz / 2));

    if ( (transform->patient_pos == PATIENT_POSITION_HFS) ||
	 (transform->patient_pos == PATIENT_POSITION_UNKNOWN) ) {
	ox =   ox * v->direction_cosines[0];
	oy =   oy * v->direction_cosines[8];
	oz =   oz * v->direction_cosines[4];
    } else if (transform->patient_pos == PATIENT_POSITION_HFP) {
	ox = - ox * v->direction_cosines[0];
	oy =   oy * v->direction_cosines[8];
	oz = - oz * v->direction_cosines[4];
    } else if (transform->patient_pos == PATIENT_POSITION_FFS) {
	ox = - ox * v->direction_cosines[0];
	oy = - oy * v->direction_cosines[8];
	oz =  oz * v->direction_cosines[4];
    } else if (transform->patient_pos == PATIENT_POSITION_FFP) {
	ox =   ox * v->direction_cosines[0];
	oy = - oy * v->direction_cosines[8];
	oz = - oz * v->direction_cosines[4];
    }

    nx = v->dim[0];
    ny = v->dim[2];
    nz = v->dim[1];

    fprintf (fp, "%d,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d,%d\n",
	0, rx, rz, ry, ox, oz, oy, nx, nz, ny);

    /* Write second part of header */
    fseek (fpt, xdh.header_pos_end_geometry, SEEK_SET);
    for (i = 0; i < xdh.header_size - xdh.header_pos_end_geometry; i++) {
        result = fread (&header, sizeof(header), 1, fpt);
        if (result != 1) {
            print_and_exit ("Error. Cannot read dose template header (2).\n");
        }
        fwrite (&header, sizeof(header), 1, fp);
    }

    /* Create new volume for output */
    Volume* v_write;
    v_write = volume_create (v->dim, v->offset, v->pix_spacing, v->pix_type, v->direction_cosines, 0);

    /* Clone volume and flip XiO Z axis */
    for (k=0;k<v->dim[2];k++) {
	for (j=0;j<v->dim[1];j++) {
	    for (i=0;i<v->dim[0];i++) {
		memcpy ((float*)v_write->img
		    + volume_index (v_write->dim, i, (v_write->dim[1]-1-j), k), 
		    (float*)v->img 
		    + volume_index (v->dim, i, j, k), v->pix_size);
	    }
	}
    }

    /* Convert to floating point */
    volume_convert_to_float (v_write);

    /* Apply normalization backwards */
    volume_scale (v_write, 1 / (xdh.dose_weight * xdh.dose_scale_factor * 0.01));

    /* Convert to unsigned 32-bit integer */
    volume_convert_to_uint32 (v_write);
    unsigned int *cube_img_write = (unsigned int*) v_write->img;

    /* Switch little-endian to big-endian */
    for (i = 0; i < v_write->dim[0] * v_write->dim[1] * v_write->dim[2]; i++) {
	char lenbuf[4];
	char tmpc;
	memcpy (lenbuf, (char*) &cube_img_write[i], 4);
	tmpc = lenbuf[0]; lenbuf[0] = lenbuf[3]; lenbuf[3] = tmpc;
	tmpc = lenbuf[1]; lenbuf[1] = lenbuf[2]; lenbuf[2] = tmpc;
	memcpy ((char*) &cube_img_write[i], lenbuf, 4);
    }

    /* Write dose cube */
    /* FIX: Not taking direction cosines into account */
    result = fwrite (cube_img_write, sizeof(unsigned int),
	v_write->dim[0] * v_write->dim[1] * v_write->dim[2], fp);
    if (result != (size_t) (v_write->dim[0] * v_write->dim[1] * v_write->dim[2])) {
	print_and_exit ("Error. Cannot write dose cube to %s.\n", filename);
    }

    fclose(fp);
    fclose(fpt);

    volume_destroy (v_write);
}

void
xio_dose_apply_transform (Plm_image *pli, Xio_ct_transform *transform)
{
    int i;

    Volume *v;
    v = (Volume*) pli->m_gpuit;

    /* Set patient position */
    pli->m_patient_pos = transform->patient_pos;

    /* Set offsets */
    v->offset[0] = (v->offset[0] * transform->direction_cosines[0]) + transform->x_offset;
    v->offset[1] = (v->offset[1] * transform->direction_cosines[4]) + transform->y_offset;

    /* Set direction cosines */
    for (i = 0; i <= 8; i++) {
    	v->direction_cosines[i] = transform->direction_cosines[i];
    }
}

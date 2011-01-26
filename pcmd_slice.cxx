/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <time.h>
#include "itkImageRegionIterator.h"
#include "getopt.h"
#include "pcmd_slice.h"
#include "plm_image.h"
#include "plm_image_header.h"
#include "thumbnail.h"

static void
slice_main (Slice_parms* parms)
{
    Plm_image *pli;

    /* Load image */
    pli = plm_image_load ((const char*) parms->img_in_fn, 
	PLM_IMG_TYPE_ITK_FLOAT);

    /* Make thumbnail */
    Thumbnail thumbnail;
    thumbnail.set_input_image (pli);
    thumbnail.set_thumbnail_dim (parms->thumbnail_dim);
    thumbnail.set_thumbnail_spacing (parms->thumbnail_spacing);
    if (parms->have_slice_loc) {
	thumbnail.set_slice_loc (parms->slice_loc);
    }
    pli->m_itk_float = thumbnail.make_thumbnail ();

    /* Save the output file */
    pli->save_image ((const char*) parms->img_out_fn);

    delete (pli);
}

static void
slice_print_usage (void)
{
    printf (
	"Usage: plastimatch slice [options] input-file\n"
	"Options:\n"
	"  --input file\n"
	"  --output file\n"
	"  --thumbnail-dim size\n"
	"  --thumbnail-spacing size\n"
	"  --slice-loc location\n"
    );
    exit (-1);
}

static void
slice_parse_args (Slice_parms* parms, int argc, char* argv[])
{
    int rc, ch;
    static struct option longopts[] = {
	{ "input",          required_argument,      NULL,           1 },
	{ "thumbnail-dim",  required_argument,      NULL,           2 },
	{ "thumbnail_dim",  required_argument,      NULL,           2 },
	{ "thumbnail-spacing",required_argument,    NULL,           3 },
	{ "thumbnail_spacing",required_argument,    NULL,           3 },
	{ "output",         required_argument,      NULL,           4 },
	{ "slice-loc",      required_argument,      NULL,           5 },
	{ "slice_loc",      required_argument,      NULL,           5 },
	{ NULL,             0,                      NULL,           0 }
    };

    while ((ch = getopt_long (argc, argv, "", longopts, NULL)) != -1) {
	switch (ch) {
	case 1:
	    parms->img_in_fn = optarg;
	    break;
	case 2:
	    rc = sscanf (optarg, "%d", &parms->thumbnail_dim);
	    if (rc != 1) {
		printf ("Error: %s requires an argument", argv[optind]);
		slice_print_usage ();
	    }
	    break;
	case 3:
	    rc = sscanf (optarg, "%f", &parms->thumbnail_spacing);
	    if (rc != 1) {
		printf ("Error: %s requires an argument", argv[optind]);
		slice_print_usage ();
	    }
	    break;
	case 4:
	    parms->img_out_fn = optarg;
	    break;
	case 5:
	    rc = sscanf (optarg, "%f", &parms->slice_loc);
	    if (rc != 1) {
		printf ("Error: %s requires an argument", argv[optind]);
		slice_print_usage ();
	    }
	    parms->have_slice_loc = true;
	    break;
	default:
	    break;
	}
    }
    if (parms->img_in_fn.length() == 0) {
	optind ++;   /* Skip plastimatch command argument */
	if (optind < argc) {
	    parms->img_in_fn = argv[optind];
	} else {
	    printf ("Error: must specify input file\n");
	    slice_print_usage ();
	}
    }
}

void
do_command_slice (int argc, char *argv[])
{
    Slice_parms parms;
    
    slice_parse_args (&parms, argc, argv);

    slice_main (&parms);
}

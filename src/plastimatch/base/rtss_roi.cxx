/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plm_math.h"
#include "pstring.h"
#include "rtss_roi.h"

Rtss_contour::Rtss_contour ()
{
    this->slice_no = -1;
    this->ct_slice_uid = "";
    this->num_vertices = 0;
    this->x = 0;
    this->y = 0;
    this->z = 0;
}

Rtss_contour::~Rtss_contour ()
{
    free (this->x);
    free (this->y);
    free (this->z);

    this->slice_no = -1;
    this->ct_slice_uid = "";
    this->num_vertices = 0;
    this->x = 0;
    this->y = 0;
    this->z = 0;
}

Rtss_roi::Rtss_roi ()
{
    this->id = -1;
    this->bit = 0;
    this->num_contours = 0;
    this->pslist = 0;
}

Rtss_roi::~Rtss_roi ()
{
    this->clear ();
}

void
Rtss_roi::clear ()
{
    for (size_t i = 0; i < this->num_contours; i++) {
	delete this->pslist[i];
    }
    free (this->pslist);

    this->name = "";
    this->color = "";
    this->id = -1;
    this->bit = 0;
    this->num_contours = 0;
    this->pslist = 0;
}

Rtss_contour*
Rtss_roi::add_polyline ()
{
    Rtss_contour* new_polyline;

    this->num_contours++;
    this->pslist = (Rtss_contour**) realloc (this->pslist, 
	    this->num_contours * sizeof(Rtss_contour*));

    new_polyline 
	= this->pslist[this->num_contours - 1]
	= new Rtss_contour;
    return new_polyline;
}

void
Rtss_roi::adjust_name (Pstring *name_out, const Pstring *name_in)
{
    int i;

    *name_out = *name_in;

    /* GE Adv sim doesn't like names with strange punctuation. */
    /* 3D Slicer color table doesn't allow spaces */
    for (i = 0; i < name_in->length(); i++) {
	if (isalnum ((*name_in)[i])) {
	    (*name_out)[i] = (*name_in)[i];
	} else {
	    (*name_out)[i] = '_';
	}
    }
}

void
Rtss_roi::set_color (const char* color_string)
{
    int r, g, b;
    if (3 == sscanf (color_string, "%d %d %d", &r, &g, &b)) {
	/* Parsed OK */
    }
    else if (3 == sscanf (color_string, "%d\\%d\\%d", &r, &g, &b)) {
	/* Parsed OK */
    } else {
	r = 255;
	g = 0;
	b = 0;
    }

    this->color.format ("%d %d %d", r, g, b);
}

void
Rtss_roi::get_dcm_color_string (Pstring *dcm_color) const
{
    int r, g, b;
    this->structure_rgb (&r, &g, &b);
    dcm_color->format ("%d\\%d\\%d", r, g, b);
}

void
Rtss_roi::structure_rgb (int *r, int *g, int *b) const
{
    *r = 255;
    *g = 0;
    *b = 0;
    if (this->color.empty()) {
	return;
    }

    /* Ignore return code -- unparsed values will remain unassigned */
    sscanf (this->color, "%d %d %d", r, g, b);
}

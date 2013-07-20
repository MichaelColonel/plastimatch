/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aperture.h"
#include "ion_beam.h"
#include "ion_parms.h"
#include "ion_plan.h"
#include "plm_image.h"
#include "plm_math.h"
#include "print_and_exit.h"
#include "string_util.h"

class Ion_parms_private {
public:
    /* Plan */
    Ion_plan::Pointer plan;

    /* [BEAM] */
    float src[3];
    float isocenter[3];
    float prescription_min;
    float prescription_max;
    double max_depth;
    double depth_res;

    /* [APERTURE] */
    float vup[3];
    int ires[2];
    float ap_offset;
    float ap_have_origin;
    float ap_origin[2];
    float ap_spacing[2];

    /* [PEAK] */
    bool have_manual_peaks;
    double E0;
    double spread;
    double weight;

    std::string ap_filename;
    std::string rc_filename;
public:
    Ion_parms_private () {
        /* GCS FIX: Copy-paste with wed_parms.cxx */
        this->src[0] = -1000.f;
        this->src[1] = 0.f;
        this->src[2] = 0.f;
        this->isocenter[0] = 0.f;
        this->isocenter[1] = 0.f;
        this->isocenter[2] = 0.f;
        this->depth_res = 1.f;
        this->max_depth = 800.0f;
        this->prescription_min = 50.0f;
        this->prescription_max = 100.0f;

        this->vup[0] = 0.f;
        this->vup[1] = 0.f;
        this->vup[2] = 1.f;
        this->ires[0] = 200;
        this->ires[1] = 200;
#if defined (commentout)
        this->have_ic = false;
        this->ic[0] = 99.5f;
        this->ic[1] = 99.5f;
#endif
        this->ap_offset = 100;
        this->ap_have_origin = false;
        this->ap_origin[0] = 0.;
        this->ap_origin[1] = 0.;
        this->ap_spacing[0] = 1.;
        this->ap_spacing[1] = 1.;

        this->have_manual_peaks = false;
        this->E0 = 0.;
        this->spread = 0.;

        this->plan = Ion_plan::New ();
    }
};

Ion_parms::Ion_parms ()
{
    this->d_ptr = new Ion_parms_private;

    this->threading = THREADING_CPU_OPENMP;
    this->flavor = 'a';

    this->debug = 0;
    this->detail = 0;
    this->ray_step = 1.0f;
    this->scale = 1.0f;
}

Ion_parms::~Ion_parms ()
{
}

static void
print_usage (void)
{
    printf (
        "Usage: proton_dose [options] config_file\n"
        "Options:\n"
        " --debug           Create various debug files\n"
    );
    exit (1);
}

int
Ion_parms::set_key_val (
    const char* key, 
    const char* val, 
    int section
)
{
    switch (section) {

    /* [SETTINGS] */
    case 0:
        if (!strcmp (key, "flavor")) {
            if (strlen (val) >= 1) {
                this->flavor = val[0];
            } else {
                goto error_exit;
            } 
        }
        else if (!strcmp (key, "threading")) {
            if (!strcmp (val,"single")) {
                this->threading = THREADING_CPU_SINGLE;
            }
            else if (!strcmp (val,"openmp")) {
#if (OPENMP_FOUND)
                this->threading = THREADING_CPU_OPENMP;
#else
                this->threading = THREADING_CPU_SINGLE;
#endif
            }
            else if (!strcmp (val,"cuda")) {
#if (CUDA_FOUND)
                this->threading = THREADING_CUDA;
#elif (OPENMP_FOUND)
                this->threading = THREADING_CPU_OPENMP;
#else
                this->threading = THREADING_CPU_SINGLE;
#endif
            }
            else {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "ray_step")) {
            if (sscanf (val, "%f", &this->ray_step) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "scale")) {
            if (sscanf (val, "%f", &this->scale) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "detail")) {
            if (!strcmp (val, "low")) {
                this->detail = 1;
            }
            else if (!strcmp (val, "high")) {
                this->detail = 0;
            }
            else {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "patient")) {
            this->input_ct_fn = val;
        }
        else if (!strcmp (key, "dose")) {
            this->output_dose_fn = val;
        }
        else {
            goto error_exit;
        }
        break;

    /* [BEAM] */
    case 1:
        if (!strcmp (key, "bragg_curve")) {
            d_ptr->plan->beam->load (val);
        }
        else if (!strcmp (key, "pos")) {
            int rc = sscanf (val, "%f %f %f", 
                &d_ptr->src[0], &d_ptr->src[1], &d_ptr->src[2]);
            if (rc != 3) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "isocenter")) {
            int rc = sscanf (val, "%f %f %f", &d_ptr->isocenter[0],
                &d_ptr->isocenter[1], &d_ptr->isocenter[2]);
            if (rc != 3) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "max_depth")) {
            if (sscanf (val, "%lf", &(d_ptr->max_depth)) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "depth_res")) {
            if (sscanf (val, "%lf", &(d_ptr->depth_res)) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "prescription_min")) {
            int rc = sscanf (val, "%f", &d_ptr->prescription_min);
            if (rc != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "prescription_max")) {
            int rc = sscanf (val, "%f", &d_ptr->prescription_max);
            if (rc != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "debug")) {
            d_ptr->plan->beam->set_debug (val);
        }
        else {
            goto error_exit;
        }
        break;

    /* [APERTURE] */
    case 2:
        if (!strcmp (key, "up")) {
            if (sscanf (val, "%f %f %f", &d_ptr->vup[0], 
                    &d_ptr->vup[1], &d_ptr->vup[2]) != 3)
            {
                goto error_exit;
            }
        }
#if defined (commentout)
        else if (!strcmp (key, "center")) {
            if (sscanf (val, "%f %f", &d_ptr->ic[0], &d_ptr->ic[1]) != 2) {
                goto error_exit;
            }
        }
#endif
        else if (!strcmp (key, "offset")) {
            if (sscanf (val, "%f", &d_ptr->ap_offset) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "origin")) {
            if (sscanf (val, "%f %f", 
                    &d_ptr->ap_origin[0], &d_ptr->ap_origin[1]) != 2) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "resolution")) {
            if (sscanf (val, "%i %i", &d_ptr->ires[0], &d_ptr->ires[1]) != 2) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "spacing")) {
            if (sscanf (val, "%f %f", 
                    &d_ptr->ap_spacing[0], &d_ptr->ap_spacing[1]) != 2) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "aperture")) {
            d_ptr->ap_filename = val;
        }
        else if (!strcmp (key, "range_compensator")) {
            d_ptr->rc_filename = val;
        }
        else {
            goto error_exit;
        }
        break;

        /* [PEAK] */
    case 3:
        if (!strcmp (key, "energy")) {
            if (sscanf (val, "%lf", &(d_ptr->E0)) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "spread")) {
            if (sscanf (val, "%lf", &(d_ptr->spread)) != 1) {
                goto error_exit;
            }
        }
        else if (!strcmp (key, "weight")) {
            if (sscanf (val, "%lf", &(d_ptr->weight)) != 1) {
                goto error_exit;
            }
        }
        else {
            goto error_exit;
        }
        break;

    }
    return 0;

  error_exit:
    print_and_exit ("Unknown (key,val) combination: (%s,%s)\n", key, val);
    return -1;
}

void
Ion_parms::handle_end_of_section (int section)
{
    switch (section) {
    case 0:
        /* Settings */
        break;
    case 1:
        /* Beam */
        break;
    case 2:
        /* Aperture */
        break;
    case 3:
        /* Peak */
        d_ptr->plan->beam->add_peak (
            d_ptr->E0, d_ptr->spread, d_ptr->depth_res, 
            d_ptr->max_depth, d_ptr->weight);
        d_ptr->have_manual_peaks = true;
        break;
    }
}

Ion_plan::Pointer& 
Ion_parms::get_plan ()
{
    return d_ptr->plan;
}

void
Ion_parms::parse_config (
    const char* config_fn
)
{
    /* Read file into string */
    std::ifstream t (config_fn);
    std::stringstream buffer;
    buffer << t.rdbuf();

    std::string buf;
    std::string buf_ori;    /* An extra copy for diagnostics */
    int section = 0;

    std::stringstream ss (buffer.str());

    while (getline (ss, buf)) {
        buf_ori = buf;
        buf = trim (buf);
        buf_ori = trim (buf_ori, "\r\n");

        if (buf == "") continue;
        if (buf[0] == '#') continue;

        if (buf[0] == '[') {
            handle_end_of_section (section);
            if (ci_find (buf, "[SETTINGS]") != std::string::npos)
            {
                section = 0;
                continue;
            }
            else if (ci_find (buf, "[BEAM]") != std::string::npos)
            {
                section = 1;
                continue;
            }
            else if (ci_find (buf, "[APERTURE]") != std::string::npos)
            {
                section = 2;
                continue;
            }
            else if (ci_find (buf, "[PEAK]") != std::string::npos) 
            {
                section = 3;
                continue;
            }
            else {
                printf ("Parse error: %s\n", buf_ori.c_str());
            }
        }

        size_t key_loc = buf.find ("=");
        if (key_loc == std::string::npos) {
            continue;
        }

        std::string key = buf.substr (0, key_loc);
        std::string val = buf.substr (key_loc+1);
        key = trim (key);
        val = trim (val);

        if (key != "" && val != "") {
            if (this->set_key_val (key.c_str(), val.c_str(), section) < 0) {
                printf ("Parse error: %s\n", buf_ori.c_str());
            }
        }
    }

    handle_end_of_section (section);
}

bool
Ion_parms::parse_args (int argc, char** argv)
{
    int i;
    for (i=1; i<argc; i++) {
        if (argv[i][0] != '-') break;

        if (!strcmp (argv[i], "--debug")) {
            d_ptr->plan->set_debug (true);
        }
        else {
            print_usage ();
            break;
        }
    }

    if (!argv[i]) {
        print_usage ();
    } else {
        this->parse_config (argv[i]);
    }

    if (this->output_dose_fn == "") {
        fprintf (stderr, "\n** ERROR: Output dose not specified in configuration file!\n");
        return false;
    }

    if (this->input_ct_fn == "") {
        fprintf (stderr, "\n** ERROR: Patient image not specified in configuration file!\n");
        return false;
    }

    /* load the patient and insert into the plan */
    Plm_image *ct = plm_image_load (this->input_ct_fn.c_str(), 
        PLM_IMG_TYPE_ITK_FLOAT);
    if (!ct) {
        fprintf (stderr, "\n** ERROR: Unable to load patient volume.\n");
        return false;
    }
    d_ptr->plan->set_patient (ct);

    /* generate depth dose curve, might be manual peaks or 
       automatically optimized */
    if (d_ptr->have_manual_peaks) {
        if (!d_ptr->plan->beam->generate ()) {
            return false;
        }
    } else {
        d_ptr->plan->beam->set_sobp_prescription_min_max (
            d_ptr->prescription_min, d_ptr->prescription_max);
        d_ptr->plan->beam->optimize_sobp ();
    }

    /* set plan parameters */
    d_ptr->plan->beam->set_source_position (d_ptr->src);
    d_ptr->plan->beam->set_isocenter_position (d_ptr->isocenter);

    d_ptr->plan->get_aperture()->set_distance (d_ptr->ap_offset);
    d_ptr->plan->get_aperture()->set_dim (d_ptr->ires);
    d_ptr->plan->get_aperture()->set_spacing (d_ptr->ap_spacing);
    if (d_ptr->ap_have_origin) {
        d_ptr->plan->get_aperture()->set_origin (d_ptr->ap_origin);
    }
    if (d_ptr->ap_filename != "") {
        d_ptr->plan->get_aperture()->set_aperture_image (
            d_ptr->ap_filename.c_str());
    }
    if (d_ptr->rc_filename != "") {
        d_ptr->plan->get_aperture()->set_range_compensator_image (
            d_ptr->rc_filename.c_str());
    }

    /* try to setup the plan with the provided parameters */
    d_ptr->plan->set_step_length (this->ray_step);
    if (!d_ptr->plan->init ()) {
        fprintf (stderr, "ERROR: Unable to initilize plan.\n");
        return false;
    }

    printf ("parse_args complete.\n");

    return true;
}
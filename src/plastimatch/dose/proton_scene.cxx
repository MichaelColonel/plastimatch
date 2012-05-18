/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmdose_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "plmbase.h"
#include "plmdose.h"

Proton_Scene::Proton_Scene ()
{
    this->ap = new Aperture;
    this->beam = new Proton_Beam;
    this->pmat = new Proj_matrix;

    this->patient = NULL;
    this->rpl_vol = NULL;
}

Proton_Scene::~Proton_Scene ()
{
    delete this->ap;
    delete this->beam;
    delete this->pmat;
    if (this->rpl_vol) {
        rpl_volume_destroy (this->rpl_vol);
    }
}

bool
Proton_Scene::init (int ray_step)
{
    double tmp[3];       
    double ps[2] = { 1., 1. };  /* NEEDS TO BE MOVED */

    if (!this->ap) return false;
    if (!this->beam) return false;
    if (!this->patient) return false;

    /* build projection matrix */
    proj_matrix_set (
            this->pmat,
            this->beam->src,
            this->beam->isocenter,
            this->ap->vup,
            this->ap->ap_offset,
            this->ap->ic,
            ps,
            this->ap->ires
    );

    /* populate aperture orientation unit vectors */
    proj_matrix_get_nrm (this->pmat, this->ap->nrm);
    proj_matrix_get_pdn (this->pmat, this->ap->pdn);
    proj_matrix_get_prt (this->pmat, this->ap->prt);

    /* compute position of aperture in room coordinates */
    vec3_scale3 (tmp, this->ap->nrm, - this->pmat->sid);
    vec3_add3 (this->ap->ic_room, this->pmat->cam, tmp);

    /* compute incremental change in 3d position for each change 
       in aperture row/column. */
    vec3_scale3 (this->ap->incr_c, this->ap->prt, ps[1]);
    vec3_scale3 (this->ap->incr_r, this->ap->pdn, ps[0]);

    /* get position of upper left pixel on panel */
    vec3_copy (this->ap->ul_room, this->ap->ic_room);
    vec3_scale3 (tmp, this->ap->incr_r, - this->pmat->ic[0]);
    vec3_add2 (this->ap->ul_room, tmp);
    vec3_scale3 (tmp, this->ap->incr_c, - this->pmat->ic[1]);
    vec3_add2 (this->ap->ul_room, tmp);

    /* create the depth volume */
    this->rpl_vol = rpl_volume_create (
            this->patient,            /* CT volume */
            this->pmat,               /* from source to aperature  */
            this->ap->ires,           /* aperature dimension       */
            this->pmat->cam,          /* position of source        */
            this->ap->ul_room,        /* position of aperature     */
            this->ap->incr_r,         /* aperature row++ vector    */
            this->ap->incr_c,         /* aperature col++ vector    */
            ray_step                  /* step size along ray trace */
    );
    if (!this->rpl_vol) return false;

    /* scan through aperture to fill in rpl_volume */
    rpl_volume_compute (this->rpl_vol, this->patient);

    return true;
}

void
Proton_Scene::set_patient (Volume* ct_vol)
{
    this->patient = ct_vol;
}

void
Proton_Scene::print ()
{
    Aperture* ap = this->ap;
    Proton_Beam* beam = this->beam;

    printf ("BEAM\n");
    printf ("  -- [POS] Location : %g %g %g\n", beam->src[0], beam->src[1], beam->src[2]);
    printf ("  -- [POS] Isocenter: %g %g %g\n", beam->isocenter[0], beam->isocenter[1], beam->isocenter[2]);
    printf ("APERATURE\n");
    printf ("  -- [NUM] Res   : %i %i\n", ap->ires[0], ap->ires[1]);
    printf ("  -- [DIS] Offset: %g\n", ap->ap_offset);
    printf ("  -- [POS] Center: %g %g %g\n", ap->ic_room[0], ap->ic_room[1], ap->ic_room[2]);
    printf ("  -- [POS] UpLeft: %g %g %g\n", ap->ul_room[0], ap->ul_room[1], ap->ul_room[2]);
    printf ("  -- [VEC] Up    : %g %g %g\n", ap->vup[0], ap->vup[1], ap->vup[2]);
    printf ("  -- [VEC] Normal: %g %g %g\n", ap->nrm[0], ap->nrm[1], ap->nrm[2]);
    printf ("  -- [VEC] Right : %g %g %g\n", ap->prt[0], ap->prt[1], ap->prt[2]);
    printf ("  -- [VEC] Down  : %g %g %g\n", ap->pdn[0], ap->pdn[1], ap->pdn[2]);
    printf ("  -- [VEC] col++ : %g %g %g\n", ap->incr_c[0], ap->incr_c[1], ap->incr_c[2]);
    printf ("  -- [VEC] row++ : %g %g %g\n", ap->incr_r[0], ap->incr_r[1], ap->incr_r[2]);
}
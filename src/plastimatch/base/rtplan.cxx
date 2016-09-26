/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <limits>
#include <set>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "plm_image_header.h"
#include "plm_int.h"
#include "plm_math.h"
#include "rtplan_beam.h"
#include "rtplan.h"
#include "rt_study_metadata.h"
#include "string_util.h"

Rtplan::Rtplan()
{
}

Rtplan::~Rtplan()
{
    this->clear ();
}

void
Rtplan::init(void)
{
    this->clear ();
}

void
Rtplan::clear(void)
{
    for (size_t i = 0; i < this->beamlist.size(); i++) {
        delete this->beamlist[i];
    }
    this->beamlist.clear ();
}

/* Add structure (if it doesn't already exist) */
Rtplan_beam*
Rtplan::add_beam (
    const std::string& beam_name,     
    int beam_id)
{
    Rtplan_beam* new_beam;

    new_beam = this->find_beam_by_id(beam_id);
    if (new_beam) {
        return new_beam;
    }

    new_beam = new Rtplan_beam;
    new_beam->name = beam_name;
    if (beam_name == "") {
        new_beam->name = "Unknown beam";
    }
    new_beam->name = string_trim (new_beam->name);

    this->beamlist.push_back (new_beam);
    return new_beam;
}

void
Rtplan::delete_beam(int index)
{
    delete this->beamlist[index];
    this->beamlist.erase (this->beamlist.begin() + index);
}

Rtplan_beam*
Rtplan::find_beam_by_id (size_t index)
{
    if (index < this->beamlist.size()) {
        return this->beamlist[index];
    }
    return 0;
}

void 
Rtplan::set_beam_name (size_t index, const std::string& name)
{
    if (index < this->beamlist.size()) {
        this->beamlist[index]->name = name;
    }
}

std::string
Rtplan::get_beam_name(size_t index)
{
    if (index < this->beamlist.size()) {
        return this->beamlist[index]->name;
    } else {
        return "";
    }
}

void
Rtplan::debug(void)
{   
}

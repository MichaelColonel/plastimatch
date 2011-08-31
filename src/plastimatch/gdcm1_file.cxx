/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <stdlib.h>
#include "gdcmFile.h"
#include "gdcmFileHelper.h"
#include "img_metadata.h"

void
meta_from_gdcm_file (
    Img_metadata *meta, 
    gdcm::File gdcm_file, 
    unsigned short group, 
    unsigned short elem)
{
}

std::string
gdcm_file_GetEntryValue (gdcm::File *file, unsigned short group, 
    unsigned short elem)
{
    return file->GetEntryValue (group, elem);
}

const std::string&
gdcm_file_GDCM_UNKNOWN ()
{
    return gdcm::GDCM_UNKNOWN;
}

const std::string&
gdcm_file_GDCM_UNFOUND ()
{
    return gdcm::GDCM_UNFOUND;
}
/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <stdlib.h>
#if defined (commentout)
#include "gdcmFile.h"
#include "gdcmFileHelper.h"
#include "gdcmGlobal.h"
#include "gdcmSeqEntry.h"
#include "gdcmSQItem.h"
#include "gdcmUtil.h"
#endif

#include "gdcm_file.h"
#include "gdcm_rtss.h"
#include "gdcm_series.h"
#include "math_util.h"
#include "plm_uid_prefix.h"
#include "plm_version.h"
#include "print_and_exit.h"
#include "referenced_dicom_dir.h"
#include "rtss_polyline_set.h"

Referenced_dicom_dir::Referenced_dicom_dir ()
{
    this->m_loaded = 0;
}

Referenced_dicom_dir::~Referenced_dicom_dir ()
{
}

void
Referenced_dicom_dir::load (const char *dicom_dir)
{
    Gdcm_series gs;
    std::string tmp;

    if (!dicom_dir) {
	return;
    }

    gs.load (dicom_dir);
    gs.digest_files ();
    if (!gs.m_have_ct) {
	return;
    }
    gdcm::File* file = gs.get_ct_slice ();

    /* Add geometry */
    int d;
    float offset[3], spacing[3];
    this->m_loaded = 1;
    /* Convert double to float */
    for (d = 0; d < 3; d++) {
	offset[d] = gs.m_origin[d];
	spacing[d] = gs.m_spacing[d];
    }
    this->m_pih.set_from_gpuit (offset, spacing, gs.m_dim, 0);

    /* PatientName */
    this->m_demographics.set_from_gdcm_file (file, 0x0010, 0x0010);

    /* PatientID */
    this->m_demographics.set_from_gdcm_file (file, 0x0010, 0x0020);

    /* PatientSex */
    this->m_demographics.set_from_gdcm_file (file, 0x0010, 0x0040);

    /* StudyID */
    tmp = gdcm_file_GetEntryValue (file, 0x0020, 0x0010);
    if (tmp != gdcm_file_GDCM_UNFOUND()) {
	this->m_study_id = tmp.c_str();
    }

    /* StudyInstanceUID */
    tmp = gdcm_file_GetEntryValue (file, 0x0020, 0x000d);
    this->m_ct_study_uid = tmp.c_str();

    /* SeriesInstanceUID */
    tmp = gdcm_file_GetEntryValue (file, 0x0020, 0x000e);
    this->m_ct_series_uid = tmp.c_str();
	
    /* FrameOfReferenceUID */
    tmp = gdcm_file_GetEntryValue (file, 0x0020, 0x0052);
    this->m_ct_fref_uid = tmp.c_str();

    /* Slice uids */
    gs.get_slice_uids (&this->m_ct_slice_uids);
}

void
Referenced_dicom_dir::get_slice_info (
    int *slice_no,                  /* Output */
    CBString *ct_slice_uid,         /* Output */
    float z                         /* Input */
) const
{
    if (!this->m_loaded) {
	*slice_no = -1;
	return;
    }

    /* NOTE: This algorithm doesn't work if there are duplicate slices */
    *slice_no = ROUND_INT ((z - this->m_pih.m_origin[2]) 
	/ this->m_pih.m_spacing[2]);
    if (*slice_no < 0 || *slice_no >= this->m_pih.Size(2)) {
	*slice_no = -1;
	return;
    }

    (*ct_slice_uid) = this->m_ct_slice_uids[*slice_no];
}

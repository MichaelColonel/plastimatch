/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include "dcmtk_config.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"

#include "dcmtk_module.h"
#include "dcmtk_metadata.h"
#include "dicom_util.h"
#include "metadata.h"
#include "plm_uid_prefix.h"

void
Dcmtk_module::set_patient (
    DcmDataset *dataset, const Metadata::Pointer& meta)
{
    dcmtk_copy_from_metadata (dataset, meta, DCM_PatientName, "");
    dcmtk_copy_from_metadata (dataset, meta, DCM_PatientID, "");
    dataset->putAndInsertString (DCM_PatientBirthDate, "");
    dcmtk_copy_from_metadata (dataset, meta, DCM_PatientSex, "O");
}

void
Dcmtk_module::set_general_study (
    DcmDataset *dataset, 
    const Rt_study_metadata::Pointer& rsm)
{
    dataset->putAndInsertString (DCM_StudyInstanceUID, 
        rsm->get_study_uid());
    dataset->putAndInsertOFStringArray (DCM_StudyDate, 
        rsm->get_study_date());
    dataset->putAndInsertOFStringArray (DCM_StudyTime, 
        rsm->get_study_time());
    dataset->putAndInsertString (DCM_ReferringPhysicianName, "");
    dcmtk_copy_from_metadata (dataset, rsm->get_study_metadata(), 
        DCM_StudyID, "");
    dataset->putAndInsertOFStringArray (DCM_AccessionNumber, "");
    dcmtk_copy_from_metadata (dataset, rsm->get_study_metadata (),
        DCM_StudyDescription, "");
}

void
Dcmtk_module::set_general_series_sro (
    DcmDataset *dataset, 
    const Rt_study_metadata::Pointer& rsm)
{
    dataset->putAndInsertOFStringArray (DCM_Modality, "REG");
    dataset->putAndInsertString (DCM_SeriesInstanceUID, 
        dicom_uid(PLM_UID_PREFIX).c_str());
    dataset->putAndInsertString (DCM_SeriesNumber, "");
}

void
Dcmtk_module::set_rt_series (
    DcmDataset *dataset,
    const Metadata::Pointer& meta,
    const char* modality)
{
    dataset->putAndInsertOFStringArray (DCM_Modality, modality);
    /* Series Instance UID, this gets copied from e.g. 
        d_ptr->rt_study_metadata->get_dose_series_uid(), 
        in order to correctly make cross references between series.
        It is safe to set here, and allow caller to override. */
    dataset->putAndInsertString (DCM_SeriesInstanceUID, 
        dicom_uid(PLM_UID_PREFIX).c_str());
    dcmtk_copy_from_metadata (dataset, meta, DCM_SeriesNumber, "");
    dcmtk_copy_from_metadata (dataset, meta, DCM_SeriesDescription, "");
    /* Series Date, Series Time go here */
    dataset->putAndInsertString (DCM_OperatorsName, "");
}

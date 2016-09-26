/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdlib.h>
#include <stdio.h>
#include "dcmtk_config.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"

#include "dcmtk_file.h"
#include "dcmtk_metadata.h"
#include "dcmtk_module.h"
#include "dcmtk_rt_study.h"
#include "dcmtk_rt_study_p.h"
#include "dcmtk_rtplan.h"
#include "dcmtk_series.h"
#include "file_util.h"
#include "logfile.h"
#include "metadata.h"
#include "plm_uid_prefix.h"
#include "plm_version.h"
#include "print_and_exit.h"
#include "rtplan_control_pt.h"
#include "rtplan_beam.h"
#include "string_util.h"

PLMBASE_C_API bool
dcmtk_rtplan_probe(const char *rtplan_fn)
{
    DcmFileFormat dfile;

    /* Suppress warning messages */
    OFLog::configure(OFLogger::FATAL_LOG_LEVEL);

    OFCondition ofrc = dfile.loadFile(rtplan_fn, EXS_Unknown, EGL_noChange);

    /* Restore error messages -- n.b. dcmtk doesn't have a way to
    query current setting, so I just set to default */
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);

    if (ofrc.bad()) {
        return false;
    }

    const char *c;
    DcmDataset *dset = dfile.getDataset();
    ofrc = dset->findAndGetString(DCM_Modality, c);
    if (ofrc.bad() || !c) {
        return false;
    }

    if (strncmp(c, "RTPLAN", strlen("RTPLAN"))) {
        return false;
    }
    else {
        return true;
    }
}

void
Dcmtk_rt_study::rtplan_load(void)
{
    Dcmtk_series *ds_rtplan = d_ptr->ds_rtplan;    

    d_ptr->rtplan = Rtplan::New();

    /* Modality -- better be RTSTRUCT */
    std::string modality = ds_rtplan->get_modality();
    if (modality == "RTPLAN") {
        lprintf("Trying to load rt plan.\n");
    }
    else {
        print_and_exit("Oops.\n");
    }

    /* FIX: load metadata such as patient name, etc. */

    /*const char *val2 = ds_rtplan->get_cstr(DCM_PatientName);
      const char *val3 = ds_rtplan->get_cstr(DCM_PatientID);*/


    /* Load Beam sequence */

    DcmSequenceOfItems *seq = 0;
    bool rc = ds_rtplan->get_sequence(DCM_BeamSequence, seq);
    if (!rc) {
        return;
    }
    unsigned long iNumOfBeam = seq->card();
    for (unsigned long i = 0; i < iNumOfBeam; i++) {
        Rtplan_beam *curr_beam;
        OFCondition orc;
        const char *strVal = 0;
        long int iVal = 0;

        int beam_id = 0;
        std::string strBeamName;

        DcmItem *item = seq->getItem(i);
        orc = item->findAndGetLongInt(DCM_BeamNumber, iVal);
        if (!orc.good()){
            continue;
        }
        beam_id = iVal;


        orc = item->findAndGetString(DCM_BeamName, strVal);
        if (!orc.good()){
            continue;
        }

        strBeamName = strVal;            
        strVal = 0;

        curr_beam = d_ptr->rtplan->add_beam(strBeamName, beam_id);

        DcmSequenceOfItems *cp_seq = 0;
        orc = item->findAndGetSequence(DCM_ControlPointSequence, cp_seq);

        unsigned long iNumOfCP = cp_seq->card();

        for (unsigned long j = 0; j <iNumOfCP; j++) {                
            DcmItem *c_item = cp_seq->getItem(j);

            c_item->findAndGetLongInt(DCM_ControlPointIndex, iVal);
            //std::string strIsocenter;
            Rtplan_control_pt* curr_cp = curr_beam->add_control_pt ();

            /* ContourGeometricType */
            orc = c_item->findAndGetString(DCM_IsocenterPosition,strVal);
            if (!orc.good()){
                continue;
            }

            float iso_pos[3];
            int rc = parse_dicom_float3 (iso_pos, strVal);
            if (!rc) {
                curr_cp->set_isocenter (iso_pos);
            }
            strVal = 0;

            /*to be implemented*/
            //Get Beam Energy
            //Get Gantry Angle
            //Get Collimator openning
            //GetTable positions
            //Get MLC positions
            //Get CumulativeMetersetWeight
        }

        if (iNumOfCP > 0){                
            if (!curr_beam->check_isocenter_identical()){
                /* action: isonceter of the control points are not same. */
            }
        }            
    }    
}

void
Dcmtk_rt_study::save_rtplan (const char *dicom_dir)
{
    /* Required modules (ref DICOM PS3.3 2016c)
       Patient
       General Study
       RT Series
       Frame of Reference
       General Equipment
       RT General Plan
       SOP Common
    */

    /* Prepare varibles */
    const Rt_study_metadata::Pointer& rsm = d_ptr->rt_study_metadata;
    const Metadata::Pointer& rtplan_metadata = rsm->get_rtplan_metadata ();
    std::string s; // Dummy string

    /* Prepare dcmtk */
    OFCondition ofc;
    DcmFileFormat fileformat;
    DcmDataset *dataset = fileformat.getDataset();

    /* Patient module, general study module */
    Dcmtk_module::set_patient (dataset, rsm->get_study_metadata ());
    Dcmtk_module::set_general_study (dataset, rsm);

    /* RT series module */
    Dcmtk_module::set_rt_series (dataset, rtplan_metadata, "RTPLAN");

    /* Frame of reference module */
    dataset->putAndInsertString (DCM_FrameOfReferenceUID, 
        rsm->get_frame_of_reference_uid());
    dataset->putAndInsertString (DCM_PositionReferenceIndicator, "");

    /* General equipment module */
    Dcmtk_module::set_general_equipment (dataset);

    /* RT general plan module */
    dataset->putAndInsertString (DCM_RTPlanLabel, "TESTONLY");
    dataset->putAndInsertString (DCM_RTPlanName, "TESTONLY");
    dataset->putAndInsertString (DCM_RTPlanDescription, "This is only a test");
    dataset->putAndInsertString (DCM_RTPlanDate, "");
    dataset->putAndInsertString (DCM_RTPlanTime, "");

    /* GCS TODO: Add support for PATIENT at some point */
    // dataset->putAndInsertString (DCM_RTPlanGeometry, "PATIENT");
    dataset->putAndInsertString (DCM_RTPlanGeometry, "TREATMENT_DEVICE");

    /* SOP common module */
    /* GCS TODO: Figure out whether to use Plan or Ion Plan */
    // dataset->putAndInsertString (DCM_SOPClassUID, UID_RTPlanStorage);
    dataset->putAndInsertString (DCM_SOPClassUID, UID_RTIonPlanStorage);
    dataset->putAndInsertString (DCM_SOPInstanceUID, 
        d_ptr->rt_study_metadata->get_dose_instance_uid());
    dataset->putAndInsertOFStringArray(DCM_InstanceCreationDate, 
        d_ptr->rt_study_metadata->get_study_date());
    dataset->putAndInsertOFStringArray(DCM_InstanceCreationTime, 
        d_ptr->rt_study_metadata->get_study_time());

    /* GCS TODO: RT fraction scheme module, RT prescription module */
    
    /* RT ion beams module */
    Rtplan::Pointer& rtplan = d_ptr->rtplan;
    for (size_t b = 0; b < rtplan->num_beams; b++) {
        DcmItem *ib_item = 0;
        dataset->findOrCreateSequenceItem (
            DCM_IonBeamSequence, ib_item, -2);
        s = PLM_to_string (b);
        ib_item->putAndInsertString (DCM_BeamNumber, s.c_str());

        Rtplan_beam *beam = rtplan->beamlist[b];
        ib_item->putAndInsertString (DCM_BeamName, beam->name.c_str());
        ib_item->putAndInsertString (DCM_BeamDescription,
            beam->description.c_str());
        ib_item->putAndInsertString (DCM_BeamType, "STATIC");
        ib_item->putAndInsertString (DCM_RadiationType, "PROTON");
        ib_item->putAndInsertString (DCM_ScanMode, "MODULATED");
#if defined (commentout)
        ib_item->putAndInsertString (DCM_ScanMode, "MODULATED_SPEC");
        ib_item->putAndInsertString (DCM_ModulatedScanModeType,
            "STATIONARY");
#endif
        ib_item->putAndInsertString (DCM_TreatmentMachineName,
            "Reference machine");
        ib_item->putAndInsertString (DCM_PrimaryDosimeterUnit, "NP");
        ib_item->putAndInsertString (DCM_VirtualSourceAxisDistances,
            "2000\\2000");
        ib_item->putAndInsertString (DCM_TreatmentDeliveryType, "TREATMENT");
        ib_item->putAndInsertString (DCM_NumberOfWedges, "0");
        ib_item->putAndInsertString (DCM_NumberOfCompensators, "0");
        ib_item->putAndInsertString (DCM_NumberOfBoli, "0");
        ib_item->putAndInsertString (DCM_NumberOfBlocks, "0");
        ib_item->putAndInsertString (DCM_NumberOfRangeShifters, "0");
        ib_item->putAndInsertString (DCM_NumberOfLateralSpreadingDevices, "0");
        ib_item->putAndInsertString (DCM_NumberOfRangeModulators, "0");
        ib_item->putAndInsertString (DCM_PatientSupportType, "TABLE");

        /* GCS TODO: Still need FinalCumulativeMetersetWeight */

        s = PLM_to_string (beam->num_cp);
        ib_item->putAndInsertString (DCM_NumberOfControlPoints, s.c_str());
        for (size_t c = 0; c < beam->num_cp; c++) {
            DcmItem *cp_item = 0;
            dataset->findOrCreateSequenceItem (
                DCM_IonControlPointSequence, cp_item, -2);
            s = PLM_to_string (c);
            cp_item->putAndInsertString (DCM_ControlPointIndex, s.c_str());

            Rtplan_control_pt *cp = beam->cplist[c];
            s = PLM_to_string (cp->cumulative_meterset_weight);
            cp_item->putAndInsertString (DCM_CumulativeMetersetWeight,
                s.c_str());
            s = PLM_to_string (cp->nominal_beam_energy);
            cp_item->putAndInsertString (DCM_NominalBeamEnergy, s.c_str());
            s = PLM_to_string (cp->gantry_angle);
            cp_item->putAndInsertString (DCM_GantryAngle, s.c_str());
            cp_item->putAndInsertString (DCM_GantryRotationDirection,
                cp->gantry_rotation_direction.c_str());

            /* Dcmtk has no putAndInsertFloat32Array, so we must 
               use more primitive methods */
            DcmFloatingPointSingle *fele
                = new DcmFloatingPointSingle (DCM_ScanSpotPositionMap);
#if __cplusplus >= 201103L
            Float32 *f = cp->scan_spot_position_map.data ();
#else
            Float32 *f = &cp->scan_spot_position_map[0];
#endif
            ofc = fele->putFloat32Array (f, 2);
            ofc = cp_item->insert (fele);
        }
    }
    
    /* ----------------------------------------------------------------- */
    /*     Write the output file                                         */
    /* ----------------------------------------------------------------- */
    std::string filename;
    if (d_ptr->filenames_with_uid) {
        filename = string_format ("%s/rtplan_%s.dcm", dicom_dir,
            d_ptr->rt_study_metadata->get_dose_series_uid());
    } else {
        filename = string_format ("%s/rtplan.dcm", dicom_dir);
    }
    make_parent_directories (filename);

    ofc = fileformat.saveFile (filename.c_str(), EXS_LittleEndianExplicit);
    if (ofc.bad()) {
        print_and_exit ("Error: cannot write DICOM RTPLAN (%s)\n", 
            ofc.text());
    }
}

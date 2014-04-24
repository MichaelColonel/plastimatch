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
#include "dcmtk_loader.h"
#include "dcmtk_loader_p.h"
#include "dcmtk_metadata.h"
#include "dcmtk_rt_study.h"
#include "dcmtk_rt_study_p.h"
#include "dcmtk_series.h"
#include "dcmtk_slice_data.h"
#include "dcmtk_uid.h"
#include "file_util.h"
#include "logfile.h"
#include "rt_study_metadata.h"
#include "plm_image.h"
#include "plm_image_set.h"
#include "plm_image_header.h"
#include "plm_math.h"
#include "plm_uid_prefix.h"
#include "plm_version.h"
#include "print_and_exit.h"
#include "string_util.h"
#include "volume.h"

void
Dcmtk_loader::image_load ()
{
    /* Set up outputs */
    Plm_image_set::Pointer pli_set = Plm_image_set::New();
    Plm_image::Pointer pli = Plm_image::New();
    d_ptr->img = pli;

    /* Make abbreviations */
    Dcmtk_series *ds_image = d_ptr->ds_image;
    const Dcmtk_file_list& flist = ds_image->get_flist ();

    /* Create a container to hold different groups of files */
    std::list<Dcmtk_file_list> group_list;

    /* Insert files into groups according to direction cosines */
    {
        //printf ("----------\n");
        Dcmtk_file_list::const_iterator it;
        for (it = flist.begin(); it != flist.end(); ++it) {
            const Dcmtk_file::Pointer& df = (*it);

            //df->debug ();

            bool match_found = false;
            std::list<Dcmtk_file_list>::iterator grit;
            for (grit = group_list.begin(); grit != group_list.end(); ++grit) {
                Dcmtk_file_list& flp = *grit;
                const Dcmtk_file::Pointer& flp_df = flp.front();

                if (flp_df->get_direction_cosines() 
                    == df->get_direction_cosines())
                {
                    /* Add logic to append to flp */
                    //printf ("Match found.  :)\n");
                    match_found = true;
                    flp.push_back (df);
                    break;
                }
            }
            if (match_found) {
                continue;
            }
            /* Else insert new element into group_list */
            //printf ("Need to insert.\n");
            group_list.push_back (Dcmtk_file_list());
            group_list.back().push_back (df);
        }
        //printf ("----------\n");
    }

    /* Sort each group in Z direction */
    {
        std::list<Dcmtk_file_list>::iterator grit;
        for (grit = group_list.begin(); grit != group_list.end(); ++grit) {
            grit->sort();
        }
    }

    /* Regroup as needed according to inter-slice spacing */
    {
    }

    /* Sort in Z direction */
    ds_image->sort ();

    /* GCS FIX:
       (1) Direction cosines
       (2) Minimum 2 slices
       (3) Consistency of images w/in series
       (4) done
       (5) Different image types
       (6) Refine slice spacing based on entire chunk size
    */

    /* Check for minimum 2 slices */
    if (flist.size() < 2) {
        return;
    }
    
    /* Get first slice */
    std::list<Dcmtk_file::Pointer>::const_iterator it = flist.begin();
    const Dcmtk_file* df = (*it).get();
    float z_init, z_prev, z_diff, z_last;
    int slice_no = 0;
    float best_chunk_z_start = z_init = z_prev = df->get_z_position ();
    
    /* Store UIDs */
    if (d_ptr->m_drs) {
        d_ptr->m_drs->set_ct_series_uid (
            df->get_cstr (DCM_SeriesInstanceUID));
        d_ptr->m_drs->set_frame_of_reference_uid (
            df->get_cstr (DCM_FrameOfReferenceUID));
        d_ptr->m_drs->set_study_uid (
            df->get_cstr (DCM_StudyInstanceUID));
        d_ptr->m_drs->set_study_date (
            df->get_cstr (DCM_StudyDate));
        d_ptr->m_drs->set_study_time (
            df->get_cstr (DCM_StudyTime));

        /* Store remaining metadata */
        Metadata *study_metadata = d_ptr->m_drs->get_study_metadata ();
        dcmtk_copy_into_metadata (study_metadata, df, DCM_PatientName);
        dcmtk_copy_into_metadata (study_metadata, df, DCM_PatientID);
        dcmtk_copy_into_metadata (study_metadata, df, DCM_PatientSex);
        dcmtk_copy_into_metadata (study_metadata, df, DCM_PatientPosition);
        dcmtk_copy_into_metadata (study_metadata, df, DCM_StudyID);

        Metadata *image_metadata = d_ptr->m_drs->get_image_metadata ();
        dcmtk_copy_into_metadata (image_metadata, df, DCM_Modality);
    }

    /* Get next slice */
    ++it; ++slice_no;
    df = (*it).get();
    z_diff = df->get_z_position() - z_prev;
    z_last = z_prev = df->get_z_position();

    /* We want to find the largest chunk with equal spacing.  This will 
       be used to resample in the case of irregular spacing. */
    int this_chunk_start = 0, best_chunk_start = 0;
    float this_chunk_diff = z_diff, best_chunk_diff = z_diff;
    int this_chunk_len = 2, best_chunk_len = 2;

    /* Loop through remaining slices */
    while (++it != flist.end())
    {
	++slice_no;
	df = (*it).get();
	z_diff = df->get_z_position() - z_prev;
	z_last = z_prev = df->get_z_position();

	if (fabs (this_chunk_diff - z_diff) > 0.11) {
	    /* Start a new chunk if difference in thickness is 
	       more than 0.1 millimeter */
	    this_chunk_start = slice_no - 1;
	    this_chunk_len = 2;
	    this_chunk_diff = z_diff;
	} else {
	    /* Same thickness, increase size of this chunk */
	    this_chunk_diff = ((this_chunk_len * this_chunk_diff) + z_diff)
		/ (this_chunk_len + 1);
	    this_chunk_len++;

	    /* Check if this chunk is now the best chunk */
	    if (this_chunk_len > best_chunk_len) {
		best_chunk_start = this_chunk_start;
		best_chunk_len = this_chunk_len;
		best_chunk_diff = this_chunk_diff;
		best_chunk_z_start = z_prev 
		    - (best_chunk_len-1) * best_chunk_diff;
	    }
	}
    }

    /* Report information about best chunk */
    lprintf ("Best chunck:\n  Slices %d to %d from (0 to %d)\n"
	"  Z_loc = %f %f\n" 
	"  Slice spacing = %f\n", 
	best_chunk_start, best_chunk_start + best_chunk_len - 1, slice_no, 
	best_chunk_z_start, 
	best_chunk_z_start + (best_chunk_len - 1) * best_chunk_diff, 
	best_chunk_diff);

    /* Some debugging info */
#if defined (commentout)
    lprintf ("Slices: ");
    for (it = flist.begin(); it != flist.end(); ++it) {
        df = (*it).get();
	lprintf ("%f ", df->get_z_position());
    }
    lprintf ("\n");
#endif

    /* Create a Volume_header to hold the image geometry */
    Volume_header vh;
    plm_long *dim = vh.get_dim();

    /* Compute resampled volume header */
    int slices_before = 
	ROUND_INT ((best_chunk_z_start - z_init) / best_chunk_diff);
    int slices_after = 
	ROUND_INT ((z_last - best_chunk_z_start 
		- (best_chunk_len - 1) * best_chunk_diff) / best_chunk_diff);
    df = (*flist.begin()).get();
    vh.clone (df->get_volume_header());
    dim[2] = slices_before + best_chunk_len + slices_after;
    vh.get_origin()[2] = best_chunk_z_start - slices_before * best_chunk_diff;
    vh.get_spacing()[2] = best_chunk_diff;

    /* Store image header */
    if (d_ptr->m_drs) {
        d_ptr->m_drs->set_image_header (Plm_image_header (vh));
    }

    /* More debugging info */
    //vh.print ();

    /* Still more debugging info */
#if defined (commentout)
    lprintf ("Resamples slices: ");
    for (plm_long i = 0; i < dim[2]; i++) {
	lprintf ("%f ", vh.get_origin()[2] + i * vh.get_spacing()[2]);
    }
    lprintf ("\n");
#endif

    /* Divine image type */
    df = (*flist.begin()).get();
    uint16_t samp_per_pix, bits_alloc, bits_stored, high_bit, pixel_rep;
    const char* phot_interp;
    bool rc = df->get_uint16 (DCM_SamplesPerPixel, &samp_per_pix);
    if (!rc) {
	//return pli;
        return;
    }
    phot_interp = df->get_cstr (DCM_PhotometricInterpretation);
    if (!phot_interp) {
	//return pli;
        return;
    }
    rc = df->get_uint16 (DCM_BitsAllocated, &bits_alloc);
    if (!rc) {
	//return pli;
        return;
    }
    rc = df->get_uint16 (DCM_BitsStored, &bits_stored);
    if (!rc) {
	//return pli;
        return;
    }
    rc = df->get_uint16 (DCM_HighBit, &high_bit);
    if (!rc) {
	//return pli;
        return;
    }
    rc = df->get_uint16 (DCM_PixelRepresentation, &pixel_rep);
    if (!rc) {
	//return pli;
        return;
    }
    lprintf ("Samp_per_pix: %d\n", (int) samp_per_pix);
    lprintf ("Phot_interp: %s\n", phot_interp);
    lprintf ("Bits_alloc: %d\n", (int) bits_alloc);
    lprintf ("Bits_stored: %d\n", (int) bits_stored);
    lprintf ("High_bit: %d\n", (int) high_bit);
    lprintf ("Pixel_rep: %d\n", (int) pixel_rep);

    float rescale_slope, rescale_intercept;
    rc = df->get_ds_float (DCM_RescaleIntercept, &rescale_intercept);
    if (!rc) {
        rescale_intercept = 0;
    }
    rc = df->get_ds_float (DCM_RescaleSlope, &rescale_slope);
    if (!rc) {
        rescale_slope = 1;
    }

    lprintf ("S/I = %f/%f\n", rescale_slope, rescale_intercept);

    /* Some kinds of images we don't know how to deal with.  
       Don't load these. */
    if (samp_per_pix != 1) {
        lprintf ("Sorry, couldn't load image: samp_per_pix\n");
	//return pli;
        return;
    }
    if (strcmp (phot_interp, "MONOCHROME2")) {
        lprintf ("Sorry, couldn't load image: phot_interp\n");
	//return pli;
        return;
    }
    if (bits_alloc != 16) {
        lprintf ("Sorry, couldn't load image: bits_alloc\n");
	//return pli;
        return;
    }
    if (bits_stored != high_bit + 1) {
        lprintf ("Sorry, couldn't load image: bits_stored/high_bit\n");
	//return pli;
        return;
    }
    if (pixel_rep != 0 && pixel_rep != 1) {
        lprintf ("Sorry, couldn't load image: pixel_rep\n");
	//return pli;
        return;
    }

    lprintf ("Image looks ok.  Try to load.\n");

    pli->m_type = PLM_IMG_TYPE_GPUIT_FLOAT;
    pli->m_original_type = PLM_IMG_TYPE_GPUIT_FLOAT;
    Volume* vol = new Volume (vh, PT_FLOAT, 1);

    pli->set_volume (vol);
    float* img = (float*) vol->img;

    for (plm_long i = 0; i < dim[2]; i++) {
	/* Find the best slice, using nearest neighbor interpolation */
	std::list<Dcmtk_file::Pointer>::const_iterator best_slice_it 
            = flist.begin();
	float best_z_dist = FLT_MAX;
	float z_pos = vh.get_origin()[2] + i * vh.get_spacing()[2];
	for (it = flist.begin(); it != flist.end(); ++it) {
	    float this_z_dist = fabs ((*it)->get_z_position() - z_pos);
	    if (this_z_dist < best_z_dist) {
		best_z_dist = this_z_dist;
		best_slice_it = it;
	    }
	}

	/* Load the slice image data into volume */
	const uint16_t* pixel_data;
	df = (*best_slice_it).get();
	unsigned long length;

#if defined (commentout)
	lprintf ("Loading slice z=%f at location z=%f\n",
	    (*best_slice_it)->get_z_position(), z_pos);
#endif

        /* GCS FIX: This should probably use DicomImage::getOutputData()
           cf. http://support.dcmtk.org/docs/mod_dcmimage.html */
	rc = df->get_uint16_array (DCM_PixelData, &pixel_data, &length);
	if (!rc) {
	    print_and_exit ("Oops.  Error reading pixel data.  Punting.\n");
	}
	if (((long) length) != dim[0] * dim[1]) {
	    print_and_exit ("Oops.  Dicom image had wrong length "
		"(%d vs. %d x %d).\n", length, dim[0], dim[1]);
	}

        /* Apply slope and offset */
        for (plm_long j = 0; j < (plm_long) length; j++) {
            img[j] = rescale_slope * pixel_data[j] + rescale_intercept;
        }
	img += length;

	/* Store slice UID */
        if (d_ptr->m_drs) {
            d_ptr->m_drs->set_slice_uid (i, df->get_cstr (DCM_SOPInstanceUID));
        }
    }
    if (d_ptr->m_drs) {
        d_ptr->m_drs->set_slice_list_complete ();
    }
}

static void
dcmtk_save_slice (const Rt_study_metadata::Pointer drs, Dcmtk_slice_data *dsd)
{
    Pstring tmp;
    DcmFileFormat fileformat;
    DcmDataset *dataset = fileformat.getDataset();
    const Metadata *image_metadata = 0;
    if (drs) {
        image_metadata = drs->get_image_metadata ();
    }

    dataset->putAndInsertString (DCM_ImageType, 
        "DERIVED\\SECONDARY\\REFORMATTED");
    dataset->putAndInsertOFStringArray(DCM_InstanceCreationDate, 
        drs->get_study_date());
    dataset->putAndInsertOFStringArray(DCM_InstanceCreationTime, 
        drs->get_study_time());
    dataset->putAndInsertString (DCM_SOPClassUID, UID_CTImageStorage);
    dataset->putAndInsertString (DCM_SOPInstanceUID, dsd->slice_uid);
    dataset->putAndInsertOFStringArray (DCM_StudyDate, drs->get_study_date());
    dataset->putAndInsertOFStringArray (DCM_StudyTime, drs->get_study_time());
    dataset->putAndInsertString (DCM_AccessionNumber, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, DCM_Modality, "CT");
    dataset->putAndInsertString (DCM_Manufacturer, "Plastimatch");
    dataset->putAndInsertString (DCM_ReferringPhysicianName, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, 
        DCM_SeriesDescription, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, DCM_PatientName, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, DCM_PatientID, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, 
        DCM_PatientBirthDate, "");
    dcmtk_copy_from_metadata (dataset, image_metadata, DCM_PatientSex, "");
    dataset->putAndInsertString (DCM_SliceThickness, dsd->sthk.c_str());
    dataset->putAndInsertString (DCM_SoftwareVersions, 
        PLASTIMATCH_VERSION_STRING);
    dcmtk_copy_from_metadata (dataset, image_metadata, 
        DCM_PatientPosition, "HFS");
    dataset->putAndInsertString (DCM_StudyInstanceUID, drs->get_study_uid());
    dataset->putAndInsertString (DCM_SeriesInstanceUID, 
        drs->get_ct_series_uid());
    dcmtk_copy_from_metadata (dataset, image_metadata, DCM_StudyID, "10001");
    dataset->putAndInsertString (DCM_SeriesNumber, "303");
    tmp.format ("%d", dsd->instance_no);
    dataset->putAndInsertString (DCM_InstanceNumber, tmp.c_str());
    //dataset->putAndInsertString (DCM_InstanceNumber, "0");
    /* DCM_PatientOrientation seems to be not required.  */
    // dataset->putAndInsertString (DCM_PatientOrientation, "L\\P");
    dataset->putAndInsertString (DCM_ImagePositionPatient, dsd->ipp);
    dataset->putAndInsertString (DCM_ImageOrientationPatient, dsd->iop);
    dataset->putAndInsertString (DCM_FrameOfReferenceUID, 
        drs->get_frame_of_reference_uid());
    /* XVI 4.5 requires a DCM_PositionReferenceIndicator */
    dataset->putAndInsertString (DCM_PositionReferenceIndicator, "SP");
    dataset->putAndInsertString (DCM_SliceLocation, dsd->sloc.c_str());
    dataset->putAndInsertString (DCM_SamplesPerPixel, "1");
    dataset->putAndInsertString (DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset->putAndInsertUint16 (DCM_Rows, (Uint16) dsd->vol->dim[1]);
    dataset->putAndInsertUint16 (DCM_Columns, (Uint16) dsd->vol->dim[0]);
    tmp.format ("%f\\%f", dsd->vol->spacing[0], dsd->vol->spacing[1]);
    dataset->putAndInsertString (DCM_PixelSpacing, tmp.c_str());
    dataset->putAndInsertString (DCM_BitsAllocated, "16");
    dataset->putAndInsertString (DCM_BitsStored, "16");
    dataset->putAndInsertString (DCM_HighBit, "15");
    dataset->putAndInsertString (DCM_PixelRepresentation, "1");

    tmp.format ("%f", dsd->intercept);
    dataset->putAndInsertString (DCM_RescaleIntercept, tmp.c_str());
    tmp.format ("%f", dsd->slope);
    dataset->putAndInsertString (DCM_RescaleSlope, tmp.c_str());

    //dataset->putAndInsertString (DCM_RescaleIntercept, "-1024");
    //dataset->putAndInsertString (DCM_RescaleSlope, "1");
    dataset->putAndInsertString (DCM_RescaleType, "HU");

    dataset->putAndInsertString (DCM_WindowCenter, "40");
    dataset->putAndInsertString (DCM_WindowWidth, "400");

    /* Convert to 16-bit signed int */
    for (size_t i = 0; i < dsd->slice_size; i++) {
        float f = dsd->slice_float[i];
        //dsd->slice_int16[i] = (int16_t) (f + 1024);
        dsd->slice_int16[i] = (int16_t) 
            ((f - dsd->intercept) / dsd->slope);
    }

    dataset->putAndInsertUint16Array (DCM_PixelData, 
        (Uint16*) dsd->slice_int16, dsd->slice_size);
    OFCondition status = fileformat.saveFile (dsd->fn.c_str(), 
        EXS_LittleEndianExplicit);
    if (status.bad()) {
        print_and_exit ("Error: cannot write DICOM file (%s)\n", 
            status.text());
    }
}

void
Dcmtk_rt_study::save_image (
    const char *dicom_dir)
{
    Dcmtk_slice_data dsd;
    dsd.vol = this->get_image_volume_float();
    dsd.slice_size = dsd.vol->dim[0] * dsd.vol->dim[1];
    dsd.slice_int16 = new int16_t[dsd.slice_size];
    float *dc = dsd.vol->direction_cosines.get();
    dsd.iop.format ("%f\\%f\\%f\\%f\\%f\\%f",
        dc[0], dc[1], dc[2], dc[3], dc[4], dc[5]);

    Plm_image_header pih (dsd.vol.get());
    d_ptr->dicom_metadata->set_image_header (pih);

    /* Find slope / offset on a per-volume basis */
    float vol_min = FLT_MAX;
    float vol_max = - FLT_MAX;
    float *img = (float*) dsd.vol->img;
    bool all_integers = true;
    for (plm_long v = 0; v < dsd.vol->npix; v++) {
        if (vol_min > img[v]) {
            vol_min = img[v];
        }
        else if (vol_max < img[v]) {
            vol_max = img[v];
        }
        if (img[v] != floorf(img[v])) {
            all_integers = false;
        }
    }
    /* Use a heuristic to determine intercept and offset.
       The heuristic is designed around the following principles:
       - prevent underflow when using low-precision DICOM string-encoded
         floating point numbers
       - map integers to integers
    */
    dsd.intercept = floorf (vol_min);
    if (all_integers) {
        dsd.slope = 1;
    }
    else {
        float range = vol_max - dsd.intercept;
        if (range < 1) {
            range = 1;
        }
        dsd.slope = range / (SHRT_MAX - 100);
        std::string tmp = string_format ("%f", dsd.slope);
        sscanf (tmp.c_str(), "%f", &dsd.slope);
    }
    printf ("OFF = %f, SLO = %f\n", dsd.intercept, dsd.slope);

    for (plm_long k = 0; k < dsd.vol->dim[2]; k++) {
        /* GCS FIX: direction cosines */
        float z_loc = dsd.vol->offset[2] + k * dsd.vol->spacing[2];
        dsd.instance_no = k;
        dsd.fn.format ("%s/image%03d.dcm", dicom_dir, (int) k);
        make_directory_recursive (dsd.fn);
        dsd.sthk.format ("%f", dsd.vol->spacing[2]);
        dsd.sloc.format ("%f", z_loc);
        dsd.ipp.format ("%f\\%f\\%f", dsd.vol->offset[0], dsd.vol->offset[1], 
            dsd.vol->offset[2] + k * dsd.vol->spacing[2]);
        dcmtk_uid (dsd.slice_uid, PLM_UID_PREFIX);

        dsd.slice_float = &((float*)dsd.vol->img)[k*dsd.slice_size];
        dcmtk_save_slice (d_ptr->dicom_metadata, &dsd);

        d_ptr->dicom_metadata->set_slice_uid (k, dsd.slice_uid);
    }
    delete[] dsd.slice_int16;
    d_ptr->dicom_metadata->set_slice_list_complete ();
}

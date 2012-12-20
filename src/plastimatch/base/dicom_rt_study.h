/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dicom_rt_study_h_
#define _dicom_rt_study_h_

#include "plmbase_config.h"
#include <list>

#include "plm_int.h"
#include "pstring.h"

class Dicom_rt_study_private;
class Metadata;
class Plm_image;
class Plm_image_header;
class Slice_list;
class Volume;

class PLMBASE_API Dicom_rt_study {
public:
    Dicom_rt_study_private *d_ptr;
public:
    Dicom_rt_study ();
    ~Dicom_rt_study ();
public:
    const char* get_ct_series_uid () const;
    void set_ct_series_uid (const char* uid);
    const char* get_dose_instance_uid () const;
    const char* get_dose_series_uid () const;
    const char* get_frame_of_reference_uid () const;
    void set_frame_of_reference_uid (const char* uid);
    const char* get_rtss_instance_uid () const;
    const char* get_rtss_series_uid () const;
    const char* get_study_date () const;
    void set_study_date (const char* date);
    const char* get_study_time () const;
    void set_study_time (const char* time);
    const char* get_study_uid () const;
    void set_study_uid (const char* uid);
    void set_image_header (const Plm_image_header& pih);
    const char* get_slice_uid (int index) const;
    void set_slice_uid (int index, const char* slice_uid);
    void set_slice_list_complete ();
    const Slice_list *get_slice_list ();
    int num_slices ();

    Metadata *get_study_metadata ();
    const Metadata *get_study_metadata () const;
    Metadata *get_image_metadata ();
    const Metadata *get_image_metadata () const;
    Metadata *get_rtss_metadata ();
    const Metadata *get_rtss_metadata () const;
    Metadata *get_dose_metadata ();
    const Metadata *get_dose_metadata () const;
};

#endif
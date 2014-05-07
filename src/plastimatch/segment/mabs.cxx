/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmsegment_config.h"
#include <list>
#include <stdio.h>
#include <stdlib.h>

#include "dir_list.h"
#include "dice_statistics.h"
#include "distance_map.h"
#include "file_util.h"
#include "hausdorff_distance.h"
#include "itk_adjust.h"
#include "itk_resample.h"
#include "itk_image_save.h"
#include "itk_threshold.h"
#include "logfile.h"
#include "mabs.h"
#include "mabs_atlas_selection.h"
#include "mabs_parms.h"
#include "mabs_staple.h"
#include "mabs_stats.h"
#include "mabs_vote.h"
#include "option_range.h"
#include "path_util.h"
#include "plm_image.h"
#include "plm_image_header.h"
#include "plm_stages.h"
#include "plm_timer.h"
#include "plm_warp.h"
#include "print_and_exit.h"
#include "registration_data.h"
#include "registration_parms.h"
#include "rt_study.h"
#include "rtss.h"
#include "segmentation.h"
#include "string_util.h"
#include "xform.h"

class Mabs_private {
public:
    /* These are the input parameters */
    const Mabs_parms *parms;
    
    /* traindir_base is the output directory when we are 
       doing a training task (i.e. not labeling), and is of the form:
       ".../train_dir */
    std::string traindir_base;

    /* ".../train_dir/convert" */
    std::string convert_dir;
    /* ".../train_dir/atlas-train" */
    std::string atlas_train_dir;
    /* ".../train_dir/prealign" */
    std::string prealign_dir;
    /* ".../train_dir/mabs-train" */
    std::string mabs_train_dir;

    /* segment_input_fn is the input location for a segmentation task */
    std::string segment_input_fn;
    /* outdir_base is the output directory when we are 
       doing a labeling task (i.e. not training) */
    std::string segment_outdir_base;

    /* registration_list is the list of registration parameter files */
    std::list<std::string> registration_list;
    /* output_dir is ??? */
    std::string output_dir;
    /* segmentation_training_dir is of the form:
       training/0035/segmentations/reg_parm.txt/rho_0.5_sigma_1.2/
    */
    std::string segmentation_training_dir;

    /* process_dir_list is a list of the input directories which 
       need to be processed, one directory per case
       - during the atlas_convert stage, it is the list of 
         original (dicom) atlas directories
       - during the atlas_prealign stage, it is the list of 
         converted atlas directories
       - <need more detail here for later stages>
    */
    std::list<std::string> process_dir_list;

    /* There is one reference image at a time, which is the 
       image we are segmenting. */
    std::string ref_id;
    Rt_study::Pointer ref_rtds;
    std::list<std::string> atlas_list;

    /* Select atlas parameters */
    std::map<std::string, std::list<std::pair<std::string, double> > 
             > selected_atlases;

    /* Prealign parameters */
    bool prealign_resample;
    float prealign_spacing[3];

    /* Utility class for keeping track of statistics */
    Mabs_stats stats;

    /* Segmentation parameters */
    std::string registration_id;
    float minsim;
    float rho;
    float sigma;
    std::string threshold_values;

    /* While segmenting an image, we sometimes loop through 
       the structures for evaluation.  This holds the 
       binary image of the current structure which we are 
       evaluating. */
    bool have_ref_structure;
    UCharImageType::Pointer ref_structure_image;

    /* This configures the trainer to evaluate segmentation parameters,
       it is set to false when --train-registration is used */
    bool train_segmentation;

    /* You can set these variables to save some intermediate data 
       for debugging and tuning */
    bool compute_distance_map;
    bool write_weight_files;
    bool write_thresholded_files;
    bool write_distance_map_files;
    bool write_registration_files;
    bool write_warped_images;

    /* While looping through atlases, the gaussin voting/staple information is stored here */
    std::map<std::string, Mabs_vote*> vote_map;
    std::map<std::string, Mabs_staple*> staple_map;

    /* Store timing information for performance evaluation */
    double time_atlas_selection;
    double time_dmap;
    double time_extract;
    double time_io;
    double time_reg;
    double time_vote;
    double time_staple;
    double time_warp_img;
    double time_warp_str;

public:
    Mabs_private () {
        parms = 0;
        train_segmentation = true;
        compute_distance_map = true;
        write_weight_files = false;
        write_thresholded_files = true;
        write_distance_map_files = true;
        write_registration_files = true;
        write_warped_images = true;
        have_ref_structure = false;
        this->reset_timers ();

        prealign_resample = false;

        registration_id = "";
        minsim = 0.0;
        rho = 0.0;
        sigma = 0.0;
        threshold_values = "";

        ref_rtds = Rt_study::New ();
    }
    void reset_timers () {
        time_atlas_selection = 0;
        time_dmap = 0;
        time_extract = 0;
        time_io = 0;
        time_reg = 0;
        time_vote = 0;
        time_staple = 0;
        time_warp_img = 0;
        time_warp_str = 0;
    }
    void clear_vote_map () {
        std::map<std::string, Mabs_vote*>::iterator it;
        for (it = vote_map.begin(); it != vote_map.end(); ++it) {
            delete it->second;
        }
        vote_map.clear ();
    }
public:
    void print_structure_map ();
    std::string map_structure_name (const std::string& ori_name);
    void extract_reference_image (const std::string& mapped_name);
    void segmentation_threshold_weight (
        FloatImageType::Pointer weight_image, 
        const std::string& mapped_name, 
        const std::string& structure_label, 
        float thresh_val);
};

/* Print out structure map */
void
Mabs_private::print_structure_map ()
{
    std::map<std::string, std::string>::const_iterator it;
    for (it = this->parms->structure_map.begin ();
         it != this->parms->structure_map.end (); 
         it++)
    {
        lprintf ("SM> %s\n", (*it).first.c_str());
    }
}

/* Map an input-specific structure name to a canonical structure name 
   Return "" if no canonical name */
std::string
Mabs_private::map_structure_name (
    const std::string& ori_name)
{
    if (this->parms->structure_map.size() == 0) {
        lprintf (" > no structure list specified\n");
        return ori_name;
    }

    std::map<std::string, std::string>::const_iterator it 
        = this->parms->structure_map.find (ori_name);
    if (it == this->parms->structure_map.end()) {
        lprintf (" > irrelevant structure: %s\n", ori_name.c_str());
        return "";
    }

    const std::string& mapped_name = it->second;
    if (mapped_name == "") {
        lprintf (" > irrelevant structure: %s\n", ori_name.c_str());
    }
    else if (mapped_name == ori_name) {
        lprintf (" > relevant structure: %s\n", ori_name.c_str());
    }
    else {
        lprintf (" > relevant structure: %s -> %s\n", 
            ori_name.c_str(), mapped_name.c_str());
    }
    return mapped_name;
}

/* Extract reference structure as binary mask, and save into the 
   variable d_ptr->ref_structure_image.
   This image is used when computing dice statistics. 

   GCS FIX: This is inefficient, it could be extracted 
   once at the beginning, and cached. */
void
Mabs_private::extract_reference_image (const std::string& mapped_name)
{
    this->have_ref_structure = false;
    Segmentation::Pointer rtss = this->ref_rtds->get_rtss();
    if (!rtss) {
        return;
    }
    for (size_t j = 0; j < rtss->get_num_structures(); j++)
    {
        std::string ref_ori_name 
            = rtss->get_structure_name (j);
        std::string ref_mapped_name 
            = this->map_structure_name (ref_ori_name);
        if (ref_mapped_name == mapped_name) {
            this->ref_structure_image = rtss->get_structure_image (j);
            this->have_ref_structure = true;
            break;
        }
    }
}

void
Mabs_private::segmentation_threshold_weight (
    FloatImageType::Pointer weight_image, 
    const std::string& mapped_name, 
    const std::string& structure_label, 
    float thresh_val
)
{
    Plm_timer timer;

    /* Threshold the weight image */
    timer.start();
    UCharImageType::Pointer thresh_img = itk_threshold_above (
        weight_image, thresh_val);
    this->time_vote += timer.report();

    /* Optionally, save the thresholded files */
    if (this->write_thresholded_files) {
        lprintf ("Saving thresholded structures\n");
        std::string thresh_img_fn = string_format (
            "%s/%s_thresh_%f.nrrd", 
            this->segmentation_training_dir.c_str(), 
            structure_label.c_str(), 
            thresh_val);
        timer.start();
        itk_image_save (thresh_img, thresh_img_fn.c_str());
        this->time_io += timer.report();
    }

    /* Extract reference structure as binary mask. */
    timer.start();
    this->extract_reference_image (mapped_name);
    this->time_extract += timer.report();

    /* Compute Dice, etc. */
    if (this->have_ref_structure) {

        std::string stats_string = this->stats.compute_statistics (
            "segmentation", /* Not used yet */
            this->ref_structure_image,
            thresh_img);
        std::string seg_log_string = string_format (
            "%s,reg=%s,struct=%s,"
            "rho=%f,sigma=%f,minsim=%f,thresh=%f,"
            "%s\n",
            this->ref_id.c_str(),
            this->registration_id.c_str(),
            mapped_name.c_str(),
            this->rho,
            this->sigma,
            this->minsim,
            thresh_val,
            stats_string.c_str());
        lprintf ("%s", seg_log_string.c_str());

        /* Update seg_dice file */
        std::string seg_dice_log_fn = string_format (
            "%s/seg_dice.csv",
            this->mabs_train_dir.c_str());
        FILE *fp = fopen (seg_dice_log_fn.c_str(), "a");
        fprintf (fp, "%s", seg_log_string.c_str());
        fclose (fp);
    }
}

Mabs::Mabs () {
    d_ptr = new Mabs_private;
}

Mabs::~Mabs () {
    delete d_ptr;
}

void
Mabs::sanity_checks ()
{
    /* Do a few sanity checks */
    /*
    if (!is_directory (d_ptr->parms->atlas_dir)) {
        print_and_exit ("Atlas dir (%s) is not a directory\n",
            d_ptr->parms->atlas_dir.c_str());
    }
    if (!is_directory (d_ptr->parms->registration_config)) {
        if (!file_exists (d_ptr->parms->registration_config)) {
            print_and_exit ("Couldn't find registration config (%s)\n", 
                d_ptr->parms->registration_config.c_str());
        }
    }
    */
}

void
Mabs::load_process_dir_list (const std::string& dir)
{
    /* Clear process_dir_list to avoid multiple entries in case of multiple
     * calls to this function */
    d_ptr->process_dir_list.clear();

    Dir_list d (dir);
    for (int i = 0; i < d.num_entries; i++)
    {
        /* Skip "." and ".." */
        if (!strcmp (d.entries[i], ".") || !strcmp (d.entries[i], "..")) {
            continue;
        }

        /* Build string containing full path to atlas item */
        std::string path = compose_filename (dir, d.entries[i]);

        /* Only consider directories */
        if (!is_directory (path.c_str())) {
            continue;
        }

        /* Add directory to atlas_dir_list */
        d_ptr->process_dir_list.push_back (path);
    }
}

/* The following variables should be set before running this:
   d_ptr->ref_rtds           the fixed image and its structure set
   d_ptr->atlas_list         list of images that should be registred
   d_ptr->output_dir         directory containing output results
                             (e.g. .../prealign or .../mabs-train)
   d_ptr->registration_list  list of registration command files
*/
void
Mabs::run_registration_loop ()
{
    Plm_timer timer;

    /* Loop through images in the atlas */
    std::list<std::string>::iterator atl_it;
    for (atl_it = d_ptr->atlas_list.begin();
         atl_it != d_ptr->atlas_list.end(); atl_it++)
    {
        Rt_study rtds;
        std::string path = *atl_it;
        std::string input_dir = dirname (path);
        std::string atlas_id = basename (path);
        printf ("%s\n -> %s\n -> %s\n",
            path.c_str(), input_dir.c_str(), atlas_id.c_str());
        std::string atlas_input_path = string_format ("%s/%s",
            input_dir.c_str(), atlas_id.c_str());
        std::string atlas_output_path = string_format ("%s/%s",
            d_ptr->output_dir.c_str(), atlas_id.c_str());

        /* Check if this registration is already complete.
           We might be able to skip it. */
        std::string atl_checkpoint_fn = string_format (
            "%s/checkpoint.txt", atlas_output_path.c_str());
        if (file_exists (atl_checkpoint_fn)) {
            lprintf ("Atlas registration complete for %s\n",
                atlas_output_path.c_str());
            continue;
        }

        /* Load image & structures from "prep" directory */
        timer.start();
        std::string fn = string_format ("%s/img.nrrd", 
            atlas_input_path.c_str());
        rtds.load_image (fn.c_str());
        fn = string_format ("%s/structures", 
            atlas_input_path.c_str());
        rtds.load_prefix (fn.c_str());
        d_ptr->time_io += timer.report();

        /* Inspect the structures -- we might be able to skip the 
           atlas if it has no relevant structures */
        bool can_skip = true;
        Segmentation::Pointer rtss = rtds.get_rtss();
        for (size_t i = 0; i < rtss->get_num_structures(); i++) {
            std::string ori_name = rtss->get_structure_name (i);
            std::string mapped_name = d_ptr->map_structure_name (ori_name);
            if (mapped_name != "") {
                can_skip = false;
                break;
            }
        }
        if (can_skip) {
            lprintf ("No relevant structures. Skipping.\n");
            continue;
        }

        /* Loop through each registration parameter set */
        std::list<std::string>::iterator reg_it;
        for (reg_it = d_ptr->registration_list.begin(); 
             reg_it != d_ptr->registration_list.end(); reg_it++) 
        {
            /* Set up files & directories for this job */
            std::string command_file = *reg_it;
            std::string curr_output_dir;
            std::string registration_id;
            registration_id = basename (command_file);
            curr_output_dir = string_format ("%s/%s",
                atlas_output_path.c_str(),
                registration_id.c_str());

            /* Check if this registration is already complete.
               We might be able to skip it. */
            std::string reg_checkpoint_fn = string_format (
                "%s/checkpoint.txt", curr_output_dir.c_str());
            if (file_exists (reg_checkpoint_fn)) {
                lprintf ("Registration parms complete for %s\n",
                    curr_output_dir.c_str());
                continue;
            }

            /* Make a registration command string */
            lprintf ("Processing command file: %s\n", command_file.c_str());
            std::string command_string = slurp_file (command_file);

            /* Parse the registration command string */
            Registration_parms *regp = new Registration_parms;
            int rc = regp->set_command_string (command_string);
            if (rc) {
                lprintf ("Skipping command file \"%s\" "
                    "due to parse error.\n", command_file.c_str());
                delete regp;
                continue;
            }

            /* Manually set input files */
            Registration_data *regd = new Registration_data;
            regd->fixed_image = Plm_image::New ();
            regd->fixed_image->set_itk (
                d_ptr->ref_rtds->get_image()->itk_float());
            regd->moving_image = Plm_image::New ();
            regd->moving_image->set_itk (
                rtds.get_image()->itk_float());

            /* Run the registration */
            lprintf ("DO_REGISTRATION_PURE\n");
            lprintf ("regp->num_stages = %d\n", regp->num_stages);
            timer.start();
            Xform::Pointer xf_out = do_registration_pure (regd, regp);
            d_ptr->time_reg += timer.report();

            /* Warp the output image */
            lprintf ("Warp output image...\n");
            Plm_image_header fixed_pih (regd->fixed_image);
            Plm_image::Pointer warped_image = Plm_image::New();
            timer.start();
            plm_warp (warped_image, 0, xf_out, &fixed_pih, 
                regd->moving_image, 
                regp->default_value, 0, 1);
            d_ptr->time_warp_img += timer.report();
            
            /* We're done with this */
            delete regp;

            /* Warp the structures */
            lprintf ("Warp structures...\n");
            Plm_image_header source_pih (rtds.get_image().get());
            timer.start();
            //rtss->warp (xf_out, &fixed_pih);
            Segmentation::Pointer warped_rtss 
                = rtss->warp_nondestructive (xf_out, &fixed_pih);
            d_ptr->time_warp_str += timer.report();

            /* Save some debugging information */
            if (d_ptr->write_registration_files) {
                timer.start();
                std::string fn;
                lprintf ("Saving registration_files\n");
                if (d_ptr->write_warped_images) {
                    fn = string_format ("%s/img.nrrd", 
                        curr_output_dir.c_str());
                    warped_image->save_image (fn.c_str());
                }

                fn = string_format ("%s/xf.txt", curr_output_dir.c_str());
                xf_out->save (fn.c_str());

                if (d_ptr->parms->write_warped_structures) {
                    fn = string_format ("%s/structures", 
                        curr_output_dir.c_str());
                    warped_rtss->save_prefix (fn, "nrrd");
                }
                d_ptr->time_io += timer.report();
            }

            /* We're done with these */
            delete regd;

            /* Loop through structures for this atlas image */
            lprintf ("Process structures...\n");
            for (size_t i = 0; i < warped_rtss->get_num_structures(); i++) {
                /* Check structure name, make sure it is something we 
                   want to segment */
                std::string ori_name = warped_rtss->get_structure_name (i);
                std::string mapped_name = d_ptr->map_structure_name (ori_name);
                if (mapped_name == "") {
                    continue;
                }

                /* Extract structure as binary mask */
                timer.start();
                UCharImageType::Pointer structure_image 
                    = warped_rtss->get_structure_image (i);
                d_ptr->time_extract += timer.report();

                /* Make the distance map */
                if (d_ptr->compute_distance_map && d_ptr->parms->fusion_criteria == "gaussian") {
                    timer.start();
                    lprintf ("Computing distance map...\n");
                    this->compute_dmap (structure_image,
                        curr_output_dir, mapped_name);
                }

                /* Extract reference structure as binary mask. */
                timer.start();
                d_ptr->extract_reference_image (mapped_name);
                d_ptr->time_extract += timer.report();

                /* Compute Dice, etc. */
                timer.start();
                if (d_ptr->have_ref_structure) {

                    std::string stats_string 
                        = d_ptr->stats.compute_statistics (
                            registration_id,
                            d_ptr->ref_structure_image,
                            structure_image);
                    std::string reg_log_string = string_format (
                        "%s,%s,reg=%s,struct=%s,%s\n",
                        d_ptr->ref_id.c_str(), 
                        atlas_id.c_str(),
                        registration_id.c_str(),
                        mapped_name.c_str(), 
                        stats_string.c_str());
                    lprintf ("%s", reg_log_string.c_str());

                    /* Update reg_dice file */
                    std::string reg_dice_log_fn = string_format (
                        "%s/reg_dice.csv",
                        d_ptr->output_dir.c_str());
                    FILE *fp = fopen (reg_dice_log_fn.c_str(), "a");
                    fprintf (fp, "%s", reg_log_string.c_str());
                    fclose (fp);
                }
            }

            /* Create checkpoint file which means that this registration
               is complete */
            touch_file (reg_checkpoint_fn);

        } /* end for each registration parameter */

        /* Create checkpoint file which means that training for 
           this atlas example is complete */
        touch_file (atl_checkpoint_fn);
    } /* end for each atlas image */
}

void
Mabs::convert (const std::string& input_dir, const std::string& output_dir)
{
    Rt_study rtds;
    Plm_timer timer;

    /* Load the rtds for the atlas */
    timer.start();
    lprintf ("MABS loading %s\n", input_dir.c_str());
    rtds.load_dicom_dir (input_dir.c_str());
    d_ptr->time_io += timer.report();

    /* Save the image as raw files */
    timer.start();
    std::string fn = string_format ("%s/img.nrrd", output_dir.c_str());
    rtds.get_image()->save_image (fn.c_str());

    /* Remove structures which are not part of the atlas */
    timer.start();
    Segmentation::Pointer rtss = rtds.get_rtss();
    rtss->prune_empty ();
    Rtss *cxt = rtss->get_structure_set_raw ();
    for (size_t i = 0; i < rtss->get_num_structures(); i++) {
        /* Check structure name, make sure it is something we 
           want to segment */
        std::string ori_name = rtss->get_structure_name (i);
        std::string mapped_name = d_ptr->map_structure_name (ori_name);
        lprintf ("Structure i (%s), checking for mapped name\n",
            ori_name.c_str());
        if (mapped_name == "") {
            /* If not, delete it (before rasterizing) */
            lprintf ("Deleted structure %s\n");
            cxt->delete_structure (i);
            --i;
        }
    }

    /* Rasterize structure sets and save */
    Plm_image_header pih (rtds.get_image().get());
    rtss->rasterize (&pih, false, false);
    d_ptr->time_extract += timer.report();

    /* Save structures which are part of the atlas */
    std::string prefix = string_format ("%s/structures", output_dir.c_str());
    rtss->save_prefix (prefix, "nrrd");
    d_ptr->time_io += timer.report();
}

void
Mabs::atlas_convert ()
{
    /* Do a few sanity checks */
    this->sanity_checks ();

    /* Parse atlas directory */
    this->load_process_dir_list (d_ptr->parms->atlas_dir);

    /* Just a little debugging */
    d_ptr->print_structure_map ();

    /* Loop through atlas_dir, converting file formats */
    for (std::list<std::string>::iterator it = d_ptr->process_dir_list.begin();
         it != d_ptr->process_dir_list.end(); it++)
    {
        std::string input_dir = *it;
        std::string atlas_id = basename (input_dir);
        std::string output_dir = string_format (
            "%s/%s", d_ptr->convert_dir.c_str(), 
            atlas_id.c_str());

        this->convert (input_dir, output_dir);
    }
    lprintf ("Rasterization time:   %10.1f seconds\n", d_ptr->time_extract);
    lprintf ("I/O time:             %10.1f seconds\n", d_ptr->time_io);
    lprintf ("MABS prep complete\n");
}

void
Mabs::atlas_selection ()
{
    /* Create and start timer */
    Plm_timer timer;
    timer.start();

    /* Parse atlas directory */
    this->load_process_dir_list (d_ptr->prealign_dir);

    /* Define stuff to save ranking */
    std::map<std::string, std::list<std::pair<std::string, double> > > ranked_atlases; // Only ranked, not selected
    
    std::string atlas_ranking_file_name = 
        string_format ("%s/atlas_ranking.txt", d_ptr->atlas_train_dir.c_str());
   
    bool compute_new_ranking = true;

    /* Check if a precomputed ranking (not specified by user) can be used */
    if (is_directory(d_ptr->atlas_train_dir.c_str()) &&
        file_exists(atlas_ranking_file_name.c_str()) &&
        d_ptr->parms->atlases_from_ranking != -1) {
        
        /* Count lines */
        FILE *count_lines_file = fopen (atlas_ranking_file_name.c_str(), "r");
        char ch;
        int lines_number = 1; /* The last line doesn't have \n */
        
        while ((ch=getc(count_lines_file)) != EOF) {
            if (ch == '\n') ++lines_number;
        }

        fclose(count_lines_file);

        /* If number of lines is equal to number of  atlases use precomputed ranking */
        if (lines_number == (int) d_ptr->process_dir_list.size()) {
            compute_new_ranking = false;
        }
    }

    /* Create atlas-train directory */
    if (compute_new_ranking) {
        make_directory(d_ptr->atlas_train_dir.c_str());
    }

    /* Open log file for atlas selection */
    std::string atlas_selection_log_file_name = string_format ("%s/log_atlas_seletion.txt",
        d_ptr->atlas_train_dir.c_str());
    
    FILE *atlas_selection_log_file = plm_fopen (atlas_selection_log_file_name.c_str(), "w");
    
    if (atlas_selection_log_file == NULL) {
        printf("Error opening atlas selection log file!\n");
        exit(1);
    }

    /* Loop through atlas_dir, choosing reference images to segment */
    for (std::list<std::string>::iterator it = d_ptr->process_dir_list.begin();
         it != d_ptr->process_dir_list.end(); it++)
    {
        /* Create atlas list for this test case */
        std::string path = *it;
        d_ptr->atlas_list = d_ptr->process_dir_list;
        d_ptr->atlas_list.remove (path);

        std::string patient_id = basename (path);
        d_ptr->ref_id = patient_id;
        
        /* Load image & structures from "prep" directory */
        if (compute_new_ranking) {
            std::string fn = string_format ("%s/%s/img.nrrd", 
                d_ptr->prealign_dir.c_str(), patient_id.c_str());
            d_ptr->ref_rtds->load_image (fn.c_str());
        }
        
        /* Create object and set the parameters */
        Mabs_atlas_selection* atlas_selector = new Mabs_atlas_selection();
        atlas_selector->atlas_selection_criteria = d_ptr->parms->atlas_selection_criteria;
        atlas_selector->selection_reg_parms_fn = d_ptr->parms->selection_reg_parms_fn;
        atlas_selector->similarity_percent_threshold = d_ptr->parms->similarity_percent_threshold;
        atlas_selector->max_random_atlases = d_ptr->parms->max_random_atlases;
        atlas_selector->min_random_atlases = d_ptr->parms->min_random_atlases;
        atlas_selector->hist_bins = d_ptr->parms->mi_histogram_bins;
        atlas_selector->atlases_from_ranking = d_ptr->parms->atlases_from_ranking;
        atlas_selector->precomputed_ranking_fn = d_ptr->parms->precomputed_ranking_fn;
        atlas_selector->subject_id = patient_id;
        atlas_selector->atlas_dir = d_ptr->parms->atlas_dir;
        atlas_selector->number_of_atlases = (int) d_ptr->process_dir_list.size();
        
        if (d_ptr->parms->roi_mask_fn.compare("")!=0) { /* Set the mask if defined */
            Plm_image::Pointer mask_plm = plm_image_load (d_ptr->parms->roi_mask_fn, PLM_IMG_TYPE_ITK_UCHAR);
            atlas_selector->mask->SetImage(mask_plm->itk_uchar());
            atlas_selector->mask->Update();
        }

        atlas_selector->min_hist_sub_value_defined = d_ptr->parms->lower_mi_value_sub_defined;
        atlas_selector->min_hist_sub_value = d_ptr->parms->lower_mi_value_sub;
        atlas_selector->max_hist_sub_value_defined = d_ptr->parms->upper_mi_value_sub_defined;
        atlas_selector->max_hist_sub_value = d_ptr->parms->upper_mi_value_sub;
        atlas_selector->min_hist_atl_value_defined = d_ptr->parms->lower_mi_value_atl_defined;
        atlas_selector->min_hist_atl_value = d_ptr->parms->lower_mi_value_atl;
        atlas_selector->max_hist_atl_value_defined = d_ptr->parms->upper_mi_value_atl_defined;
        atlas_selector->max_hist_atl_value = d_ptr->parms->upper_mi_value_atl;
                                                                
        /* New selection is required, execute it */
        if (compute_new_ranking) {
            atlas_selector->subject = d_ptr->ref_rtds->get_image();
            atlas_selector->atlas_dir_list = d_ptr->process_dir_list;
            atlas_selector->run_selection();
        }

        /* Use a precomputed ranking */
        else if (!compute_new_ranking) {
            atlas_selector->precomputed_ranking_fn = atlas_ranking_file_name.c_str();
            atlas_selector->atlases_from_ranking = d_ptr->parms->atlases_from_ranking;
            atlas_selector->precomputed_ranking();
        }

        /* Write into the log file preliminary information about the selection process */
        fprintf(atlas_selection_log_file,
            "Patient = %s, initial atlases = %d, selection criteria = %s \n",
            atlas_selector->subject_id.c_str(),
            atlas_selector->number_of_atlases,
            atlas_selector->atlas_selection_criteria.c_str());
        
        if (!compute_new_ranking) {
            fprintf(atlas_selection_log_file,
                "SELECTION MADE USING A PRECOMPUTED RANKING\n");   
        }
        
        /* Print into the log file information about the selection process */
        fprintf(atlas_selection_log_file,
            "Selected atlases for patient %s: (%d) \n",
            atlas_selector->subject_id.c_str(),
            (int) atlas_selector->selected_atlases.size());
       
        for (std::list<std::pair<std::string, double> >::iterator it_selected_atlases =
                 atlas_selector->selected_atlases.begin();
             it_selected_atlases != atlas_selector->selected_atlases.end();
             it_selected_atlases++) {
        
            fprintf(atlas_selection_log_file,
                "Atlas %s with score value equal to %f \n",
                it_selected_atlases->first.c_str(),
                it_selected_atlases->second);
        }
         
        fprintf(atlas_selection_log_file, "\n");
        
        /* Fill the map structures */
        d_ptr->selected_atlases.insert(std::make_pair(atlas_selector->subject_id,
                atlas_selector->selected_atlases));

        ranked_atlases.insert(std::make_pair(atlas_selector->subject_id,
                atlas_selector->ranked_atlases));

        /* Delete object */
        delete atlas_selector;
    }

    /* Close log file */
    fclose(atlas_selection_log_file);
    
    /* Write the new ranking */
    if (compute_new_ranking) {

        FILE *ranking_file = fopen (atlas_ranking_file_name.c_str(), "w");
       
        /* Cycle over reference images */
        std::map<std::string, std::list<std::pair<std::string, double> > >::iterator it_map;
        for (it_map = ranked_atlases.begin(); it_map != ranked_atlases.end(); it_map++) {
            
            fprintf(ranking_file, "%s: ", it_map->first.c_str());
            
            /* Cycle over atlases */
            for (std::list<std::pair<std::string, double> >::iterator it_list = it_map->second.begin();
                 it_list != it_map->second.end(); it_list++) {
                fprintf(ranking_file, "%s ", it_list->first.c_str());
            }

            /* If it is not the last subject write on a new line */
            if (it_map != (--ranked_atlases.end()))
                fprintf(ranking_file, "\n");
        }
        
        fclose(ranking_file);
    }

    /* Stop timer */
    d_ptr->time_atlas_selection += timer.report();

    printf("Atlas selection done! \n");
}

void
Mabs::atlas_prealign ()
{
    /* Do a few sanity checks */
    this->sanity_checks ();

    /* Open logfile */
    std::string logfile_path = string_format (
        "%s/%s", d_ptr->prealign_dir.c_str(), "logfile.txt");
    logfile_open (logfile_path.c_str(), "a");

    /* Parse directory with registration files */
    this->parse_registration_dir (d_ptr->parms->prealign_registration_config);

    /* Parse convert directory */
    this->load_process_dir_list (d_ptr->convert_dir);
    if (d_ptr->process_dir_list.size() < 2) {
        print_and_exit ("Error.  Prealignment requires at least two "
            "images in the convert directory.");
    }

    /* Identify directory of reference image */
    std::string reference_id;
    std::string reference_dir;
    if (d_ptr->parms->prealign_reference != "") {
        reference_id = d_ptr->parms->prealign_reference;
        reference_dir = string_format (
            "%s/%s", d_ptr->convert_dir.c_str(), 
            d_ptr->parms->prealign_reference.c_str());
        if (!is_directory (reference_dir)) {
            print_and_exit ("Error.  Prealignment reference directory (%s) "
                " was not found.", reference_dir.c_str());
        }
    } else {
        reference_dir = d_ptr->process_dir_list.front();
        reference_id = basename (reference_dir);
    }

    lprintf ("Prealignment reference directory is %s\n",
        reference_dir.c_str());

    /* Load reference image and structures */
    std::string reference_convert_img_fn = string_format (
        "%s/img.nrrd", reference_dir.c_str());
    std::string reference_convert_structures_dir = string_format (
        "%s/structures", reference_dir.c_str());

    /* Load reference image -- we assume this is successful */
    Rt_study::Pointer ref_rtds = Rt_study::New();
    ref_rtds->load_image (reference_convert_img_fn);
    ref_rtds->load_prefix (reference_convert_structures_dir.c_str());

    /* Resample and save reference image and structures */
    std::string reference_prealign_img_fn = string_format (
        "%s/%s/img.nrrd", d_ptr->prealign_dir.c_str(), reference_id.c_str());
    std::string reference_prealign_structures_dir = string_format (
        "%s/%s/structures", d_ptr->prealign_dir.c_str(), reference_id.c_str());
    if (d_ptr->prealign_resample) {
        ref_rtds->resample (d_ptr->prealign_spacing);
    }
    Plm_image::Pointer reference_image = ref_rtds->get_image ();
    reference_image->save_image (reference_prealign_img_fn);
    ref_rtds->save_prefix (reference_prealign_structures_dir, "nrrd");

    /* Do it. */
    d_ptr->ref_rtds = ref_rtds;
    d_ptr->ref_id = reference_id;
    d_ptr->atlas_list = d_ptr->process_dir_list;
    d_ptr->output_dir = d_ptr->prealign_dir;
    run_registration_loop ();

    /* Choose best registration parameter settings based on statistics */
    std::string best_registration_name = d_ptr->stats.choose_best ();

    /* Copy results of best pre-alignment method into prealign base */
    std::list<std::string>::iterator atl_it;
    for (atl_it = d_ptr->atlas_list.begin();
         atl_it != d_ptr->atlas_list.end(); atl_it++)
    {
        std::string path = *atl_it;
        std::string atlas_id = basename (path);

        /* Don't copy over results for reference image */
        if (atlas_id == d_ptr->ref_id) {
            continue;
        }

        std::string src_directory = string_format ("%s/%s/%s", 
            d_ptr->prealign_dir.c_str(), atlas_id.c_str(), 
            best_registration_name.c_str());
        std::string dst_directory = string_format ("%s/%s", 
            d_ptr->prealign_dir.c_str(), atlas_id.c_str());
        std::string src_img = string_format ("%s/%s",
            src_directory.c_str(), "img.nrrd");
        std::string dst_img = string_format ("%s/%s",
            dst_directory.c_str(), "img.nrrd");

        /* Copy image */
        printf ("copying %s <- %s\n", dst_img.c_str(), src_img.c_str());
        copy_file (dst_img, src_img);

        /* Copy structures */
        std::string src_structures_dir = string_format ("%s/%s",
            src_directory.c_str(), "structures");
        std::string dst_structures_dir = string_format ("%s/%s",
            dst_directory.c_str(), "structures");
        make_directory (dst_structures_dir.c_str());
        Dir_list d (src_structures_dir);
        for (int i = 0; i < d.num_entries; i++)
        {
            /* Skip "." and ".." */
            if (!strcmp (d.entries[i], ".") || !strcmp (d.entries[i], "..")) {
                continue;
            }

            std::string src_structure = compose_filename (src_structures_dir, 
                d.entries[i]);
            std::string dst_structure = compose_filename (dst_structures_dir, 
                d.entries[i]);
            printf ("copying %s <- %s\n", 
                dst_structure.c_str(), src_structure.c_str());
            copy_file (dst_structure, src_structure);
        }
    }

    lprintf ("MABS pre-align complete\n");

    logfile_close ();
}

void
Mabs::parse_registration_dir (const std::string& registration_config)
{
    /* Figure out whether we need to do a single registration 
       or multiple registrations (for atlas tuning) */

    if (is_directory (registration_config)) {
        Dir_list dir (registration_config);
        for (int i = 0; i < dir.num_entries; i++) {
            std::string full_path = string_format (
                "%s/%s", registration_config.c_str(), 
                dir.entries[i]);
            /* Skip backup files */
            if (extension_is (dir.entries[i], "~")) {
                continue;
            }
            /* Skip directories */
            if (is_directory (full_path)) {
                continue;
            }
            d_ptr->registration_list.push_back (full_path);
        }
    }
    else {
        d_ptr->registration_list.push_back (registration_config);
    }
}

FloatImageType::Pointer
Mabs::compute_dmap (
    UCharImageType::Pointer& structure_image,
    const std::string& curr_output_dir,
    const std::string& mapped_name)
{
    Plm_timer timer;
    Distance_map dmap;

    /* Compute the dmap */
    timer.start ();
    dmap.set_input_image (structure_image);
    dmap.set_inside_is_positive (false);
    dmap.set_use_squared_distance (false);
    dmap.run ();
    FloatImageType::Pointer dmap_image = dmap.get_output_image ();

    /* Truncate the dmap.  This is to save disk space. 
       Maybe we won't need this if we can crop. */
    Adjustment_list al;
    al.push_back (std::make_pair (
            -std::numeric_limits<float>::max(), 0));
    al.push_back (std::make_pair (-400, -400));
    al.push_back (std::make_pair (400, 400));
    al.push_back (std::make_pair (
            std::numeric_limits<float>::max(), 0));
    itk_adjust (dmap_image, al);
    d_ptr->time_dmap += timer.report();

    if (d_ptr->write_distance_map_files) {
        timer.start();
        std::string fn = string_format ("%s/dmap_%s.nrrd", 
            curr_output_dir.c_str(), mapped_name.c_str());
        itk_image_save (dmap_image, fn.c_str());
        d_ptr->time_io += timer.report();
    }

    return dmap_image;
}

void
Mabs::gaussian_segmentation_vote (const std::string& atlas_id)
{
    Plm_timer timer;
    
    /* Set up files & directories for this job */
    std::string atlas_input_path;
    atlas_input_path = string_format ("%s/%s",
        d_ptr->prealign_dir.c_str(), atlas_id.c_str());
    lprintf ("atlas_input_path: %s\n",
        atlas_input_path.c_str());
    std::string atlas_output_path;
    atlas_output_path = string_format ("%s/%s",
        d_ptr->output_dir.c_str(), atlas_id.c_str());
    lprintf ("atlas_output_path: %s\n",
        atlas_output_path.c_str());
    std::string curr_output_dir;
    curr_output_dir = string_format ("%s/%s",
        atlas_output_path.c_str(),
        d_ptr->registration_id.c_str());
    lprintf ("curr_output_dir: %s\n", curr_output_dir.c_str());

    /* Load xform */
    timer.start();
    std::string xf_fn = string_format ("%s/%s",
        curr_output_dir.c_str(),
        "xf.txt");
    lprintf ("Loading xform: %s\n", xf_fn.c_str());
    Xform::Pointer xf = xform_load (xf_fn);
    d_ptr->time_io += timer.report();

    /* Load warped image */
    timer.start();
    std::string warped_image_fn;
    warped_image_fn = string_format (
        "%s/img.nrrd", curr_output_dir.c_str());
    Plm_image::Pointer warped_image = plm_image_load_native (warped_image_fn);
    d_ptr->time_io += timer.report();
    if (!warped_image) {
        /* Load atlas image */
        timer.start();
        std::string atlas_image_fn;
        atlas_image_fn = string_format ("%s/img.nrrd", 
            atlas_input_path.c_str());
        lprintf ("That's ok.  Loading atlas image instead: %s\n", 
            atlas_image_fn.c_str());
        Plm_image::Pointer atlas_image = 
            plm_image_load_native (atlas_image_fn);
        d_ptr->time_io += timer.report();
        /* Warp atlas image */
        lprintf ("Warping atlas image.\n");
        timer.start();
        warped_image = Plm_image::New();
        Plm_image_header fixed_pih (d_ptr->ref_rtds->get_image());
        plm_warp (warped_image, 0, xf, 
            &fixed_pih, 
            atlas_image, 
            0, 0, 1);
        d_ptr->time_warp_img += timer.report();
        /* Save warped image */
        if (d_ptr->write_warped_images) {
            timer.start();
            lprintf ("Saving warped atlas image: %s\n",
                warped_image_fn.c_str());
            warped_image->save_image (warped_image_fn.c_str());
            d_ptr->time_io += timer.report();
        }
    }

    /* Loop through structures for this atlas image */
    std::map<std::string, std::string>::const_iterator it;
    for (it = d_ptr->parms->structure_map.begin ();
         it != d_ptr->parms->structure_map.end (); it++)
    {
        std::string mapped_name = it->first;
        lprintf ("Segmenting structure: %s\n", mapped_name.c_str());

        /* Make a new voter if needed */
        Mabs_vote *vote;
        std::map<std::string, Mabs_vote*>::const_iterator vote_it 
            = d_ptr->vote_map.find (mapped_name);
        if (vote_it == d_ptr->vote_map.end()) {
            vote = new Mabs_vote;
            vote->set_rho (d_ptr->rho);
            vote->set_sigma (d_ptr->sigma);
            vote->set_minimum_similarity (d_ptr->minsim);
            d_ptr->vote_map[mapped_name] = vote;
            vote->set_fixed_image (
                d_ptr->ref_rtds->get_image()->itk_float());
        } else {
            vote = vote_it->second;
        }

        /* Load dmap */
        timer.start();
        lprintf ("Loading dmap\n");
        std::string dmap_fn = string_format ("%s/dmap_%s.nrrd", 
            curr_output_dir.c_str(), mapped_name.c_str());
        Plm_image::Pointer dmap_image = plm_image_load_native (
            dmap_fn.c_str());
        d_ptr->time_io += timer.report();
        if (!dmap_image) {
            /* Load warped structure */
            timer.start();
            std::string warped_structure_fn = string_format (
                "%s/structures/%s.nrrd", curr_output_dir.c_str(),
                mapped_name.c_str());
            lprintf ("That's ok, loading warped structure instead: %s\n",
                warped_structure_fn.c_str());
            Plm_image::Pointer warped_structure = plm_image_load_native (
                warped_structure_fn);
            d_ptr->time_io += timer.report();
            if (!warped_structure) {
                /* Load original structure */
                timer.start();
                std::string atlas_struct_fn;
                atlas_struct_fn = string_format ("%s/structures/%s.nrrd", 
                    atlas_input_path.c_str(), mapped_name.c_str());
                lprintf ("That's ok, loading atlas structure instead: %s\n", 
                    atlas_struct_fn.c_str());
                Plm_image::Pointer atlas_struct = 
                    plm_image_load_native (atlas_struct_fn);
                d_ptr->time_io += timer.report();
                if (!atlas_struct) {
                    lprintf ("Atlas %s doesn't have structure %s\n",
                        atlas_id.c_str(), mapped_name.c_str());
                    continue;
                }
                /* Warp structure */
                timer.start();
                warped_structure = Plm_image::New();
                Plm_image_header fixed_pih (d_ptr->ref_rtds->get_image());
                lprintf ("Warping atlas structure.\n");
                plm_warp (warped_structure, 0, xf, 
                    &fixed_pih, 
                    atlas_struct,
                    0, 0, 1);
                d_ptr->time_warp_str += timer.report();
            }
            if (!warped_structure) continue;
            /* Recompute distance map */
            timer.start();
            FloatImageType::Pointer dmap_image_itk = this->compute_dmap (
                warped_structure->itk_uchar(),
                curr_output_dir, mapped_name);
            dmap_image = Plm_image::New (dmap_image_itk);
            d_ptr->time_dmap += timer.report();
        }

        /* Vote */
        timer.start();
        lprintf ("Voting\n");
        vote->vote (warped_image->itk_float(), 
            dmap_image->itk_float());
        d_ptr->time_vote += timer.report();
    }
}

void
Mabs::prepare_staple_segmentation (const std::string& atlas_id)
{
    Plm_timer timer;
    timer.start();
    
    std::string current_dir;
    current_dir = string_format ("%s/%s/%s",
        d_ptr->output_dir.c_str(), atlas_id.c_str(), d_ptr->registration_id.c_str());

    /* Loop through structures for this atlas image */
    std::map<std::string, std::string>::const_iterator it;
    for (it = d_ptr->parms->structure_map.begin ();
         it != d_ptr->parms->structure_map.end (); it++)
    {
        std::string mapped_name = it->first;
        lprintf ("Preparing structure: %s (atl %s)\n", mapped_name.c_str(), atlas_id.c_str());

        std::string warped_structure_fn = string_format (
            "%s/structures/%s.nrrd", current_dir.c_str(),
            mapped_name.c_str());
        Plm_image::Pointer warped_structures = 
            plm_image_load_native (warped_structure_fn);

        /* Make a new staple object if needed */
        Mabs_staple *staple;
        std::map<std::string, Mabs_staple*>::const_iterator staple_it 
            = d_ptr->staple_map.find (mapped_name);
        if (staple_it == d_ptr->staple_map.end()) {
            staple = new Mabs_staple;
            staple->add_input_structure (warped_structures);
            d_ptr->staple_map[mapped_name] = staple;
        } else {
            d_ptr->staple_map[mapped_name]->add_input_structure (warped_structures);
        }

    }

    d_ptr->time_staple += timer.report();
}

void
Mabs::staple_segmentation_label ()
{
    Plm_timer timer;
    timer.start();

    /* Set up files & directories for this job */
    d_ptr->segmentation_training_dir
        = string_format ("%s/segmentations/%s/staple",
            d_ptr->output_dir.c_str(), d_ptr->registration_id.c_str());
    lprintf ("segmentation_training_dir: %s\n", 
        d_ptr->segmentation_training_dir.c_str());

    /* Get output image for each label */
    lprintf ("Extracting and saving final contour\n");
    for (std::map<std::string, Mabs_staple*>::const_iterator staple_it 
             = d_ptr->staple_map.begin(); 
         staple_it != d_ptr->staple_map.end(); staple_it++)
    {
        const std::string& mapped_name = staple_it->first;
        staple_it->second->run();

        std::string final_segmentation_img_fn = string_format (
            "%s/%s_staple.nrrd", 
            d_ptr->segmentation_training_dir.c_str(), 
            mapped_name.c_str()); 
        itk_image_save (staple_it->second->output_img->itk_uchar(), final_segmentation_img_fn.c_str());
       
       std::string atl_name = basename (d_ptr->output_dir);
        
        std::string ref_stru_fn;
        ref_stru_fn = string_format ("%s/%s/structures/%s.nrrd",
            d_ptr->prealign_dir.c_str(), atl_name.c_str(), mapped_name.c_str());
        Plm_image::Pointer ref_stru = 
            plm_image_load_native (ref_stru_fn);

        /* Compute Dice, etc. */
        std::string stats_string = d_ptr->stats.compute_statistics (
            "segmentation", /* Not used yet */
            ref_stru->itk_uchar(),
            staple_it->second->output_img->itk_uchar());
        std::string seg_log_string = string_format (
            "%s,reg=%s,struct=%s,"
            "rho=0,sigma=0,minsim=0,thresh=0,"
            "%s\n",
            d_ptr->ref_id.c_str(),
            d_ptr->registration_id.c_str(),
            mapped_name.c_str(),
            stats_string.c_str());
        lprintf ("%s", seg_log_string.c_str());

        /* Update seg_dice file */
        std::string seg_dice_log_fn = string_format (
            "%s/seg_dice.csv",
            d_ptr->mabs_train_dir.c_str());
        FILE *fp = fopen (seg_dice_log_fn.c_str(), "a");
        fprintf (fp, "%s", seg_log_string.c_str());
        fclose (fp);
    }

    d_ptr->time_staple += timer.report();
}

void
Mabs::gaussian_segmentation_label ()
{
    Plm_timer timer;

    /* Set up files & directories for this job */
    d_ptr->segmentation_training_dir
        = string_format ("%s/segmentations/%s/rho_%f_sig_%f_ms_%f",
            d_ptr->output_dir.c_str(), d_ptr->registration_id.c_str(),
            d_ptr->rho, d_ptr->sigma, d_ptr->minsim);
    lprintf ("segmentation_training_dir: %s\n", 
        d_ptr->segmentation_training_dir.c_str());

    /* Get output image for each label */
    lprintf ("Normalizing and saving weights\n");
    for (std::map<std::string, Mabs_vote*>::const_iterator vote_it 
             = d_ptr->vote_map.begin(); 
         vote_it != d_ptr->vote_map.end(); vote_it++)
    {
        const std::string& mapped_name = vote_it->first;
        Mabs_vote *vote = vote_it->second;
        lprintf ("Normalizing votes\n");
        timer.start();
        vote->normalize_votes();
        d_ptr->time_vote += timer.report();

        /* Get the weight image */
        FloatImageType::Pointer weight_image;
        weight_image = vote->get_weight_image ();

        /* Optionally, save the weight files */
        if (d_ptr->write_weight_files) {
            lprintf ("Saving weights\n");
            std::string fn = string_format ("%s/weight_%s.nrrd", 
                d_ptr->segmentation_training_dir.c_str(),
                vote_it->first.c_str());
            timer.start();
            itk_image_save (weight_image, fn.c_str());
            d_ptr->time_io += timer.report();
        }

        Option_range thresh_range;
        thresh_range.set_range (d_ptr->threshold_values);

        /* Loop through each threshold value, do thresholding,
           and then record score */

        const std::list<float>& thresh_list = thresh_range.get_range();
        std::list<float>::const_iterator thresh_it;
        for (thresh_it = thresh_list.begin(); 
             thresh_it != thresh_list.end(); thresh_it++) 
        {
            d_ptr->segmentation_threshold_weight (
                weight_image, mapped_name, 
                vote_it->first.c_str(), *thresh_it);
        }
    }
}

void
Mabs::run_segmentation ()
{
    /* Clear out internal structure */
    d_ptr->clear_vote_map ();

    /* Check if this segmentation is already complete.
       We might be able to skip it. */
    std::string curr_output_dir;
    if (d_ptr->parms->fusion_criteria == "gaussian") {
        curr_output_dir = string_format (
            "%s/segmentations/%s/rho_%f_sig_%f_ms_%f",
            d_ptr->output_dir.c_str(), d_ptr->registration_id.c_str(),
            d_ptr->rho, d_ptr->sigma, d_ptr->minsim);
    }

    else if (d_ptr->parms->fusion_criteria == "staple"){
        curr_output_dir = string_format (
            "%s/segmentations/%s/staple",
            d_ptr->output_dir.c_str(), d_ptr->registration_id.c_str());
    }

    std::string seg_checkpoint_fn = string_format (
        "%s/checkpoint.txt", curr_output_dir.c_str());
    if (file_exists (seg_checkpoint_fn)) {
        lprintf ("Segmentation complete for %s\n",
            curr_output_dir.c_str());
        return;
    }

    /* Loop through images in the atlas */
    std::list<std::string>::iterator atl_it;
    for (atl_it = d_ptr->atlas_list.begin();
         atl_it != d_ptr->atlas_list.end(); atl_it++)
    {
        std::string atlas_id = basename (*atl_it);
        if (d_ptr->parms->fusion_criteria == "gaussian") {
            gaussian_segmentation_vote (atlas_id);
        }
        else if (d_ptr->parms->fusion_criteria == "staple") {
            prepare_staple_segmentation (atlas_id);
        }
    }
    
    if (d_ptr->parms->fusion_criteria == "gaussian") {
        /* Threshold images based on weight */
        gaussian_segmentation_label ();

        /* Clear out internal structure */
        d_ptr->clear_vote_map ();
    }
    else if (d_ptr->parms->fusion_criteria == "staple") {
        staple_segmentation_label ();
    }

    /* Create checkpoint file which means that this segmentation
       is complete */
    touch_file (seg_checkpoint_fn);

}


void
Mabs::run_segmentation_loop ()
{

    Option_range minsim_range, rho_range, sigma_range;
    minsim_range.set_range (d_ptr->parms->minsim_values);
    rho_range.set_range (d_ptr->parms->rho_values);
    sigma_range.set_range (d_ptr->parms->sigma_values);

    d_ptr->threshold_values = d_ptr->parms->threshold_values;

    /* Loop through each registration parameter set */
    std::list<std::string>::iterator reg_it;
    for (reg_it = d_ptr->registration_list.begin(); 
         reg_it != d_ptr->registration_list.end(); reg_it++) 
    {
        d_ptr->registration_id = basename (*reg_it);

        if (d_ptr->parms->fusion_criteria == "gaussian") {

            /* Loop through each training parameter: rho */
            const std::list<float>& rho_list = rho_range.get_range();
            std::list<float>::const_iterator rho_it;
            for (rho_it = rho_list.begin(); rho_it != rho_list.end(); rho_it++) 
            {
                d_ptr->rho = *rho_it;

                /* Loop through each training parameter: sigma */
                const std::list<float>& sigma_list = sigma_range.get_range();
                std::list<float>::const_iterator sigma_it;
                for (sigma_it = sigma_list.begin(); 
                     sigma_it != sigma_list.end(); sigma_it++) 
                {
                    d_ptr->sigma = *sigma_it;

                    /* Loop through each training parameter: minimum similarity */
                    const std::list<float>& minsim_list = minsim_range.get_range();
                    std::list<float>::const_iterator minsim_it;
                    for (minsim_it = minsim_list.begin(); 
                         minsim_it != minsim_list.end(); minsim_it++) 
                    {
                        d_ptr->minsim = *minsim_it;

                        run_segmentation ();
                    }
                }
            }
        }

        else if (d_ptr->parms->fusion_criteria == "staple") {

            run_segmentation ();

        }

    }
}

void 
Mabs::set_parms (const Mabs_parms *parms)
{
    int rc;

    d_ptr->parms = parms;

    /* Set up directory strings */
    d_ptr->segment_input_fn = d_ptr->parms->labeling_input_fn;
    d_ptr->segment_outdir_base = d_ptr->parms->labeling_output_fn;
    if (d_ptr->segment_outdir_base == "") {
        d_ptr->segment_outdir_base = "mabs";
    }
    d_ptr->traindir_base = d_ptr->parms->training_dir;
    if (d_ptr->traindir_base == "") {
        d_ptr->traindir_base = "training";
    }
    d_ptr->convert_dir = string_format (
        "%s/convert", d_ptr->traindir_base.c_str());
    d_ptr->atlas_train_dir = string_format (
        "%s/atlas-train", d_ptr->traindir_base.c_str());
    d_ptr->prealign_dir = string_format (
        "%s/prealign", d_ptr->traindir_base.c_str());
    d_ptr->mabs_train_dir = string_format (
        "%s/mabs-train", d_ptr->traindir_base.c_str());

    /* Training section */
    d_ptr->stats.set_distance_map_algorithm (parms->distance_map_algorithm);

    /* Prealgnment section */
    d_ptr->prealign_resample = false;
    rc = sscanf (parms->prealign_spacing.c_str(), "%f %f %f", 
        &d_ptr->prealign_spacing[0], 
        &d_ptr->prealign_spacing[1], 
        &d_ptr->prealign_spacing[2]);
    if (rc == 3) {
        d_ptr->prealign_resample = true;
    }

    /* Segmentation training */
    d_ptr->write_distance_map_files = parms->write_distance_map_files;
    d_ptr->write_thresholded_files = parms->write_thresholded_files;
    d_ptr->write_weight_files = parms->write_weight_files;
    d_ptr->write_warped_images = parms->write_warped_images;
}

void
Mabs::set_segment_input (const std::string& input_fn)
{
    d_ptr->segment_input_fn = input_fn;
}

void 
Mabs::set_segment_output (const std::string& output_dir)
{
    d_ptr->segment_outdir_base = output_dir;
}

void 
Mabs::set_segment_output_dicom (const std::string& output_dicom_dir)
{
    /* Not yet implemented */
}

void
Mabs::train_internal ()
{
    Plm_timer timer;
    Plm_timer timer_total;
    timer_total.start();

    /* Do a few sanity checks */
    this->sanity_checks ();

    /* Open logfile */
    std::string logfile_path = string_format (
        "%s/%s", d_ptr->mabs_train_dir.c_str(), "logfile.txt");
    logfile_open (logfile_path.c_str(), "a");

    /* Prepare registration parameters */
    if (d_ptr->train_segmentation && 
        d_ptr->parms->optimization_result_reg != "")
    {
        /* We get the best registration result from an optimization file */
        std::string registration_fn = string_format ("%s/%s",
            d_ptr->parms->registration_config.c_str(),
            d_ptr->parms->optimization_result_reg.c_str());
        this->parse_registration_dir (registration_fn);
    } else {
        /* Else, parse directory with registration files */
        this->parse_registration_dir (d_ptr->parms->registration_config);
    }

    /* Parse atlas directory */
    this->load_process_dir_list (d_ptr->prealign_dir);
 
    /* If setted, run atlas selection */
    if (d_ptr->parms->enable_atlas_selection)
    {
        this->atlas_selection();
    }
   
    /* Loop through atlas_dir, choosing reference images to segment */
    for (std::list<std::string>::iterator it = d_ptr->process_dir_list.begin();
         it != d_ptr->process_dir_list.end(); it++)
    {
        /* Create atlas list for this test case */
        std::string path = *it;
        d_ptr->atlas_list = d_ptr->process_dir_list;
        d_ptr->atlas_list.remove (path);

        /* Set output dir for this test case */
        std::string patient_id = basename (path);
        d_ptr->ref_id = patient_id;

        d_ptr->output_dir = string_format ("%s/%s",
            d_ptr->mabs_train_dir.c_str(), patient_id.c_str());
        lprintf ("outdir = %s\n", d_ptr->output_dir.c_str());

        /* Load image & structures from "prep" directory */
        timer.start();
        std::string fn = string_format ("%s/%s/img.nrrd", 
            d_ptr->prealign_dir.c_str(), patient_id.c_str());
        d_ptr->ref_rtds->load_image (fn.c_str());
        fn = string_format ("%s/%s/structures", 
            d_ptr->prealign_dir.c_str(), patient_id.c_str());
        d_ptr->ref_rtds->load_prefix (fn.c_str());
        d_ptr->time_io += timer.report();
        
        /* Use the atlases coming from the selection step */
        if (!d_ptr->selected_atlases.empty())
        {
            /* Extract from map structure only the atlases 
               choosen for the current patient*/
            std::list<std::string> atlases_for_subject;
            std::list<std::pair<std::string, double> >::iterator atl_it;
            for (atl_it = d_ptr->selected_atlases[patient_id].begin();
                atl_it != d_ptr->selected_atlases[patient_id].end(); atl_it++)
            {
                std::string complete_atlas_path = string_format("%s/%s",
                    d_ptr->prealign_dir.c_str(), atl_it->first.c_str());
                atlases_for_subject.push_back(complete_atlas_path);
            }
            
            /* Assign the selected atlases */
            d_ptr->atlas_list = atlases_for_subject;
        }

        /* Run the segmentation */
        this->run_registration_loop ();
        if (d_ptr->train_segmentation == true) {
            this->run_segmentation_loop ();
        }
    }

    lprintf ("Atlas selection time: %10.1f seconds\n", 
        d_ptr->time_atlas_selection);
    lprintf ("Registration time:    %10.1f seconds\n", d_ptr->time_reg);
    lprintf ("Warping time (img):   %10.1f seconds\n", d_ptr->time_warp_img);
    lprintf ("Warping time (str):   %10.1f seconds\n", d_ptr->time_warp_str);
    lprintf ("Extraction time:      %10.1f seconds\n", d_ptr->time_extract);
    lprintf ("Dice time:            %10.1f seconds\n", 
        d_ptr->stats.get_time_dice());
    lprintf ("Hausdorff time:       %10.1f seconds\n", 
        d_ptr->stats.get_time_hausdorff());
    lprintf ("Distance map time:    %10.1f seconds\n", d_ptr->time_dmap);
    lprintf ("Voting time:          %10.1f seconds\n", d_ptr->time_vote);
    lprintf ("Staple time:          %10.1f seconds\n", d_ptr->time_staple);
    lprintf ("I/O time:             %10.1f seconds\n", d_ptr->time_io);
    lprintf ("Total time:           %10.1f seconds\n", timer_total.report());
    lprintf ("MABS training complete\n");

    logfile_close ();
}

void
Mabs::segment ()
{
    /* Do a few sanity checks */
    this->sanity_checks ();

    /* Prepare registration parameters */
    if (d_ptr->parms->optimization_result_reg != "") {
        /* We know the best registration result from an optimization file */
        std::string registration_fn = string_format ("%s/%s",
            d_ptr->parms->registration_config.c_str(),
            d_ptr->parms->optimization_result_reg.c_str());
        this->parse_registration_dir (registration_fn);
    } else {
        /* Else, parse directory with registration files */
        this->parse_registration_dir (d_ptr->parms->registration_config);
    }

    /* Load the image to be labeled.  For now, we'll assume this 
       is successful. */
    d_ptr->ref_rtds->load (d_ptr->segment_input_fn.c_str());

    /* Parse atlas directory */
    this->load_process_dir_list (d_ptr->prealign_dir);

    /* Set atlas_list */
    d_ptr->atlas_list = d_ptr->process_dir_list;

    /* Set output dir for this test case */
    d_ptr->output_dir = d_ptr->segment_outdir_base;

    /* Save it for debugging */
    std::string fn = string_format ("%s/%s", 
        d_ptr->segment_outdir_base.c_str(), 
        "img.nrrd");
    d_ptr->ref_rtds->get_image()->save_image (fn.c_str());

    /* Run the registrations */
    d_ptr->write_warped_images = true;
    this->run_registration_loop ();

    /* Run the segmentation */

    /* GCS FIX: 
       1) registration_id must be set to something sane when 
       optimization results are not available
       2) need better default values for rho, etc.
       3) need to read optimized values of rho, etc.
    */
    d_ptr->registration_id = d_ptr->parms->optimization_result_reg;
    d_ptr->rho = d_ptr->parms->optimization_result_seg_rho;
    d_ptr->sigma = d_ptr->parms->optimization_result_seg_sigma;
    d_ptr->minsim = d_ptr->parms->optimization_result_seg_minsim;
    d_ptr->threshold_values = d_ptr->parms->optimization_result_seg_thresh;
    
    run_segmentation ();
}

void
Mabs::train ()
{
    d_ptr->train_segmentation = true;
    d_ptr->compute_distance_map = true;
    d_ptr->write_warped_images = true;    /* Should be configurable */
    this->train_internal ();
}

void
Mabs::train_registration ()
{
    d_ptr->train_segmentation = false;
    d_ptr->compute_distance_map = false;
    this->train_internal ();
}

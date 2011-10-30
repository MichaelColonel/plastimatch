/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"

#include <stdio.h>
#include <itksys/SystemTools.hxx>
#include <itksys/Directory.hxx>
#include <itksys/RegularExpression.hxx>
#include "itkDirectory.h"
#include "itkImageRegionIterator.h"
#include "itkRegularExpressionSeriesFileNames.h"

#include "autolabel_trainer.h"
#include "bstrlib.h"
#include "file_util.h"
#include "itk_image.h"
#include "plm_image.h"
#include "pointset.h"
#include "print_and_exit.h"
#include "thumbnail.h"

void
Autolabel_trainer::load_input_dir (const char* input_dir)
{
    if (!itksys::SystemTools::FileIsDirectory (input_dir)) {
	print_and_exit ("Error: \'%s\' is not a directory\n", input_dir);
    }

    /* We can't load the data yet, since we don't know the task */
    m_input_dir = input_dir;
}

void
Autolabel_trainer::load_input_dir_recursive (std::string input_dir)
{
    itksys::Directory itk_dir;

    if (!itk_dir.Load (input_dir.c_str())) {
	print_and_exit ("Error loading directory (%s)\n", input_dir.c_str());
    }

    for (unsigned long i = 0; i < itk_dir.GetNumberOfFiles(); i++)
    {
	/* Skip "." and ".." */
	if (!strcmp (itk_dir.GetFile(i), ".") 
	    || !strcmp (itk_dir.GetFile(i), ".."))
	{
	    continue;
	}

	/* Make fully specified filename */
	std::string fn = input_dir + "/" + itk_dir.GetFile(i);

	/* Process directories recursively */
	if (itksys::SystemTools::FileIsDirectory (fn.c_str())) {
	    load_input_dir_recursive (fn);
	}

	/* Check for .nrrd files */
	if (extension_is (fn.c_str(), "nrrd")) {
	    /* Does .nrrd file have a corresponding .fcsv file? */
	    std::string fcsv_fn = fn;
	    fcsv_fn.replace (fn.length()-4, 4, "fcsv");

	    if (file_exists (fcsv_fn.c_str())) {
		load_input_file (fn.c_str(), fcsv_fn.c_str());
	    }
	}
    }

}

void
Autolabel_trainer::load_input_file_la (
    const char* nrrd_fn,
    const char* fcsv_fn)
{
    Plm_image pi;
    Labeled_pointset ps;

    ps.load_fcsv (fcsv_fn);

    bool found = false;
    for (unsigned int i = 0; i < ps.point_list.size(); i++) {
	if (ps.point_list[i].label == "LLA") {
	    printf ("%s, found label LLA\n", fcsv_fn);
	    found = true;
	}
    }

    (void) found;  /* Suppress compiler warning */

    /* Still need to load file & generate learning vectors here */
}

static std::map<float, Point> 
load_tspine_map (const char* fcsv_fn)
{
    Labeled_pointset ps;
    ps.load_fcsv (fcsv_fn);

    /* Generate map from t-spine # to (x,y,z) point */
    std::map<float, Point> t_map;
    for (unsigned int i = 0; i < ps.point_list.size(); i++) {
	if (ps.point_list[i].label == "C7") {
	    t_map.insert (std::pair<float,Point> (0, 
		    Point (ps.point_list[i].p[0],
			ps.point_list[i].p[1],
			ps.point_list[i].p[2])));
	}
	else if (ps.point_list[i].label == "L1") {
	    t_map.insert (std::pair<float,Point> (13, 
		    Point (ps.point_list[i].p[0],
			ps.point_list[i].p[1],
			ps.point_list[i].p[2])));
	}
	else {
	    float t;
	    int rc = sscanf (ps.point_list[i].label.c_str(), "T%f", &t);
	    if (rc != 1) {
		/* Not a vertebra point */
		continue;
	    }
	    if (t > 0.25 && t < 12.75) {
		t_map.insert (std::pair<float,Point> (t, 
			Point (ps.point_list[i].p[0],
			    ps.point_list[i].p[1],
			    ps.point_list[i].p[2])));
	    }
	}
    }
    return t_map;
}

void
Autolabel_trainer::load_input_file_tsv1 (
    const char* nrrd_fn,
    const char* fcsv_fn)
{
    Labeled_pointset ps;
    ps.load_fcsv (fcsv_fn);

    /* Generate map from t-spine # to z pos */
    std::map<float, float> t_map;
    for (unsigned int i = 0; i < ps.point_list.size(); i++) {
	if (ps.point_list[i].label == "C7") {
	    t_map.insert (std::pair<float,float> (0, ps.point_list[i].p[2]));
	}
	else if (ps.point_list[i].label == "L1") {
	    t_map.insert (std::pair<float,float> (13, ps.point_list[i].p[2]));
	}
	else {
	    float t;
	    int rc = sscanf (ps.point_list[i].label.c_str(), "T%f", &t);
	    if (rc != 1) {
		/* Not a vertebra point */
		continue;
	    }
	    if (t > 0.25 && t < 12.75) {
		t_map.insert (std::pair<float,float> (
			t, ps.point_list[i].p[2]));
	    }
	}
    }

#if defined (commentout)
    /* Print out map */
    std::map<float, float>::iterator it;
    for (it = t_map.begin(); it != t_map.end(); it++) {
	printf ("Map: %f %f\n", it->first, it->second);
    }
#endif

    /* If we want to use interpolation, we need to sort, and make 
      a "back-map" from z-pos to t-spine */

    /* Otherwise, for the simple case, we're good to go. */
    Plm_image *pli;
    pli = plm_image_load (nrrd_fn, PLM_IMG_TYPE_ITK_FLOAT);
    std::map<float, float>::iterator it;
    for (it = t_map.begin(); it != t_map.end(); ++it) {
	Thumbnail thumb;
	thumb.set_input_image (pli);
	thumb.set_thumbnail_dim (16);
	thumb.set_thumbnail_spacing (25.0f);
	thumb.set_slice_loc (it->second);
	FloatImageType::Pointer thumb_img = thumb.make_thumbnail ();
	itk::ImageRegionIterator< FloatImageType > thumb_it (
	    thumb_img, thumb_img->GetLargestPossibleRegion());
	fprintf (this->fp, "%f", it->first);
	int i;
	for (i = 0, thumb_it.GoToBegin(); !thumb_it.IsAtEnd(); ++i, ++thumb_it)
	{
	    fprintf (this->fp, " %d:%f", i, thumb_it.Get ());
	}
	fprintf (this->fp, "\n");
    }

    delete pli;
}

void
Autolabel_trainer::load_input_file_tsv2 (
    const char* nrrd_fn,
    const char* fcsv_fn)
{
    /* Create map from spine # to point from fcsv file */
    std::map<float,Point> t_map = load_tspine_map (fcsv_fn);

    /* If we want to use interpolation, we need to sort, and make 
      a "back-map" from z-pos to t-spine */

    /* Otherwise, for the simple case, we're good to go. 
       Here, for testing, we'll make a "y" map. */
    Plm_image *pli;
    pli = plm_image_load (nrrd_fn, PLM_IMG_TYPE_ITK_FLOAT);
    std::map<float, Point>::iterator it;
    for (it = t_map.begin(); it != t_map.end(); ++it) {
	Thumbnail thumb;
	thumb.set_input_image (pli);
	thumb.set_thumbnail_dim (16);
	thumb.set_thumbnail_spacing (25.0f);
	thumb.set_slice_loc (it->second.p[2]);
	FloatImageType::Pointer thumb_img = thumb.make_thumbnail ();
	itk::ImageRegionIterator< FloatImageType > thumb_it (
	    thumb_img, thumb_img->GetLargestPossibleRegion());
	//fprintf (this->fp, "%f", it->first);
	//fprintf (this->fp, "%f", it->second.p[0]);
	fprintf (this->fp, "%f", it->second.p[1]);
	int i;
	for (i = 0, thumb_it.GoToBegin(); !thumb_it.IsAtEnd(); ++i, ++thumb_it)
	{
	    fprintf (this->fp, " %d:%f", i, thumb_it.Get ());
	}
	fprintf (this->fp, "\n");
    }

    delete pli;
}

void
Autolabel_trainer::load_input_file (
    const char* nrrd_fn,
    const char* fcsv_fn)
{
    printf ("Loading\n  %s\n  %s\n", nrrd_fn, fcsv_fn);
    if (m_task == "la") {
	load_input_file_la (nrrd_fn, fcsv_fn);
    }
    else if (m_task == "tsv1") {
	load_input_file_tsv1 (nrrd_fn, fcsv_fn);
    }
    else if (m_task == "tsv2") {
	load_input_file_tsv2 (nrrd_fn, fcsv_fn);
    }
    else {
	print_and_exit ("Error: unsupported autolabel-train task (%s)\n",
	    m_task.c_str());
    }
}

void
Autolabel_trainer::set_task (const char* task)
{
    m_task = task;
}

void
Autolabel_trainer::save_libsvm (const char* output_libsvm_fn)
{
    this->fp = fopen (output_libsvm_fn, "w");

    if (m_task == "" || m_input_dir == "") {
	print_and_exit ("Error saving libsvm, inputs not fully specified.\n");
    }

    /* Load the data according to task specification */
    load_input_dir_recursive (m_input_dir);

    fclose (this->fp);
}

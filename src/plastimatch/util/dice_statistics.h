/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dice_statistics_h_
#define _dice_statistics_h_

#include "plmutil_config.h"
#include "itk_image_type.h"

class Plm_image;
class Dice_statistics_private;

/*! \brief 
 * The Dice_statistics class computes a Dice statistic for the 
 * overlap between two regions.  Dice is defined as 
 *
 * (I need to learn how to use doxygen math commands here)
 *
 * At this time, the images must be of the same size and resolution. 
 * The class will throw an exception if they are not.
 */
class PLMUTIL_API Dice_statistics {
public:
    Dice_statistics ();
    ~Dice_statistics ();
public:
    Dice_statistics_private *d_ptr;
public:

    /*! \name Inputs */
    ///@{
    /*! \brief Set the reference image.  The image will be loaded
      from the specified filename. */
    void set_reference_image (const char* image_fn);
    /*! \brief Set the reference image as an ITK image. */
    void set_reference_image (const UCharImageType::Pointer image);
    /*! \brief Set the compare image.  The image will be loaded
      from the specified filename. */
    void set_compare_image (const char* image_fn);
    /*! \brief Set the compare image as an ITK image. */
    void set_compare_image (const UCharImageType::Pointer image);
    ///@}

    /*! \name Execution */
    ///@{
    /*! \brief Compute dice statistics */
    void run ();
    ///@}

    /*! \name Outputs */
    ///@{
    /*! \brief Return the Dice coefficient value */
    float get_dice ();
    /*! \brief Return the number of true positive voxels, i.e. positive 
      reference voxels that are positive in compare image */
    size_t get_true_positives ();
    /*! \brief Return the number of true negative voxels, i.e. negative 
      reference voxels that are negative in compare image */
    size_t get_true_negatives ();
    /*! \brief Return the number of false positive voxels, i.e. negative 
      reference voxels that are positive in compare image */
    size_t get_false_positives ();
    /*! \brief Return the number of false negative voxels, i.e. positive 
      reference voxels that are negative in compare image */
    size_t get_false_negatives ();
    /*! \brief Return the location of the center of mass 
      of the reference image structure */
    DoubleVector3DType get_reference_center ();
    /*! \brief Return the location of the center of mass 
      of the compare image structure */
    DoubleVector3DType get_compare_center ();
    /*! \brief Return the volume of the reference image structure */
    double get_reference_volume ();
    /*! \brief Return the volume of the compare image structure */
    double get_compare_volume ();
    ///@}
};

template<class T> float 
PLMUTIL_API
do_dice (typename itk::Image<T,3>::Pointer reference, 
    typename itk::Image<T,3>::Pointer warped, FILE* output);

#endif
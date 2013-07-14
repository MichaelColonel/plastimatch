/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _ion_beam_h_
#define _ion_beam_h_

#include "plmdose_config.h"

class Ion_beam_private;
class Ion_sobp;

/*! \brief 
 * The Ion_beam class encapsulates a single SOBP ion beam, including 
 * its associated aperture and range compensator.
 */
class PLMDOSE_API Ion_beam {
public:
    Ion_beam ();
    ~Ion_beam ();
public:
    Ion_beam_private *d_ptr;

public:
    /*! \name Inputs */
    ///@{
    /*! \brief Load PDD from XiO or txt file */
    bool load (const char* fn);

    /*! \brief Get the position of the beam source in world coordinates. */
    const double* get_source_position ();
    /*! \brief Get the x, y, or z coordinate of the beam source 
      in world coordinates. */
    double get_source_position (int dim);
    /*! \brief Set the position of the beam source in world coordinates. */
    void set_source_position (const float position[3]);
    /*! \brief Set the position of the beam source in world coordinates. */
    void set_source_position (const double position[3]);

    /*! \brief Get the position of the beam isocenter in world coordinates. */
    const double* get_isocenter_position ();
    /*! \brief Get the x, y, or z coordinate of the beam source 
      in world coordinates. */
    double get_isocenter_position (int dim);
    /*! \brief Set the position of the beam isocenter in world coordinates. */
    void set_isocenter_position (const float position[3]);
    /*! \brief Set the position of the beam isocenter in world coordinates. */
    void set_isocenter_position (const double position[3]);

    /*! \brief Add an SOBP pristine peak to this beam */
    void add_peak (
        double E0,                 /* initial ion energy (MeV) */
        double spread,             /* beam energy sigma (MeV) */
        double dres,               /* spatial resolution of bragg curve (mm)*/
        double dmax,               /* maximum w.e.d. (mm) */
        double weight);

    /*! \brief Get "detail" parameter of dose calculation algorithm */
    int get_detail () const;
    /*! \brief Set "detail" parameter of dose calculation algorithm */
    void set_detail (int detail);
    /*! \brief Get "flavor" parameter of dose calculation algorithm */
    char get_flavor () const;
    /*! \brief Set "flavor" parameter of dose calculation algorithm */
    void set_flavor (char flavor);

    /*! \brief Get maximum depth (in mm) in SOBP curve */
    double get_sobp_maximum_depth ();
    /*! \brief Set SOBP range and modulation for prescription 
      as minimum and maximum depth (in mm) */
    void set_sobp_prescription_min_max (float d_min, float d_max);
    ///@}

    /*! \name Execution */
    ///@{
    bool generate ();               /* generate analytically */
    ///@}

    /*! \name Outputs */
    ///@{
    void dump (const char* fn);     /* debug: print bragg curve to file */
    float lookup_sobp_dose (float depth);
    ///@}

private:
    bool load_xio (const char* fn);
    bool load_txt (const char* fn);

};

#endif

/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _sigma_spread_h_
#define _sigma_spread_h_

#include "plmdose_config.h"
#include <math.h>
#include <vector>
#include "volume.h"
#include "rpl_volume.h"
#include "proj_volume.h"

extern const double lookup_range_water[][2];
extern const double lookup_stop_water[][2];
extern const double lookup_r2_over_sigma2[][2];

class Ion_beam;

void convert_radiologic_length_to_sigma(Rpl_volume* sigma_vol, Rpl_volume* ct_vol, float energy, float spacing_z, float* sigma_max); // compute the sigma_vol and return sigma_max
void length_to_sigma(std::vector<float>* p_sigma, std::vector<float>* p_density, float spacing_z,float* sigma_max, float energy);

void compute_dose_ray_desplanques(Volume* dose_volume, Volume::Pointer ct_vol, Rpl_volume* rpl_vol, Rpl_volume* sigma_vol, Rpl_volume* ct_vol_density, Ion_beam* beam, Volume::Pointer final_dose_volume);
void compute_dose_ray_sharp(Volume::Pointer ct_vol, Rpl_volume* rpl_vol, Rpl_volume* sigma_vol, Rpl_volume* ct_vol_density, Ion_beam* beam, Volume::Pointer final_dose_volume, Rpl_volume* rpl_dose_volume, Aperture::Pointer ap);

void calculate_rpl_coordinates_xyz(std::vector<std::vector<double> >* xyz_coordinates_volume, Rpl_volume* rpl_volume);
void find_ijk_pixel(int* ijk_idx, double* xyz_ray_center, Volume* dose_volume);
void find_xyz_center_entrance(double* xyz_ray_center, double* ray, float z_axis_offset);
void find_xyz_center(double* xyz_ray_center, double* ray, float z_axis_offset, int k);
void find_xyz_from_ijk(double* xyz, Volume* volume, int* ijk);
double distance(std::vector< std::vector <double> >, int, int);

float LR_interpolation(float density);
float WER_interpolation(float density);

double getrange(double energy);
double getstop(double energy);

#endif

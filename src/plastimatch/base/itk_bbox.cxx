/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <float.h>
#include "itkContinuousIndex.h"
#include "itkImageRegionConstIteratorWithIndex.h"

#include "itk_bbox.h"
#include "itk_image.h"

void
itk_bbox (UCharImageType::Pointer img, float *bbox)
{
    for (int d = 0; d < 3; d++)
    {
        bbox[2*d+0] =  FLT_MAX;
        bbox[2*d+1] = -FLT_MAX;
    }
    
    UCharImageType::RegionType region = img->GetLargestPossibleRegion();
    itk::ImageRegionConstIteratorWithIndex< UCharImageType >
        it (img,  region);

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        unsigned char c = it.Get();
        if (!c) {
            continue;
        }
        /* If voxel is non-zero, loop through the eight corners of the 
           voxels, find their position, and set bounding box to contain */
        itk::ContinuousIndex<float,3> cidx = it.GetIndex();
        for (int i = 0; i < 8; i++) {
            itk::ContinuousIndex<float,3> cidx_corner = cidx;
            cidx_corner[0] += (i % 2) - 0.5;
            cidx_corner[1] += ((i / 2) % 2) - 0.5;
            cidx_corner[2] += (i / 4) - 0.5;
            FloatPoint3DType point;
            img->TransformContinuousIndexToPhysicalPoint (cidx_corner, point);
            for (int d = 0; d < 3; d++) {
                if (point[d] < bbox[2*d+0]) {
                    bbox[2*d+0] = point[d];
                }
                if (point[d] > bbox[2*d+1]) {
                   bbox[2*d+1] = point[d];
                }
            }
        }
    }
}
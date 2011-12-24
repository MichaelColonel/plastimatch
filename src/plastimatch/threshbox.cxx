/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "itkImageRegionIteratorWithIndex.h"

#include "itk_directions.h"
#include "itk_image.h"
#include "math_util.h"
#include "threshbox.h"
#include "plm_image_header.h"

void do_threshbox( Threshbox_parms *parms) {

    float spacing_in[3], origin_in[3];
    size_t dim_in[3];
    Plm_image_header pih;
    unsigned char label_uchar, label_box;

    FloatImageType::Pointer img_in = parms->img_in->itk_float();

    pih.set_from_itk_image (img_in);
    pih.get_dim (dim_in);
    pih.get_origin (origin_in );
    pih.get_spacing (spacing_in );
    // direction cosines??

    /* Create ITK image for labelmap */
    FloatImageType::SizeType sz;
    FloatImageType::IndexType st;
    FloatImageType::RegionType rg;
    FloatImageType::PointType og;
    FloatImageType::SpacingType sp;
    FloatImageType::DirectionType itk_dc;
    for (int d1 = 0; d1 < 3; d1++) {
	st[d1] = 0;
	sz[d1] = dim_in[d1];
	sp[d1] = spacing_in[d1];
	og[d1] = origin_in[d1];
    }
    rg.SetSize (sz);
    rg.SetIndex (st);
    itk_direction_from_dc (&itk_dc, parms->dc);

    // labelmap thresholded image
    UCharImageType::Pointer uchar_img = UCharImageType::New();
    uchar_img->SetRegions (rg);
    uchar_img->SetOrigin (og);
    uchar_img->SetSpacing (sp);
    uchar_img->Allocate();

    // box image
    UCharImageType::Pointer box_img = UCharImageType::New();
    box_img->SetRegions (rg);
    box_img->SetOrigin (og);
    box_img->SetSpacing (sp);
    box_img->Allocate();

    typedef itk::ImageRegionIteratorWithIndex< UCharImageType > UCharIteratorType;
    UCharIteratorType uchar_img_iterator;
    uchar_img_iterator = UCharIteratorType (uchar_img, uchar_img->GetLargestPossibleRegion());
    uchar_img_iterator.GoToBegin();

    UCharIteratorType box_img_iterator;
    box_img_iterator = UCharIteratorType (box_img, box_img->GetLargestPossibleRegion());
    box_img_iterator.GoToBegin();

    typedef itk::ImageRegionIteratorWithIndex< FloatImageType > FloatIteratorType;
    FloatIteratorType img_in_iterator (img_in, img_in->GetLargestPossibleRegion());

    FloatImageType::IndexType k;
    FloatPoint3DType phys;

    float level, maxlevel=-1e20;
    for (img_in_iterator.GoToBegin(); !img_in_iterator.IsAtEnd(); ++img_in_iterator) {
	    
	k=img_in_iterator.GetIndex();
//	    img_in->TransformIndexToPhysicalPoint( k, phys );

	level = img_in_iterator.Get();
	    
	if ( (parms->center[0]- parms->boxsize[0]/2 <= k[0] && k[0] < parms->center[0]+parms->boxsize[0]/2) &&
	    (parms->center[1]- parms->boxsize[1]/2 <= k[1] && k[1] < parms->center[1]+parms->boxsize[1]/2) &&
	    (parms->center[2]- parms->boxsize[2]/2 <= k[2] && k[2] < parms->center[2]+parms->boxsize[2]/2) )
	{
	    if (level> maxlevel) maxlevel = level;
	}
    }

    for (img_in_iterator.GoToBegin(); !img_in_iterator.IsAtEnd(); ++img_in_iterator) {
	    
	level = img_in_iterator.Get();
	k=img_in_iterator.GetIndex();
	//img_in->TransformIndexToPhysicalPoint( k, phys );

	label_uchar = 0;
	label_box = 0;
            
	if ( (parms->center[0]- parms->boxsize[0]/2 <= k[0] && k[0] < parms->center[0]+parms->boxsize[0]/2) &&
	    (parms->center[1]- parms->boxsize[1]/2 <= k[1] && k[1] < parms->center[1]+parms->boxsize[1]/2) &&
	    (parms->center[2]- parms->boxsize[2]/2 <= k[2] && k[2] < parms->center[2]+parms->boxsize[2]/2) ) 
	{ /* label_uchar = 2; */ label_box=1; } 
            
	if ( (parms->center[0] - parms->boxsize[0]/2 <= k[0] && k[0] < parms->center[0]+parms->boxsize[0]/2) &&
	    (parms->center[1] - parms->boxsize[1]/2 <= k[1] && k[1] < parms->center[1]+parms->boxsize[1]/2) &&
	    (parms->center[2] - parms->boxsize[2]/2 <= k[2] && k[2] < parms->center[2]+parms->boxsize[2]/2) )	
	    if	(level > parms->threshold/100.*maxlevel) label_uchar = 1;
	    
	uchar_img_iterator.Set ( label_uchar );
	box_img_iterator.Set ( label_box );
	++uchar_img_iterator;
	++box_img_iterator;
    }

    parms->img_out = new Plm_image;
    parms->img_out->set_itk( uchar_img);

    parms->img_box = new Plm_image;
    parms->img_box->set_itk( box_img);

}

void do_overlap_fraction( Threshbox_parms *parms) {

    double vol_img1=0, vol_img2=0, vol_min=0, vol_overlap=0;
    int level1, level2;

    UCharImageType::Pointer img1 = parms->overlap_labelmap1->itk_uchar();
    UCharImageType::Pointer img2 = parms->overlap_labelmap2->itk_uchar();

    typedef itk::ImageRegionIteratorWithIndex< UCharImageType > UCharIteratorType;

    UCharIteratorType img1_iterator;
    img1_iterator = UCharIteratorType (img1, img1->GetLargestPossibleRegion());
    img1_iterator.GoToBegin();
    
    UCharIteratorType img2_iterator;
    img2_iterator = UCharIteratorType (img2, img2->GetLargestPossibleRegion());
    img2_iterator.GoToBegin();

    for (img1_iterator.GoToBegin(); !img1_iterator.IsAtEnd(); ++img1_iterator) {
	    level1 = img1_iterator.Get();
	    if ( level1>0 ) vol_img1++;
    }

    for (img2_iterator.GoToBegin(); !img2_iterator.IsAtEnd(); ++img2_iterator) {
	    level2 = img2_iterator.Get();
	    if ( level2>0 ) vol_img2++;
    }

    FloatPoint3DType phys;
    bool in_image;
    UCharImageType::IndexType k1;
    UCharImageType::IndexType k2;
    
    img1_iterator = UCharIteratorType (img1, img1->GetLargestPossibleRegion());
    img1_iterator.GoToBegin();
    
    for (img1_iterator.GoToBegin(); !img1_iterator.IsAtEnd(); ++img1_iterator) {
        
	/*make sure we can process images with different offsets etc*/
	k1=img1_iterator.GetIndex();
	img1->TransformIndexToPhysicalPoint( k1, phys );
	in_image = img2->TransformPhysicalPointToIndex( phys, k2) ;

	if (in_image) {
		level1 = img1->GetPixel(k1);
		level2 = img2->GetPixel(k2);
		if (level1 >0 && level2 > 0) vol_overlap++;
		}

	}

    if (vol_img1<vol_img2) vol_min=vol_img1; else vol_min=vol_img2;

    FILE *fp = fopen( parms->overlap_fn, "w");
    if (fp) { 
	fprintf(fp, "Vol1 %.1f  Vol2 %.1f  Volmin %.1f  Vover %.1f  (voxels)\n", 
	   vol_img1,vol_img2, vol_min, vol_overlap);
	fprintf(fp, "Vover/Volmin = %.3f\n", vol_overlap/vol_min);
	fclose(fp);
	}
}

/*
void remove_face_connected_regions(img...)
http://www.itk.org/Doxygen314/html/classitk_1_1ConnectedThresholdImageFilter.html

iterate over src image
{
if (! on_threshbox_face) continue;
if GetPix(src)==0 continue;

seed on the threshbox face = current voxel

create itk::ConnectedThresholdImageFilter

virtual void itk::ImageToImageFilter< TInputImage, TOutputImage >::SetInput  
    ( const InputImageType *  image   )  [virtual] 

virtual void itk::ConnectedThresholdImageFilter< TInputImage, TOutputImage >::SetReplaceValue  
    ( OutputImagePixelType  _arg   )  [virtual] 
//Set/Get value to replace thresholded pixels. Pixels that lie * within Lower and Upper 
//(inclusive) will be replaced with this value. The default is 1. 

void itk::ConnectedThresholdImageFilter< TInputImage, TOutputImage >::SetSeed  
	    ( const IndexType &  seed   )  
virtual void itk::ConnectedThresholdImageFilter< TInputImage, TOutputImage >::SetUpper  
	    ( InputImagePixelType    )  [virtual] 
//also SetLower?  -> set both to 1.

someFilter->Update();
image = someFilter->GetOutput();

iterate over src image
 if ( GetPix( src)>0 && GetPix(filtered)==filterlabel ) Pix(src)=0

delete filter

} 

subtract 1 from nonzero pixels in src

*/
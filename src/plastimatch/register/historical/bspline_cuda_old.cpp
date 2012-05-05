////////////////////////////////////////////////////////////////////////////////
// FUNCTION: bspline_cuda_score_i_mse()
//
////////////////////////////////////////////////////////////////////////////////
void
bspline_cuda_score_i_mse (
    Bspline_parms* parms,
    Bspline_state *bst,
    Bspline_xform* bxf,
    Volume* fixed,
    Volume* moving,
    Volume* moving_grad,
    Dev_Pointers_Bspline* dev_ptrs)
{


    // --- DECLARE LOCAL VARIABLES ------------------------------
    BSPLINE_Score* ssd; // Holds the SSD "Score" information
    int num_vox;        // Holds # of voxels in the fixed volume
    float ssd_grad_norm;    // Holds the SSD Gradient's Norm
    float ssd_grad_mean;    // Holds the SSD Gradient's Mean
    Timer timer;            // The timer

    static int it=0;    // Holds Iteration Number
    char debug_fn[1024];    // Debug message buffer
    FILE* fp = NULL;        // File Pointer to Debug File
    // ----------------------------------------------------------


    // --- INITIALIZE LOCAL VARIABLES ---------------------------
    ssd = &bst->ssd;
    
    if (parms->debug) {
        sprintf (debug_fn, "dump_mse_%02d.txt", it++);
        fp = fopen (debug_fn, "w");
    }
    // ----------------------------------------------------------

    plm_timer_start (&timer);  // <=== START TIMING HERE
    
    // --- INITIALIZE GPU MEMORY --------------------------------
    bspline_cuda_h_push_coeff_lut(dev_ptrs, bxf);
    bspline_cuda_h_clear_score(dev_ptrs);
    bspline_cuda_h_clear_grad(dev_ptrs);
    // ----------------------------------------------------------


    // --- LAUNCH STUB FUNCTIONS --------------------------------

    // Populate the score, dc_dv, and gradient
    bspline_cuda_i_stage_1(
    fixed,
    moving,
    moving_grad,
    bxf,
    parms,
    dev_ptrs);


    // Calculate the score and gradient
    // via sum reduction
    bspline_cuda_j_stage_2(
    parms,
    bxf,
    fixed,
    bxf->vox_per_rgn,
    fixed->dim,
    &(ssd->score),
    bst->ssd.grad, //ssd->grad,
    &ssd_grad_mean,
    &ssd_grad_norm,
    dev_ptrs,
    &num_vox);

    if (parms->debug) {
        fclose (fp);
    }

    // --- USER FEEDBACK ----------------------------------------
    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// FUNCTION: bspline_cuda_score_h_mse()
//
////////////////////////////////////////////////////////////////////////////////
void bspline_cuda_score_h_mse(
    BSPLINE_Parms* parms,
    Bspline_state *bst,
    BSPLINE_Xform* bxf,
    Volume* fixed,
    Volume* moving,
    Volume* moving_grad,
    Dev_Pointers_Bspline* dev_ptrs)
{


    // --- DECLARE LOCAL VARIABLES ------------------------------
    BSPLINE_Score* ssd;	// Holds the SSD "Score" information
    int num_vox;		// Holds # of voxels in the fixed volume
    float ssd_grad_norm;	// Holds the SSD Gradient's Norm
    float ssd_grad_mean;	// Holds the SSD Gradient's Mean
    Timer timer;
    static int it=0;	// Holds Iteration Number
    char debug_fn[1024];	// Debug message buffer
    FILE* fp;		// File Pointer to Debug File
    // ----------------------------------------------------------


    // --- INITIALIZE LOCAL VARIABLES ---------------------------
    ssd = &bst->ssd;
    num_vox = fixed->npix;
	
    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }
    // ----------------------------------------------------------

    plm_timer_start (&timer);
	
    // --- INITIALIZE GPU MEMORY --------------------------------
    bspline_cuda_h_push_coeff_lut(dev_ptrs, bxf);
    bspline_cuda_h_clear_score(dev_ptrs);
    bspline_cuda_h_clear_grad(dev_ptrs);
    // ----------------------------------------------------------

	
    // --- LAUNCH STUB FUNCTIONS --------------------------------

    // Populate the score, dc_dv, and gradient
    bspline_cuda_h_stage_1(
	fixed,
	moving,
	moving_grad,
	bxf,
	parms,
	dev_ptrs);


    // Calculate the score and gradient
    // via sum reduction
    bspline_cuda_h_stage_2(
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	&(ssd->score),
	ssd->grad,
	&ssd_grad_mean,
	&ssd_grad_norm,
	dev_ptrs);

    if (parms->debug) {
	fclose (fp);
    }

    // --- USER FEEDBACK ----------------------------------------
    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}



/***********************************************************************
 * bspline_cuda_score_g_mse
 * 
 * This implementation is identical to version "F" with the exception
 * that the c_lut and q_lut values are computed on the fly in the
 * kernels.  This saves memory on the GPU as well as speeds up execution
 * by reducing the number of reads from global memory.
 *
 * The score and dc_dv values are calculated for the entire volume at 
 * once (as opposed to tile by tile or set by set), so the the memory 
 * requirements are slightly higher.  However, this does appear to
 * improve performance dramatically.
 *
 * This is currently the fastest CUDA implementation of the B-spline
 * algorithm.

 Updated by Naga Kandasamy
 Date: 07 July 2009
***********************************************************************/
void 
bspline_cuda_score_g_mse (
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad)
{
    BSPLINE_Score* ssd = &bst->ssd;
    int num_vox = fixed->npix;
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;

    /* This flag chooses between the one-shot version of the kernel 
       and the tile version for larger images */
    bool run_low_mem_kernel_version = false; 

    plm_timer_start (&timer);

    // Prepare the GPU to run the kernels.
    bspline_cuda_copy_coeff_lut(bxf);
    bspline_cuda_clear_score();
    bspline_cuda_clear_grad();

    // Run the kernels that fill the score, dc_dv, and gradient streams.
    bspline_cuda_calculate_run_kernels_g (
	fixed,
	moving,
	moving_grad,
	bxf,
	parms,
	run_low_mem_kernel_version,
	parms->debug);

    // Run the kernels to calculate the score and gradient values.
    bspline_cuda_final_steps_f (
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	&(ssd->score),
	ssd->grad,
	&ssd_grad_mean,
	&ssd_grad_norm);

    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}

/***********************************************************************
 * bspline_cuda_score_f_mse
 * 
 * The key feature of this implementation is the gradient calculation.  
 * To increase the overall parallelism, a thread is created for each 
 * control knot in the volume.  Each thread calculates the tiles by which
 * it is influenced, iterates through all the voxels in those tiles, and
 * sums up the total influence.  The total number of threads is in the
 * tens of thousands versus only 64 (or 64 * 3 = 192).
 *
 * The score and dc_dv values are calculated for the entire volume at 
 * once (as opposed to tile by tile or set by set), so the the memory 
 * requirements are slightly higher.  However, this does appear to
 * improve performance dramatically.
 ***********************************************************************/
void 
bspline_cuda_score_f_mse
(
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad)
{
    BSPLINE_Score* ssd = &bst->ssd;
    int num_vox = fixed->npix;
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;

    static int it = 0;
    char debug_fn[1024];
    FILE* fp = 0;
    //int dd = 0;

    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }

    plm_timer_start (&timer);

    // Prepare the GPU to run the kernels.
    bspline_cuda_copy_coeff_lut(bxf);
    bspline_cuda_clear_score();
    bspline_cuda_clear_grad();

    // Run the kernels that fill the score, dc_dv, and gradient streams.
    bspline_cuda_calculate_run_kernels_f(
	fixed,
	moving,
	moving_grad,
	bxf,
	parms);

    if (parms->debug) {
	fclose (fp);
    }

    // Run the kernels to calculate the score and gradient values.
    bspline_cuda_final_steps_f(
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	&(ssd->score),
	ssd->grad,
	&ssd_grad_mean,
	&ssd_grad_norm);

    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}

/***********************************************************************
 * bspline_cuda_score_e_mse_v2
 * 
 * The key feature of version "e" is that the tiles in the volume are
 * partitioned into "sets."  The tiles in each set are selected such
 * that no two tiles are influenced by the same control knots.  Since
 * tiles are influenced by a total of 4 knots in each dimension, there
 * are a total of 64 sets.  Performing operations on a "set by set"
 * basis rather than a "tile by tile" basis increases parallelism
 * without causing any conflicts.
 *
 * As compared to bspline_cuda_score_e_mse, this version first computes
 * the values in the score stream for the entire volume at once, and
 * then calculates the dc_dv values on a set by set basis.
 ***********************************************************************/
void 
bspline_cuda_score_e_mse_v2 
(
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad
)
{
    BSPLINE_Score* ssd = &bst->ssd;
    int num_vox = fixed->dim[0] * fixed->dim[1] * fixed->dim[2];;
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;

    int sidx[3];		// Index of the set, in the range [0, 63)

    static int it = 0;
    char debug_fn[1024];
    FILE* fp = 0;
    //int dd = 0;

    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }

    plm_timer_start (&timer);

    // Prepare the GPU to run the kernels.
    bspline_cuda_copy_coeff_lut(bxf);
    bspline_cuda_clear_score();
    bspline_cuda_clear_grad();
	  	
    // Calculate the score for the entire volume all at once.
    bspline_cuda_calculate_score_e(
	fixed,
	moving,
	moving_grad,
	bxf,
	parms);

    // There are 64 sets of tiles for which the score can be calculated in parallel.  Iterate through these sets.
    for(sidx[2] = 0; sidx[2] < 4; sidx[2]++) {
	for(sidx[1] = 0; sidx[1] < 4; sidx[1]++) {
	    for(sidx[0] = 0; sidx[0] < 4; sidx[0]++) {

		// printf("Running kernels for set (%d, %d, %d)...\n", sidx[0], sidx[1], sidx[2]);
		bspline_cuda_run_kernels_e_v2(
		    fixed,
		    moving,
		    moving_grad,
		    bxf,
		    parms,
		    sidx[0],
		    sidx[1],
		    sidx[2]);
	    }
	}
    }

    if (parms->debug) {
	fclose (fp);
    }

    // Compute the score.
    bspline_cuda_final_steps_e_v2(
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	&(ssd->score),
	ssd->grad,
	&ssd_grad_mean,
	&ssd_grad_norm);
	
    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}

/***********************************************************************
 * bspline_cuda_score_e_mse
 * 
 * The key feature of version "e" is that the tiles in the volume are
 * partitioned into "sets."  The tiles in each set are selected such
 * that no two tiles are influenced by the same control knots.  Since
 * tiles are influenced by a total of 4 knots in each dimension, there
 * are a total of 64 sets.  Performing operations on a "set by set"
 * basis rather than a "tile by tile" basis increases parallelism
 * without causing any conflicts.
 *
 * In this version, the score and dc_dv values are each computed on a
 * set by set basis.
 ***********************************************************************/
void 
bspline_cuda_score_e_mse 
(
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad)
{
    BSPLINE_Score* ssd = &bst->ssd;
	
    int num_vox;
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;

    static int it = 0;
    char debug_fn[1024];
    FILE* fp = 0;

    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }

    plm_timer_start (&timer);

    num_vox = fixed->dim[0] * fixed->dim[1] * fixed->dim[2];

    bspline_cuda_copy_coeff_lut(bxf);
    bspline_cuda_clear_score();
    bspline_cuda_clear_grad();

    // Index of the set, in the range [0, 63)
    int sidx[3];

    // There are 64 sets of tiles for which the score can be calculated 
    // in parallel.  Iterate through these sets.
    for(sidx[2] = 0; sidx[2] < 4; sidx[2]++) {
	for(sidx[1] = 0; sidx[1] < 4; sidx[1]++) {
	    for(sidx[0] = 0; sidx[0] < 4; sidx[0]++) {
		bspline_cuda_run_kernels_e(
		    fixed,
		    moving,
		    moving_grad,
		    bxf,
		    parms,
		    sidx[0],
		    sidx[1],
		    sidx[2]);
	    }
	}
    }

    if (parms->debug) {
	fclose (fp);
    }

    // Compute the score.
    bspline_cuda_final_steps_e(
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	&(ssd->score),
	ssd->grad,
	&ssd_grad_mean,
	&ssd_grad_norm);
	
    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}

/***********************************************************************
 * bspline_cuda_score_d_mse
 * 
 * The key feature of version "d" is that the score, dc_dv, and gradient
 * values are all computed on a tile by tile basis.  This reduces the
 * memory requirements on the GPU and allows larger images to be
 * processed.
 ***********************************************************************/
void bspline_cuda_score_d_mse
(
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad)
{
    BSPLINE_Score* ssd = &bst->ssd;
    int num_vox;
    int p[3];
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;
    static int it = 0;
    char debug_fn[1024];
    FILE* fp = 0;

    /* BEGIN CUDA VARIABLES */
    float *host_score;
    float *host_grad_norm;
    float *host_grad_mean;
    /* END CUDA VARIBLES */

    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }

    plm_timer_start (&timer);

    // dc_dv = (float*)malloc(3 * bxf->vox_per_rgn[0] * bxf->vox_per_rgn[1] * bxf->vox_per_rgn[2] * sizeof(float));
    // ssd->score = 0;
    // memset(ssd->grad, 0, bxf->num_coeff * sizeof(float));
    num_vox = fixed->dim[0] * fixed->dim[1] * fixed->dim[2];

    // host_dc_dv = (float*)malloc(3 * bxf->vox_per_rgn[0] * bxf->vox_per_rgn[1] * bxf->vox_per_rgn[2] * sizeof(float));
    // host_grad  = (float*)malloc(bxf->num_coeff * sizeof(float));
    host_score = (float*)malloc(sizeof(float));
    host_grad_norm = (float*)malloc(sizeof(float));
    host_grad_mean = (float*)malloc(sizeof(float));

    bspline_cuda_copy_coeff_lut(bxf);
    bspline_cuda_clear_score();
    bspline_cuda_clear_grad();

    /* Serial across tiles */
    for (p[2] = 0; p[2] < bxf->rdims[2]; p[2]++) {
	for (p[1] = 0; p[1] < bxf->rdims[1]; p[1]++) {
	    for (p[0] = 0; p[0] < bxf->rdims[0]; p[0]++) {

		// printf ("Kernel 1, tile %d %d %d\n", p[0], p[1], p[2]);

		bspline_cuda_run_kernels_d(
		    fixed,
		    moving,
		    moving_grad,
		    bxf,
		    parms,
		    p[0],
		    p[1],
		    p[2]);
				
		/* ORIGINAL CPU CODE

		// Compute linear index for tile.
		pidx = ((p[2] * bxf->rdims[1] + p[1]) * bxf->rdims[0]) + p[0];

		// Find c_lut row for this tile
		c_lut = &bxf->c_lut[pidx*64];

		// Parallel across offsets
		for (q[2] = 0; q[2] < bxf->vox_per_rgn[2]; q[2]++) {
		for (q[1] = 0; q[1] < bxf->vox_per_rgn[1]; q[1]++) {
		for (q[0] = 0; q[0] < bxf->vox_per_rgn[0]; q[0]++) {
							
		// Compute linear index for this offset
		qidx = ((q[2] * bxf->vox_per_rgn[1] + q[1]) * bxf->vox_per_rgn[0]) + q[0];					

		// Tentatively mark this pixel as no contribution
		dc_dv[3*qidx+0] = 0.f;
		dc_dv[3*qidx+1] = 0.f;
		dc_dv[3*qidx+2] = 0.f;

		// Get (i,j,k) index of the voxel
		fi = bxf->roi_offset[0] + p[0] * bxf->vox_per_rgn[0] + q[0];
		fj = bxf->roi_offset[1] + p[1] * bxf->vox_per_rgn[1] + q[1];
		fk = bxf->roi_offset[2] + p[2] * bxf->vox_per_rgn[2] + q[2];

		// Some of the pixels are outside image
		if (fi > bxf->roi_offset[0] + bxf->roi_dim[0]) continue;
		if (fj > bxf->roi_offset[1] + bxf->roi_dim[1]) continue;
		if (fk > bxf->roi_offset[2] + bxf->roi_dim[2]) continue;

		// Compute physical coordinates of fixed image voxel
		fx = bxf->img_origin[0] + bxf->img_spacing[0] * fi;
		fy = bxf->img_origin[1] + bxf->img_spacing[1] * fj;
		fz = bxf->img_origin[2] + bxf->img_spacing[2] * fk;

		// Compute linear index of fixed image voxel
		fv = fk * fixed->dim[0] * fixed->dim[1] + fj * fixed->dim[0] + fi;

		// Get B-spline deformation vector
		bspline_interp_pix_b_inline (dxyz, bxf, pidx, qidx);

		// Find correspondence in moving image
		mx = fx + dxyz[0];
		mi = (mx - moving->offset[0]) / moving->pix_spacing[0];
		if (mi < -0.5 || mi > moving->dim[0] - 0.5) continue;

		my = fy + dxyz[1];
		mj = (my - moving->offset[1]) / moving->pix_spacing[1];
		if (mj < -0.5 || mj > moving->dim[1] - 0.5) continue;

		mz = fz + dxyz[2];
		mk = (mz - moving->offset[2]) / moving->pix_spacing[2];
		if (mk < -0.5 || mk > moving->dim[2] - 0.5) continue;

		// Compute interpolation fractions
		clamp_linear_interpolate_inline (mi, moving->dim[0]-1, &mif, &mir, &fx1, &fx2);
		clamp_linear_interpolate_inline (mj, moving->dim[1]-1, &mjf, &mjr, &fy1, &fy2);
		clamp_linear_interpolate_inline (mk, moving->dim[2]-1, &mkf, &mkr, &fz1, &fz2);

		// Compute moving image intensity using linear interpolation
		mvf = (mkf * moving->dim[1] + mjf) * moving->dim[0] + mif;
		m_x1y1z1 = fx1 * fy1 * fz1 * m_img[mvf];
		m_x2y1z1 = fx2 * fy1 * fz1 * m_img[mvf+1];
		m_x1y2z1 = fx1 * fy2 * fz1 * m_img[mvf+moving->dim[0]];
		m_x2y2z1 = fx2 * fy2 * fz1 * m_img[mvf+moving->dim[0]+1];
		m_x1y1z2 = fx1 * fy1 * fz2 * m_img[mvf+moving->dim[1]*moving->dim[0]];
		m_x2y1z2 = fx2 * fy1 * fz2 * m_img[mvf+moving->dim[1]*moving->dim[0]+1];
		m_x1y2z2 = fx1 * fy2 * fz2 * m_img[mvf+moving->dim[1]*moving->dim[0]+moving->dim[0]];
		m_x2y2z2 = fx2 * fy2 * fz2 * m_img[mvf+moving->dim[1]*moving->dim[0]+moving->dim[0]+1];
		m_val = m_x1y1z1 + m_x2y1z1 + m_x1y2z1 + m_x2y2z1 
		+ m_x1y1z2 + m_x2y1z2 + m_x1y2z2 + m_x2y2z2;

		// Compute intensity difference
		diff = f_img[fv] - m_val;

		// We'll go ahead and accumulate the score here, but you would 
		// have to reduce somewhere else instead
		ssd->score += diff * diff;
		num_vox ++;

		// Compute spatial gradient using nearest neighbors
		mvr = (mkr * moving->dim[1] + mjr) * moving->dim[0] + mir;
							
		// Store dc_dv for this offset
		dc_dv[3*qidx+0] = diff * m_grad[3*mvr+0];  // x component
		dc_dv[3*qidx+1] = diff * m_grad[3*mvr+1];  // y component
		dc_dv[3*qidx+2] = diff * m_grad[3*mvr+2];  // z component

		// Compare to CUDA results.
		if((dc_dv[3*qidx+0] < (host_dc_dv[3*qidx+0] - 5.0) || dc_dv[3*qidx+0] > (host_dc_dv[3*qidx+0] + 5.0)) ||
		(dc_dv[3*qidx+1] < (host_dc_dv[3*qidx+1] - 5.0) || dc_dv[3*qidx+1] > (host_dc_dv[3*qidx+1] + 5.0)) ||
		(dc_dv[3*qidx+2] < (host_dc_dv[3*qidx+2] - 5.0) || dc_dv[3*qidx+2] > (host_dc_dv[3*qidx+2] + 5.0))) {
		printf("%d\tCPU: (%f, %f, %f) \t GPU: (%f, %f, %f) \n", 
		3*qidx+0, 
		dc_dv[3*qidx+0],
		dc_dv[3*qidx+1], 
		dc_dv[3*qidx+2], 
		host_dc_dv[3*qidx+0],
		host_dc_dv[3*qidx+1],
		host_dc_dv[3*qidx+2]);
									
		dc_dv_errors++;
		}							
		}
		}
		}
				
		//printf("dc_dv_errors = %d\n", dc_dv_errors);
		dc_dv_errors = 0;
				
		//printf ("Kernel 2, tile %d %d %d\n", p[0], p[1], p[2]);

		// Parallel across 64 control points
		for (k = 0; k < 4; k++) {
		for (j = 0; j < 4; j++) {
		for (i = 0; i < 4; i++) {

		// Compute linear index of control point
		m = k*16 + j*4 + i;

		// Find index of control point within coefficient array
		cidx = c_lut[m] * 3;
							
		// Serial across offsets within kernel 
		for (qidx = 0, q[2] = 0; q[2] < bxf->vox_per_rgn[2]; q[2]++) {
		for (q[1] = 0; q[1] < bxf->vox_per_rgn[1]; q[1]++) {
		for (q[0] = 0; q[0] < bxf->vox_per_rgn[0]; q[0]++, qidx++) {
										
		// Find q_lut row for this offset
		float *q_lut = &bxf->q_lut[qidx*64];

		// Accumulate update to gradient for this control point
		ssd->grad[cidx+0] += dc_dv[3*qidx+0] * q_lut[m];
		ssd->grad[cidx+1] += dc_dv[3*qidx+1] * q_lut[m];
		ssd->grad[cidx+2] += dc_dv[3*qidx+2] * q_lut[m];	
		}
		}
		}				

							
		if((host_grad[cidx+0] < (ssd->grad[cidx+0] - 1) || host_grad[cidx+0] > (ssd->grad[cidx+0] + 1)) ||
		(host_grad[cidx+1] < (ssd->grad[cidx+1] - 1) || host_grad[cidx+1] > (ssd->grad[cidx+1] + 1)) ||
		(host_grad[cidx+2] < (ssd->grad[cidx+2] - 1) || host_grad[cidx+2] > (ssd->grad[cidx+2] + 1))) {
		printf("%d\tCPU: (%.4f, %.4f, %.4f)\tGPU: (%.4f, %.4f, %.4f) \n", 
		cidx, 
		ssd->grad[cidx+0],
		ssd->grad[cidx+1], 
		ssd->grad[cidx+2], 
		host_grad[cidx+0],
		host_grad[cidx+1],
		host_grad[cidx+2]);
		errors++;
		}
		}
		}
		}
		*/
	    }
	}
    }

    if (parms->debug) {
	fclose (fp);
    }

    // Compute the score.
    bspline_cuda_final_steps_d(
	parms,
	bxf,
	fixed,
	bxf->vox_per_rgn,
	fixed->dim,
	host_score,
	ssd->grad,
	host_grad_mean,
	host_grad_norm);
	
    ssd->score = *host_score;
    ssd_grad_mean = *host_grad_mean;
    ssd_grad_norm = *host_grad_norm;
	
    //free(dc_dv);
    //free(host_dc_dv);
    //free(host_grad);
    free(host_score);
    free(host_grad_norm);
    free(host_grad_mean);

    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}

/***********************************************************************
 * bspline_cuda_score_c_mse
 * 
 * This version corresponds to version "c" on the CPU.  It requires the
 * most memory and is fairly slow in comparison to the other versions.
 ***********************************************************************/
void 
bspline_cuda_score_c_mse 
(
    BSPLINE_Parms *parms, 
    Bspline_state *bst,
    BSPLINE_Xform* bxf, 
    Volume *fixed, 
    Volume *moving, 
    Volume *moving_grad)
{
    BSPLINE_Score* ssd = &bst->ssd;
    //int i;
    int ri, rj, rk;
    int fi, fj, fk, fv;
    //float mi, mj, mk;
    float fx, fy, fz;
    //float mx, my, mz;
    //int mif, mjf, mkf, mvf;  /* Floor */
    //int mir, mjr, mkr, mvr;  /* Round */
    int p[3];
    int q[3];
    float diff;
    float dc_dv[3];
    //float fx1, fx2, fy1, fy2, fz1, fz2;
    //    float* f_img = (float*) fixed->img;
    //    float* m_img = (float*) moving->img;
    //    float* m_grad = (float*) moving_grad->img;
    //float dxyz[3];
    int num_vox;
    int pidx, qidx;
    float ssd_grad_norm, ssd_grad_mean;
    Timer timer;
    //float m_val;
    //float m_x1y1z1, m_x2y1z1, m_x1y2z1, m_x2y2z1;
    //float m_x1y1z2, m_x2y1z2, m_x1y2z2, m_x2y2z2;

    static int it = 0;
    char debug_fn[1024];
    FILE* fp = 0;
    //    int dd = 0;

    /* BEGIN CUDA VARIABLES */
    float *host_diff;
    float *host_dc_dv_x;
    float *host_dc_dv_y;
    float *host_dc_dv_z;
    float *host_score;
    float *host_grad_norm;
    float *host_grad_mean;
    //    int diff_errors = 0;
    //    int dc_dv_errors = 0;
    /* END CUDA VARIBLES */

    if (parms->debug) {
	sprintf (debug_fn, "dump_mse_%02d.txt", it++);
	fp = fopen (debug_fn, "w");
    }

    plm_timer_start (&timer);

    /* Run CUDA code now so the results can be compared below. */
    /* BEGIN CUDA CALLS */
    host_diff = (float*)malloc(fixed->npix * sizeof(float));
    host_dc_dv_x = (float*)malloc(fixed->npix * sizeof(float));
    host_dc_dv_y = (float*)malloc(fixed->npix * sizeof(float));
    host_dc_dv_z = (float*)malloc(fixed->npix * sizeof(float));
    host_score   = (float*)malloc(sizeof(float));
    host_grad_norm = (float*)malloc(sizeof(float));
    host_grad_mean = (float*)malloc(sizeof(float));

    bspline_cuda_run_kernels_c (fixed,
				moving,
				moving_grad,
				bxf,
				parms,
				host_diff,
				host_dc_dv_x,
				host_dc_dv_y,
				host_dc_dv_z,
				host_score);
    /* END CUDA CALLS */

    ssd->score = 0;
    memset (ssd->grad, 0, bxf->num_coeff * sizeof(float));
    num_vox = 0;
    for (rk = 0, fk = bxf->roi_offset[2]; rk < bxf->roi_dim[2]; rk++, fk++) {
	p[2] = rk / bxf->vox_per_rgn[2];
	q[2] = rk % bxf->vox_per_rgn[2];
	fz = bxf->img_origin[2] + bxf->img_spacing[2] * fk;
		
	for (rj = 0, fj = bxf->roi_offset[1]; rj < bxf->roi_dim[1]; rj++, fj++) {
	    p[1] = rj / bxf->vox_per_rgn[1];
	    q[1] = rj % bxf->vox_per_rgn[1];
	    fy = bxf->img_origin[1] + bxf->img_spacing[1] * fj;
		    
	    for (ri = 0, fi = bxf->roi_offset[0]; ri < bxf->roi_dim[0]; ri++, fi++) {
		p[0] = ri / bxf->vox_per_rgn[0];
		q[0] = ri % bxf->vox_per_rgn[0];
		fx = bxf->img_origin[0] + bxf->img_spacing[0] * fi;

		// Get B-spline deformation vector.
		pidx = ((p[2] * bxf->rdims[1] + p[1]) * bxf->rdims[0]) + p[0];
		qidx = ((q[2] * bxf->vox_per_rgn[1] + q[1]) * bxf->vox_per_rgn[0]) + q[0];

		// Compute coordinate of fixed image voxel.
		fv = fk * fixed->dim[0] * fixed->dim[1] + fj * fixed->dim[0] + fi;

		diff = host_diff[fv];
		dc_dv[0] = host_dc_dv_x[fv];
		dc_dv[1] = host_dc_dv_y[fv];
		dc_dv[2] = host_dc_dv_z[fv];
				
		bspline_update_grad_b_inline (bst, bxf, pidx, qidx, dc_dv);
				
		// ssd->score += diff * diff;
		// num_vox ++;
	    }
	}
    }

    if (parms->debug) {
	fclose (fp);
    }

    bspline_cuda_calculate_gradient_c (parms,
				       bst, 
				       bxf,
				       fixed,
				       host_grad_norm,
				       host_grad_mean);

    ssd->score = *host_score;
    ssd_grad_norm = *host_grad_norm;
    ssd_grad_mean = *host_grad_mean;

    free(host_diff);
    free(host_dc_dv_x);
    free(host_dc_dv_y);
    free(host_dc_dv_z);
    free(host_score);
    free(host_grad_norm);
    free(host_grad_mean);

    report_score ("MSE", bxf, bst, num_vox, plm_timer_report (&timer));
}
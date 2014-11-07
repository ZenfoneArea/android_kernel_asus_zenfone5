/* Release Version: ci_master_20140107_1001 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _IA_CSS_TYPES_H_
#define _IA_CSS_TYPES_H_

/*! \file */

/** @file ia_css_types.h
 * This file contains types used for the ia_css parameters.
 * These types are in a separate file because they are expected
 * to be used in software layers that do not access the CSS API
 * directly but still need to forward parameters for it.
 */

/* This code is also used by Silicon Hive in a simulation environment
 * Therefore, the following macro is used to differentiate when this
 * code is being included from within the Linux kernel source
 */

#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stdint.h>
#endif
#if defined(IS_ISP_2500_SYSTEM)
#if defined(__HOST__)
#include "components_types.host.h"                /* Skylake kernel settings structs */
#endif
#endif


#include "ia_css_frac.h"

#include "isp/kernels/aa/aa_2/ia_css_aa2_types.h"
#include "isp/kernels/anr/anr_1.0/ia_css_anr_types.h"
#include "isp/kernels/anr/anr_2/ia_css_anr2_types.h"
#include "isp/kernels/cnr/cnr_2/ia_css_cnr2_types.h"
#include "isp/kernels/csc/csc_1.0/ia_css_csc_types.h"
#include "isp/kernels/ctc/ctc_1.0/ia_css_ctc_types.h"
#include "isp/kernels/dp/dp_1.0/ia_css_dp_types.h"
#include "isp/kernels/de/de_1.0/ia_css_de_types.h"
#include "isp/kernels/de/de_2/ia_css_de2_types.h"
#include "isp/kernels/fpn/fpn_1.0/ia_css_fpn_types.h"
#include "isp/kernels/gc/gc_1.0/ia_css_gc_types.h"
#include "isp/kernels/gc/gc_2/ia_css_gc2_types.h"
#include "isp/kernels/macc/macc_1.0/ia_css_macc_types.h"
#include "isp/kernels/ob/ob_1.0/ia_css_ob_types.h"
#include "isp/kernels/s3a/s3a_1.0/ia_css_s3a_types.h"
#include "isp/kernels/sc/sc_1.0/ia_css_sc_types.h"
#include "isp/kernels/tnr/tnr_1.0/ia_css_tnr_types.h"
#include "isp/kernels/wb/wb_1.0/ia_css_wb_types.h"
#include "isp/kernels/xnr/xnr_1.0/ia_css_xnr_types.h"
#include "isp/kernels/ynr/ynr_1.0/ia_css_ynr_types.h"
#include "isp/kernels/ynr/ynr_2/ia_css_ynr2_types.h"

#define IA_CSS_VERSION_MAJOR    2
#define IA_CSS_VERSION_MINOR    0
#define IA_CSS_VERSION_REVISION 2

#define IA_CSS_MORPH_TABLE_NUM_PLANES  6

/** Number of DVS coefficient types */
#define IA_CSS_DVS_NUM_COEF_TYPES      6
#define IA_CSS_DVS_COEF_TYPES_ON_DMEM  2
#define IA_CSS_DVS2_NUM_COEF_TYPES     4

/* Virtual address within the CSS address space. */
typedef uint32_t ia_css_ptr;

/** Vector with signed values. This is used to indicate motion for
 * Digital Image Stabilization.
 */
struct ia_css_vector {
	int32_t x; /**< horizontal motion (in pixels) */
	int32_t y; /**< vertical motion (in pixels) */
};

/** DVS statistics grid
 *
 *  ISP block: SDVS1 (DIS/DVS Support for DIS/DVS ver.1 (2-axes))
 *             SDVS2 (DVS Support for DVS ver.2 (6-axes))
 *  ISP1: SDVS1 is used.
 *  ISP2: SDVS2 is used.
 */
struct ia_css_dvs_grid_info {
	uint32_t enable;        /**< DVS statistics enabled.
					0:disabled, 1:enabled */
	uint32_t width;	    	/**< Width of DVS grid table.
					(= Horizontal number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, this is equal to
					 the number of vertical statistics. */
	uint32_t aligned_width; /**< Stride of each grid line.
					(= Horizontal number of grid cells
					in table, which means
					the allocated width.) */
	uint32_t height;	/**< Height of DVS grid table.
					(= Vertical number of grid cells
					in table, which cells have effective
					statistics.)
					For DVS1, This is equal to
					the number of horizontal statistics. */
	uint32_t aligned_height;/**< Stride of each grid column.
					(= Vertical number of grid cells
					in table, which means
					the allocated height.) */
	uint32_t bqs_per_grid_cell; /**< Grid cell size in BQ(Bayer Quad) unit.
					(1BQ means {Gr,R,B,Gb}(2x2 pixels).)
					For DVS1, valid value is 64.
					For DVS2, valid value is only 64,
					currently. */
	uint32_t num_hor_coefs;	/**< Number of horizontal coefficients. */
	uint32_t num_ver_coefs;	/**< Number of vertical coefficients. */
};

/** structure that describes the 3A and DIS grids */
struct ia_css_grid_info {
	/** \name ISP input size
	  * that is visible for user
	  * @{
	  */
	uint32_t isp_in_width;
	uint32_t isp_in_height;
	/** @}*/

	struct ia_css_3a_grid_info  s3a_grid; /**< 3A grid info */
	struct ia_css_dvs_grid_info dvs_grid; /**< DVS grid info */

	enum ia_css_vamem_type vamem_type;
};

/** Morphing table, used for geometric distortion and chromatic abberration
 *  correction (GDCAC, also called GDC).
 *  This table describes the imperfections introduced by the lens, the
 *  advanced ISP can correct for these imperfections using this table.
 */
struct ia_css_morph_table {
	uint32_t enable; /**< To disable GDC, set this field to false. The
		          coordinates fields can be set to NULL in this case. */
	uint32_t height; /**< Table height */
	uint32_t width;  /**< Table width */
	uint16_t *coordinates_x[IA_CSS_MORPH_TABLE_NUM_PLANES];
	/**< X coordinates that describe the sensor imperfection */
	uint16_t *coordinates_y[IA_CSS_MORPH_TABLE_NUM_PLANES];
	/**< Y coordinates that describe the sensor imperfection */
};

struct ia_css_dvs_6axis_config {
	unsigned int exp_id;
	uint32_t width_y;
	uint32_t height_y;
	uint32_t width_uv;
	uint32_t height_uv;
	uint32_t *xcoords_y;
	uint32_t *ycoords_y;
	uint32_t *xcoords_uv;
	uint32_t *ycoords_uv;
};

/**
 * Digital zoom:
 * This feature is currently available only for video, but will become
 * available for preview and capture as well.
 * Set the digital zoom factor, this is a logarithmic scale. The actual zoom
 * factor will be 64/x.
 * Setting dx or dy to 0 disables digital zoom for that direction.
 */
struct ia_css_dz_config {
	uint32_t dx;
	uint32_t dy;
};

/** The still capture mode, this can be RAW (simply copy sensor input to DDR),
 *  Primary ISP, the Advanced ISP (GDC) or the low-light ISP (ANR).
 */
enum ia_css_capture_mode {
	IA_CSS_CAPTURE_MODE_RAW,      /**< no processing, copy data only */
	IA_CSS_CAPTURE_MODE_BAYER,    /**< bayer processing, up to demosaic */
	IA_CSS_CAPTURE_MODE_PRIMARY,  /**< primary ISP */
	IA_CSS_CAPTURE_MODE_ADVANCED, /**< advanced ISP (GDC) */
	IA_CSS_CAPTURE_MODE_LOW_LIGHT /**< low light ISP (ANR) */
};

struct ia_css_capture_config {
	enum ia_css_capture_mode mode; /**< Still capture mode */
	uint32_t enable_xnr;	       /**< Enable/disable XNR */
	uint32_t enable_raw_output;
};

/** ISP filter configuration. This is a collection of configurations
 *  for each of the ISP filters (modules).
 *
 *  NOTE! The contents of all pointers is copied when get or set with the
 *  exception of the shading and morph tables. For these we only copy the
 *  pointer, so the caller must make sure the memory contents of these pointers
 *  remain valid as long as they are used by the CSS. This will be fixed in the
 *  future by copying the contents instead of just the pointer.
 *
 *  Comment:
 *    ["ISP block", 1&2]   : ISP block is used both for ISP1 and ISP2.
 *    ["ISP block", 1only] : ISP block is used only for ISP1.
 *    ["ISP block", 2only] : ISP block is used only for ISP2.
 */
struct ia_css_isp_config {
	struct ia_css_wb_config   *wb_config;	/**< White Balance
							[WB1, 1&2] */
	struct ia_css_cc_config   *cc_config;  	/**< Color Correction
							[CSC1, 1only] */
	struct ia_css_tnr_config  *tnr_config; 	/**< Temporal Noise Reduction
							[TNR1, 1&2] */
	struct ia_css_ecd_config  *ecd_config; 	/**< Eigen Color Demosaicing
							[DE2, 2only] */
	struct ia_css_ynr_config  *ynr_config; 	/**< Y(Luma) Noise Reduction
							[YNR2&YEE2, 2only] */
	struct ia_css_fc_config   *fc_config;  	/**< Fringe Control
							[FC2, 2only] */
	struct ia_css_cnr_config  *cnr_config; 	/**< Chroma Noise Reduction
							[CNR2, 2only] */
	struct ia_css_macc_config *macc_config;	/**< MACC
							[MACC2, 2only] */
	struct ia_css_ctc_config  *ctc_config; 	/**< Chroma Tone Control
							[CTC2, 2only] */
	struct ia_css_aa_config   *aa_config;	/**< YUV Anti-Aliasing
							[AA2, 2only]
						        (not used currently) */
	struct ia_css_aa_config   *baa_config;	/**< Bayer Anti-Aliasing
							[BAA2, 1&2] */
	struct ia_css_ce_config   *ce_config;	/**< Chroma Enhancement
							[CE1, 1only] */
	struct ia_css_dvs_6axis_config *dvs_6axis_config;
	struct ia_css_ob_config   *ob_config;  /**< Objective Black
							[OB1, 1&2] */
	struct ia_css_dp_config   *dp_config;  /**< Defect Pixel Correction
							[DPC1/DPC2, 1&2] */
	struct ia_css_nr_config   *nr_config;  /**< Noise Reduction
							[BNR1&YNR1&CNR1, 1&2]*/
	struct ia_css_ee_config   *ee_config;  /**< Edge Enhancement
							[YEE1, 1&2] */
	struct ia_css_de_config   *de_config;  /**< Demosaic
							[DE1, 1only] */
	struct ia_css_gc_config   *gc_config;  /**< Gamma Correction (for YUV)
							[GC1, 1only] */
	struct ia_css_anr_config  *anr_config; /**< Advanced Noise Reduction */
	struct ia_css_3a_config   *s3a_config; /**< 3A Statistics config */
	struct ia_css_xnr_config  *xnr_config; /**< eXtra Noise Reduction */
	struct ia_css_dz_config   *dz_config;  /**< Digital Zoom */
	struct ia_css_cc_config *yuv2rgb_cc_config; /**< Color Correction
							[CCM2, 2only] */
	struct ia_css_cc_config *rgb2yuv_cc_config; /**< Color Correction
							[CSC2, 2only] */
	struct ia_css_macc_table  *macc_table;	/**< MACC
							[MACC1/MACC2, 1&2]*/
	struct ia_css_gamma_table *gamma_table;	/**< Gamma Correction (for YUV)
							[GC1, 1only] */
	struct ia_css_ctc_table   *ctc_table;	/**< Chroma Tone Control
							[CTC1, 1only] */

	/** \deprecated */
	struct ia_css_xnr_table   *xnr_table;	/**< eXtra Noise Reduction
							[XNR1, 1&2] */
	struct ia_css_rgb_gamma_table *r_gamma_table;/**< sRGB Gamma Correction
							[GC2, 2only] */
	struct ia_css_rgb_gamma_table *g_gamma_table;/**< sRGB Gamma Correction
							[GC2, 2only] */
	struct ia_css_rgb_gamma_table *b_gamma_table;/**< sRGB Gamma Correction
							[GC2, 2only] */
	struct ia_css_vector      *motion_vector; /**< For 2-axis DVS */
	struct ia_css_shading_table *shading_table;
	struct ia_css_morph_table   *morph_table;
	struct ia_css_dvs_coefficients *dvs_coefs; /**< DVS 1.0 coefficients */
	struct ia_css_dvs2_coefficients *dvs2_coefs; /**< DVS 2.0 coefficients */
	struct ia_css_capture_config   *capture_config;
	struct ia_css_anr_thres   *anr_thres;

	struct ia_css_2500_lin_kernel_config     *lin_2500_config;       /**< Skylake: Linearization config */
	struct ia_css_2500_obgrid_kernel_config  *obgrid_2500_config;    /**< Skylake: OBGRID config */
	struct ia_css_2500_bnr_kernel_config     *bnr_2500_config;       /**< Skylake: bayer denoise config */
	struct ia_css_2500_shd_kernel_config     *shd_2500_config;       /**< Skylake: shading config */
	struct ia_css_2500_dm_kernel_config      *dm_2500_config;        /**< Skylake: demosaic config */
	struct ia_css_2500_rgbpp_kernel_config   *rgbpp_2500_config;     /**< Skylake: RGBPP config */
	struct ia_css_2500_yuvp1_kernel_config   *yuvp1_2500_config;     /**< Skylake: yuvp1 config */
	struct ia_css_2500_yuvp2_kernel_config   *yuvp2_2500_config;     /**< Skylake: yuvp2 config */
	struct ia_css_2500_tnr_kernel_config     *tnr_2500_config;       /**< Skylake: TNR config */
	struct ia_css_2500_dpc_kernel_config     *dpc_2500_config;       /**< Skylake: DPC config */
	struct ia_css_2500_awb_kernel_config     *awb_2500_config;       /**< Skylake: auto white balance config */
	struct ia_css_2500_awb_fr_kernel_config  *awb_fr_2500_config;    /**< Skylake: auto white balance filter response config */
	struct ia_css_2500_anr_kernel_config     *anr_2500_config;       /**< Skylake: ANR config */
	struct ia_css_2500_af_kernel_config      *af_2500_config;        /**< Skylake: auto focus config */
	struct ia_css_2500_ae_kernel_config      *ae_2500_config;        /**< Skylake: auto exposure config */
	struct ia_css_2500_bds_kernel_config     *bds_2500_config;       /**< Skylake: bayer downscaler config */
	struct ia_css_2500_dvs_kernel_config     *dvs_2500_config;       /**< Skylake: digital video stabilization config */
};

/** DVS 1.0 Coefficients.
 *  This structure describes the coefficients that are needed for the dvs statistics.
 */

struct ia_css_dvs_coefficients {
	struct ia_css_dvs_grid_info grid;/**< grid info contains the dimensions of the dvs grid */
	int16_t *hor_coefs;	/**< the pointer to int16_t[grid.num_hor_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the horizontal coefficients */
	int16_t *ver_coefs;	/**< the pointer to int16_t[grid.num_ver_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the vertical coefficients */
};

/** DVS 1.0 Statistics.
 *  This structure describes the statistics that are generated using the provided coefficients.
 */

struct ia_css_dvs_statistics {
	struct ia_css_dvs_grid_info grid;/**< grid info contains the dimensions of the dvs grid */
	int32_t *hor_proj;	/**< the pointer to int16_t[grid.height * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the horizontal projections */
	int32_t *ver_proj;	/**< the pointer to int16_t[grid.width * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the vertical projections */
};

/** DVS 2.0 Coefficient types. This structure contains 4 pointers to
 *  arrays that contain the coeffients for each type.
 */
struct ia_css_dvs2_coef_types {
	int16_t *odd_real; /**< real part of the odd coefficients*/
	int16_t *odd_imag; /**< imaginary part of the odd coefficients*/
	int16_t *even_real;/**< real part of the even coefficients*/
	int16_t *even_imag;/**< imaginary part of the even coefficients*/
};

/** DVS 2.0 Coefficients. This structure describes the coefficients that are needed for the dvs statistics.
 *  e.g. hor_coefs.odd_real is the pointer to int16_t[grid.num_hor_coefs] containing the horizontal odd real 
 *  coefficients.
 */
struct ia_css_dvs2_coefficients {
	struct ia_css_dvs_grid_info grid;        /**< grid info contains the dimensions of the dvs grid */
	struct ia_css_dvs2_coef_types hor_coefs; /**< struct with pointers that contain the horizontal coefficients */
	struct ia_css_dvs2_coef_types ver_coefs; /**< struct with pointers that contain the vertical coefficients */
};

/** DVS 2.0 Statistic types. This structure contains 4 pointers to
 *  arrays that contain the statistics for each type.
 */
struct ia_css_dvs2_stat_types {
	int32_t *odd_real; /**< real part of the odd statistics*/
	int32_t *odd_imag; /**< imaginary part of the odd statistics*/
	int32_t *even_real;/**< real part of the even statistics*/
	int32_t *even_imag;/**< imaginary part of the even statistics*/
};

/** DVS 2.0 Statistics. This structure describes the statistics that are generated using the provided coefficients.
 *  e.g. hor_prod.odd_real is the pointer to int16_t[grid.aligned_height][grid.aligned_width] containing 
 *  the horizontal odd real statistics. Valid statistics data area is int16_t[0..grid.height-1][0..grid.width-1]
 */
struct ia_css_dvs2_statistics {
	struct ia_css_dvs_grid_info grid;       /**< grid info contains the dimensions of the dvs grid */
	struct ia_css_dvs2_stat_types hor_prod; /**< struct with pointers that contain the horizontal statistics */
	struct ia_css_dvs2_stat_types ver_prod; /**< struct with pointers that contain the vertical statistics */
};

#endif /* _IA_CSS_TYPES_H_ */

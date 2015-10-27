/******************************************************************************************************
DISCLAIMER NOTICE
*******************************************************************************************************
We are not affiliated, associated, authorized, endorsed by, 
or in any way officially connected with Xilinx, Inc. 
or any of its subsidiaries or its affiliates.  
In no event whatsoever shall Xilinx, Inc. 
or any of its subsidiaries or its affiliates have any warranty or support commitment for this software 
or liability for loss, injury or damage in connection with this software, 
including but not limited to the use or display thereof.
*******************************************************************************************************
*/
/**
* Replacement for the XHwIcap_GetClbBits and XHwIcap_SetClbBits functions for the Zynq.
* Created by Ghent University.
**/


#include "xil_assert.h"
#include <xstatus.h>

#include "micap_custom.h"

#if (XHI_FAMILY != XHI_DEV_FAMILY_7SERIES)
    #error You are using the wrong xhwicap_getset_clb driver files. This file is specific to 7 series FPGAs only!!
#endif

typedef struct {
	long slice_row;
	long slice_col;
	const u8 (*Resource)[2];
	const u8 *Value;
	long NumBits;
} LUT_config_type;


/**
* Returns true if the configuration of two slices are stored in the same set of frames.
**/
u8 MiCAP_Custom_IsSameFrame(long slice_row0, long slice_col0, long slice_row1, long slice_col1);

/**
* Sets bits contained in multiple LUTs specified by the coordinates and data in the lut_configs array.
*
* @return	XST_SUCCESS or XST_FAILURE.
**/
int MiCAP_Custom_SetMultiClbBits(LUT_config_type  *lut_configs, u32 num_lut_configs);

//Signatures
int MiCAP_DeviceReadFrames(u32 frameAddress, u8 NumFrames, u32 *FrameBuffer);

int MiCAP_DeviceWriteFrames(u32 frameAddress, u8 NumFrames,u32 *FrameData);

u32 MiCAP_Custom_GetClbFramesSize(void);

void MiCAP_GetConfigurationCoordinates(long slice_row, long slice_col,
        u8 *bottom_ntop_p, int *clock_row_p, u32 *major_frame_address_p, u32 *word_offset_p);
/**
 *FAR setup for 7 series FPGA family
 */
#if(XHI_FAMILY == XHI_DEV_FAMILY_7SERIES)
#define XHwIcap_Custom_SetupFar7series(Top, Block, Row, ColumnAddress, MinorAddress)  \
		(Block << XHI_FAR_BLOCK_SHIFT_7) | \
		((Top << XHI_FAR_TOP_BOTTOM_SHIFT_7) | \
		(Row << XHI_FAR_ROW_ADDR_SHIFT_7) | \
		(ColumnAddress << XHI_FAR_COLUMN_ADDR_SHIFT_7) | \
		(MinorAddress << XHI_FAR_MINOR_ADDR_SHIFT_7))
#endif


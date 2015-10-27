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
 * Type 1 drivers for MiCAP (Multi -Read, -Modify and Write)
* Created by Ghent University.
**/

#ifndef MICAP_MULTI_GETSET_CLB_H_
#define MICAP_MULTI_GETSET_CLB_H_

#include "assert.h"
#include "micap_multi_set_clb_7series.h"
#include "micap_custom.h"

#define FRAME_NUM_CLB 4
 
 
/**
* Returns the size of the frames of one stack of clbs in 32-bit words.
**/
u32 MiCAP_Custom_GetClbFramesSize(void) {
    return FRAME_NUM_CLB * 101; //For Zynq-SoC only.
}

/**
* Returns the word offset of the configuration of a CLB within a frame.
**/
u32 MiCAP_Custom_GetWordOffset(long slice_row) {
    u32 num_clock_rows = 3; // Specific to Zynq-SoC only.
    u32 num_clb_rows = 150; // Specific to Zynq-SoC only.
    u32 num_clb_rows_per_clock_row = num_clb_rows / num_clock_rows;

    assert(slice_row >= 0 && slice_row < num_clb_rows);

  	/* Calculating word offset */
    u32 word_offset = 0;
	slice_row = slice_row % num_clb_rows_per_clock_row;

	if (slice_row >= num_clb_rows_per_clock_row/2){
		word_offset = slice_row * 2 + 1;
	}
	else {
		word_offset = slice_row * 2;
	}
	return word_offset;
}

/**
* Returns true if the configuration of two slices are stored in the same set of frames.
**/
u8 MiCAP_Custom_IsSameFrame(long slice_row0, long slice_col0, long slice_row1, long slice_col1) {
    u32 num_clock_rows = 3; // Specific to Zynq-SoC only.
    u32 num_clb_rows = 150; // Specific to Zynq-SoC only.
    u32 num_clb_rows_per_clock_row = num_clb_rows / num_clock_rows;

    u8 slice_grp0 = slice_row0 / num_clb_rows_per_clock_row + 1;
    u8 slice_grp1 = slice_row1 / num_clb_rows_per_clock_row + 1;

    return (slice_col0 == slice_col1) && (slice_grp0 == slice_grp1);
}



void MiCAP_Custom_SetClbBitsInConfig(u32 *configuration, u32 word_offset, u32 frame_num, u32 frame_number_offset, const u8 Resource[][2], const u8 Value[], long NumBits) {
	u32 i;
	for(i = 0; i < NumBits; i++) {
		u32 frame_number = Resource[i][1];
		u32 frame_number_relative = frame_number - frame_number_offset;
		assert(frame_number >= frame_number_offset && frame_number_relative < frame_num);

		u8 bit_nr = Resource[i][0];
		u16 word_nr = frame_number_relative * 101 + word_offset;

		if(bit_nr >= 32) {
			bit_nr -= 32;
		}else {
			word_nr++;
		}

		assert(bit_nr < 32);
		u32 word = configuration[word_nr];
		u32 bit = 1 << bit_nr;
		if(Value[i])
			word |= bit;
		else
			word &= ~bit;
		configuration[word_nr] = word;
	}
}




/**
* Reads the frames of a clb.
*
* @return   XST_SUCCESS or XST_FAILURE.
**/
int MiCAP_Custom_ReadClbFrames(LUT_config_type  *lut_configs, u32 *config_buffer) {
    u32 num_lut_configs = 1;
    assert(num_lut_configs>0);
    u32 i;
    for(i = 0; i<num_lut_configs; i++) {
        u8 bottom_ntop;
        int Status;
        int clock_row;
        u32 major_frame_address;
		u32 word_offset;
        long slice_row = lut_configs[i].slice_row;
        long slice_col = lut_configs[i].slice_col;
 

     MiCAP_GetConfigurationCoordinates(slice_row, slice_col,
        &bottom_ntop, &clock_row, &major_frame_address, &word_offset);
        
        u32 frame_num = FRAME_NUM_CLB;
        u32 frame_number_offset;
        if(lut_configs[i].Resource[0][1] < 32)
            frame_number_offset = 26;
        else
            frame_number_offset = 32;
 
        u32 buffer[506];
 
 	u32 frame_address = XHwIcap_Custom_SetupFar7series(bottom_ntop, XHI_FAR_CLB_BLOCK, clock_row,  major_frame_address, frame_number_offset);

	Status = MiCAP_DeviceReadFrames(frame_address, frame_num, buffer);

        if (Status != XST_SUCCESS)
            return XST_FAILURE;
 
        u32 *configuration = buffer + 101 + 1;
 
        memcpy(config_buffer, configuration, 101 * frame_num * sizeof(u32));
 
        config_buffer += 101 * frame_num;
    }
    return XST_SUCCESS;
}
 
/**
* Sets bits contained in multiple LUTs specified by the coordinates and data in the lut_configs array.
* The current configuration is provided in a buffer so that it does not have to be read again from the FPGA.
*
* @return   XST_SUCCESS or XST_FAILURE.
**/
int MiCAP_Custom_SetMultiClbBitsWithFrames(LUT_config_type  *lut_configs, u32 num_lut_configs, u32 *config_buffer) {
    u8 bottom_ntop;
    int Status;
    int clock_row;
    u32 major_frame_address;
	u32 word_offset;
    assert(num_lut_configs>0);
 
    long slice_row = lut_configs[0].slice_row;
    long slice_col = lut_configs[0].slice_col;
 
    // Check if all the lutconfigs are indeed part of the same set of frames
    u32 i;
    for(i = 1; i<num_lut_configs; i++) {
        if(!MiCAP_Custom_IsSameFrame(slice_row, slice_col, lut_configs[i].slice_row, lut_configs[i].slice_col))
            return XST_FAILURE;
    }
 
     MiCAP_GetConfigurationCoordinates(slice_row, slice_col,
        &bottom_ntop, &clock_row, &major_frame_address, &word_offset);
 
    u32 frame_num = FRAME_NUM_CLB;
    u32 frame_number_offset;
    if(lut_configs[0].Resource[0][1] < 32)
        frame_number_offset = 26;
    else
        frame_number_offset = 32;
 
    u32 buffer[101 * (frame_num + 1) + 1];
    u32 *configuration = buffer + 101 + 1;
    memcpy(configuration, config_buffer, 101 * frame_num * sizeof(u32));
 
    for(i = 0; i< num_lut_configs; i++) {
        u32 word_offset = MiCAP_Custom_GetWordOffset(lut_configs[i].slice_row);
        MiCAP_Custom_SetClbBitsInConfig(configuration, word_offset,
                frame_num, frame_number_offset, lut_configs[i].Resource, lut_configs[i].Value, lut_configs[i].NumBits);
    }
	u32 frame_address = XHwIcap_Custom_SetupFar7series(bottom_ntop, XHI_FAR_CLB_BLOCK, clock_row,  major_frame_address, frame_number_offset);

	 Status = MiCAP_DeviceWriteFrames(frame_address, frame_num, buffer);
 
    if (Status != XST_SUCCESS)
        return XST_FAILURE;
 
    return XST_SUCCESS;
}





/**
* Sets bits contained in multiple LUTs specified by the coordinates and data in the lut_configs array.
*
* @return	XST_SUCCESS or XST_FAILURE.
**/
int MiCAP_Custom_SetMultiClbBits(LUT_config_type  *lut_configs, u32 num_lut_configs) {
	u8 bottom_ntop;
	int Status;
	int clock_row;
	u32 major_frame_address;
    u32 word_offset;

	assert(num_lut_configs>0);

	long slice_row = lut_configs[0].slice_row;
	long slice_col = lut_configs[0].slice_col;

	// Check if all the lutconfigs are indeed part of the same set of frames
	u32 i;
	for(i = 1; i<num_lut_configs; i++) {
		if(!MiCAP_Custom_IsSameFrame(slice_row, slice_col, lut_configs[i].slice_row, lut_configs[i].slice_col))
			return XST_FAILURE;
	}


    MiCAP_GetConfigurationCoordinates(slice_row, slice_col,
        &bottom_ntop, &clock_row, &major_frame_address, &word_offset);

	u32 frame_num = 4;
	u32 frame_number_offset;
	if(lut_configs[0].Resource[0][1] < 32)
		frame_number_offset = 26;
	else
		frame_number_offset = 32;

	u32 buffer[506];
	u32 frame_address = XHwIcap_Custom_SetupFar7series(bottom_ntop, XHI_FAR_CLB_BLOCK, clock_row,  major_frame_address, frame_number_offset);

	Status = MiCAP_DeviceReadFrames(frame_address, frame_num, buffer);

	if (Status != XST_SUCCESS)
		   return XST_FAILURE;

	u32 *configuration = buffer + 101 + 1;

	for(i = 0; i< num_lut_configs; i++) {
		u32 word_offset = MiCAP_Custom_GetWordOffset(lut_configs[i].slice_row);
	    MiCAP_Custom_SetClbBitsInConfig(configuration, word_offset,
	    		frame_num, frame_number_offset, lut_configs[i].Resource, lut_configs[i].Value, lut_configs[i].NumBits);
	}

	 Status = MiCAP_DeviceWriteFrames(frame_address, frame_num, buffer);

	 if (Status != XST_SUCCESS)
	       return XST_FAILURE;

	return XST_SUCCESS;
}


void MiCAP_GetConfigurationCoordinates(long slice_row, long slice_col,
        u8 *bottom_ntop_p, int *clock_row_p, u32 *major_frame_address_p, u32 *word_offset_p) {

	/* Zynq XC7Z020*/ /*The convention used here is specific to 7 series FPGA only!!*/
	 u16 XHI_XC7Z020_SKIP_COLS[] = {0-1+1, 1-2+1, 6-3+1, 9-4+1, 14-5+1, 17-6+1, 22-7+1, 25-8+1,
					33-9+1, 36-10+1, 50-11+1/*invisible column*/, 56-12+1, 59-13+1, 64-14+1, 67-15+1, 0xFFFF};

    u32 num_clock_rows = 3; // Specific to Zynq-SoC only.
    u32 num_clb_rows = 150; // Specific to Zynq-SoC only.
    u32 num_clb_rows_per_clock_row = num_clb_rows / num_clock_rows;
    u32 num_clb_cols = 57; // Specific to Zynq-SoC only.

    assert(slice_row >= 0 && slice_row < num_clb_rows);
    assert(slice_col >= 0 && slice_col < 2*num_clb_cols);

	/* Translation of Slice X-coordinate to CLB X-coordinate */
    u32 major_frame_address = slice_col/2;
    u32 i;
    for (i = 0; major_frame_address >= XHI_XC7Z020_SKIP_COLS[i] ; i++);
    major_frame_address += i;

	/* Top or Bottom bit and the row address*/
    u8 mid_line;
    u8 bottom_ntop;
    u8 slice_grp = slice_row / num_clb_rows_per_clock_row + 1;
    int clock_row = 0;

    if(num_clock_rows % 2 == 0)
    	mid_line = num_clock_rows / 2;
    else
    	mid_line = num_clock_rows / 2 + 1;

	if(slice_grp > mid_line){
		bottom_ntop = 0;
		clock_row = slice_grp - mid_line - 1;
	}
	else {
		clock_row = mid_line - slice_grp;
		bottom_ntop = 1;
	}

  	/* Calculating word offset */
    u32 word_offset = 0;
	slice_row = slice_row % num_clb_rows_per_clock_row;

	if (slice_row >= num_clb_rows_per_clock_row/2){
		word_offset = slice_row * 2 + 1;
	}
	else {
		word_offset = slice_row * 2;
	}

    *bottom_ntop_p = bottom_ntop;
    *clock_row_p = clock_row;
    *major_frame_address_p = major_frame_address;
    *word_offset_p = word_offset;
}


/*CTRL
 *Control Register Mapping
----------------------------------------
	Bit					Function
----------------------------------------
	0			ICAP read enable/disable
	1			MiCAP enable/disable
	2			ICAP write enable/disable
	3			Reset input buffer
	4			Wait state enable/disable
*/

int MiCAP_DeviceReadFrames(u32 frameAddress, u8 NumFrames, u32 *FrameBuffer)
{

	u32 frame_address;
	int i;
	u32 icap_rd_wr_done_mask = 0x4;
	u32 STAT_REG = 0x0; //This reg is filled by the micap
	u32 buff_read[507];
	u32 dummy;
	u32 CTRL_REG = 0x0;
	u32 micap_mask_en= 0x2; //CTRL[1] (0010)
	u32 micap_mask_disable= 0xFFFD; //CTRL[1] (0010)
	u32 micap_write_mask_en= 0x4; //CTRL[2] (0100)
	u32 micap_write_mask_disable= 0xFFFB; //CTRL[2] (0100)
	u32 micap_read_mask_en= 0x1; //CTRL[0] (0001)
	u32 micap_read_mask_disable= 0xFFFE; //CTRL[0] (0001)
	u32 micap_wait_en= 0x10; //CTRL[0] (010000

	u32 ibuff[] = { 0xFFFFFFFF,	0xAA995566, 0x20000000, 0x20000000, 0x30008001, 0x00000007,
					0x20000000, 0x20000000, 0x30008001, 0x00000004, 0x20000000, 0x20000000,
					0x20000000, 0x30002001, frameAddress, 0x28006000, 0x480001FA, 0x20000000,
					0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000,
					0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000,
					0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000,
					0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000,
					0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000, 0x20000000,
					0x20000000, 0x30008001, 0x0000000D, 0x20000000, 0x20000000};

	// Disable the MiCAP
	CTRL_REG = CTRL_REG & micap_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	//Wait state to fill the ibuff
	CTRL_REG = CTRL_REG | micap_wait_en;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	//set the input buffer with commands only!
	 for (i=0; i<53; i++){
		 Xil_Out32(XPAR_MICAP_0_BASEADDR + 0xC, ibuff[i]);
	 }

	// Write disable
	CTRL_REG = CTRL_REG & micap_write_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	// Read enable
	CTRL_REG = CTRL_REG | micap_read_mask_en;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	// MiCAP enable
	CTRL_REG = CTRL_REG | micap_mask_en;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	// Wait until ICAP reads the config data
    while (!(STAT_REG & icap_rd_wr_done_mask))
    	STAT_REG = Xil_In32(XPAR_MICAP_0_BASEADDR + 0x4);

    dummy = Xil_In32(XPAR_MICAP_0_BASEADDR + 0xC);

    for (i=0; i<505; i++){
        	*FrameBuffer = Xil_In32(XPAR_MICAP_0_BASEADDR + 0xC);
        	FrameBuffer++;
        }

    //Turn off read
	CTRL_REG = CTRL_REG & micap_read_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

    //Turn off the MiCAP
	CTRL_REG = CTRL_REG & micap_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

    return XST_SUCCESS;

}

int MiCAP_DeviceWriteFrames(u32 frameAddress, u8 NumFrames,u32 *FrameData)
{
	u32 frame_address;
	int i;
	int j = 0;
	int Status;
	u32 STAT_REG = 0x0; //This reg is filled by the micap
	u32 icap_rd_wr_done_mask = 0x4;

	u32 CTRL_REG = 0x0;
	u32 micap_mask_en= 0x2; //CTRL[1] (0010)
	u32 micap_mask_disable= 0xFFFD; //CTRL[1] (0010)
	u32 micap_write_mask_en= 0x4; //CTRL[2] (0100)
	u32 micap_write_mask_disable= 0xFFFB; //CTRL[2] (0100)
	//u32 micap_read_mask_en= 0x1; //CTRL[0] (0001)
	u32 micap_read_mask_disable= 0xFFFE; //CTRL[0] (0001)
	u32 micap_wait_en= 0x10; //CTRL[0] (010000)

	u32 *config_data = FrameData + 101;


		u32 head[] = {0xFFFFFFFF, 0xFFFFFFFF, 0x20000000, 0xAA995566, 0x20000000, 0x20000000,
							0x30008001, 0x00000007, 0x20000000, 0x20000000, 0x30018001, 0x03727093,
							0x30002001, frameAddress, 0x30008001, 0x00000001, 0x20000000, 0x300041F9};

		u32 tail[] =  {0x30008001, 0x00000007, 0x20000000, 0x20000000, 0x30002001, 0x00061080,
							0x30008001, 0x00000007, 0x20000000, 0x20000000, 0x30008001, 0x0000000D,
							0x20000000, 0x20000000};
		// Disable the MiCAP
		CTRL_REG = CTRL_REG & micap_mask_disable;
		Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

		//Wait state to fill the ibuff
		CTRL_REG = CTRL_REG | micap_wait_en;
		Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

		for(i=0;i<18;i++){
			 Xil_Out32(XPAR_MICAP_0_BASEADDR + 0xC, head[i]);
		}
	    for (i=0; i<404; i++){
			 config_data++;
			 Xil_Out32(XPAR_MICAP_0_BASEADDR + 0xC, *config_data);
		 }
		for (i=0; i<101; i++){
			 Xil_Out32(XPAR_MICAP_0_BASEADDR + 0xC, 0x0);
		 }
		for (i=0; i<14; i++){
			 Xil_Out32(XPAR_MICAP_0_BASEADDR + 0xC, tail[i]);
		 }

	CTRL_REG = CTRL_REG | micap_mask_en;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	//MiCAP write
	CTRL_REG = CTRL_REG | micap_write_mask_en;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);


	//icap_read_disable
	CTRL_REG = CTRL_REG & micap_read_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	// Wait until ICAP writes the config data
	while (!(STAT_REG & icap_rd_wr_done_mask))
		STAT_REG = Xil_In32(XPAR_MICAP_0_BASEADDR + 0x4);

	//Disable the Write
	CTRL_REG = CTRL_REG & micap_write_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	//Turn off the MiCAP
	CTRL_REG = CTRL_REG & micap_mask_disable;
	Xil_Out32(XPAR_MICAP_0_BASEADDR, CTRL_REG);

	return XST_SUCCESS;
};


#endif

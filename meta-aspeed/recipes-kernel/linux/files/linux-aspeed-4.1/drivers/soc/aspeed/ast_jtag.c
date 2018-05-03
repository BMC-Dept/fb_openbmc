/********************************************************************************
* File Name     : driver/char/asped/ast_jtag.c
* Author         : Ryan Chen
* Description   : AST JTAG driver
*
* Copyright (c) 2017, Intel Corporation.
* Copyright (C) 2012-2020  ASPEED Technology Inc.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by the Free Software Foundation;
* either version 2 of the License, or (at your option) any later version.
* This program is distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <plat/ast-scu.h>
#include <plat/regs-scu.h>
#include <plat/regs-jtag.h>

#include <mach/hardware.h>
#include <mach/platform.h>

/*************************************************************************************/
typedef enum jtag_xfer_mode {
	HW_MODE = 0,
	SW_MODE
} xfer_mode;

struct runtest_idle {
	xfer_mode 	mode;		//0 :HW mode, 1: SW mode
	unsigned char 	reset;		//Test Logic Reset
	unsigned char 	end;			//o: idle, 1: ir pause, 2: drpause
	unsigned char 	tck;			//keep tck
};

struct sir_xfer {
	xfer_mode 	mode;		//0 :HW mode, 1: SW mode
	unsigned short length;	//bits
	unsigned int tdi;
	unsigned int tdo;
	unsigned char endir;	//0: idle, 1:pause
};

struct sdr_xfer {
	xfer_mode 	mode;		//0 :HW mode, 1: SW mode
	unsigned char 	direct; // 0 ; read , 1 : write
	unsigned short length;	//bits
	unsigned int *tdio;
	unsigned char enddr;	//0: idle, 1:pause
};

struct tck_bitbang {
    unsigned char     tms;
    unsigned char     tdi;        // TDI bit value to write
    unsigned char     tdo;        // TDO bit value to read
};

struct scan_xfer {
    xfer_mode        mode;        // Hardware or software mode
    unsigned int     tap_state;   // Current tap state
    unsigned int     length;      // number of bits to clock
    unsigned char    *tdi;        // data to write to tap (optional)
    unsigned int     tdi_bytes;
    unsigned char    *tdo;        // data to read from tap (optional)
    unsigned int     tdo_bytes;
    unsigned int     end_tap_state;
};

struct set_tck_param {
    xfer_mode        mode;        // Hardware or software mode
    unsigned int     tck;         // This can be treated differently on systems as needed. It can be a divisor or actual frequency as needed.
};

struct get_tck_param {
    xfer_mode        mode;        // Hardware or software mode
    unsigned int     tck;         // This can be treated differently on systems as needed. It can be a divisor or actual frequency as needed.
};

struct tap_state_param {
    xfer_mode        mode;        // Hardware or software mode
    unsigned int     from_state;
    unsigned int     to_state;
};

struct controller_mode_param {
    xfer_mode        mode;        // Hardware or software mode
    unsigned int     controller_mode;
};

#define JTAGIOC_BASE       'T'

#define AST_JTAG_IOCRUNTEST		_IOW(JTAGIOC_BASE, 0, struct runtest_idle)
#define AST_JTAG_IOCSIR			_IOWR(JTAGIOC_BASE, 1, struct sir_xfer)
#define AST_JTAG_IOCSDR			_IOWR(JTAGIOC_BASE, 2, struct sdr_xfer)
#define AST_JTAG_SIOCFREQ		_IOW(JTAGIOC_BASE, 3, unsigned int)
#define AST_JTAG_GIOCFREQ		_IOR(JTAGIOC_BASE, 4, unsigned int)
#define AST_JTAG_BITBANG        _IOWR(JTAGIOC_BASE, 5, struct tck_bitbang)
#define AST_JTAG_SET_TAPSTATE     _IOW( JTAGIOC_BASE, 6, struct tap_state_param)
#define AST_JTAG_READWRITESCAN    _IOWR(JTAGIOC_BASE, 7, struct scan_xfer)
#define AST_JTAG_SLAVECONTLR      _IOW( JTAGIOC_BASE, 8, struct controller_mode_param)

#define AST_JTAG_SET_TCK          _IOW( JTAGIOC_BASE, 9, struct set_tck_param)
#define AST_JTAG_GET_TCK          _IOR( JTAGIOC_BASE, 10, struct get_tck_param)

/******************************************************************************/
//#define AST_JTAG_DEBUG
/* Disable to optimize performance */
//#define USE_INTERRUPTS
#define WAIT_ITERATIONS 75
#ifdef AST_JTAG_DEBUG
#define JTAG_DBUG(fmt, ...) \
	printk(KERN_DEBUG "%s:%d " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define JTAG_DBUG(fmt, args...)
#endif

#define JTAG_ERR(fmt, ...) \
	printk(KERN_ERR "%s:%d " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define JTAG_MSG(fmt, ...) \
	printk(KERN_INFO fmt, ##__VA_ARGS__)

struct ast_jtag_info {
	void __iomem	    *reg_base;
	u8 			          sts;			//0: idle, 1:irpause 2:drpause
  u8                tapstate; // see enum JtagStates
	int 			        irq;				//JTAG IRQ number
	u32 			        flag;
	wait_queue_head_t jtag_wq;
	bool 			        is_open;
};

/*************************************************************************************/
static u32 ast_scu_base = IO_ADDRESS(AST_SCU_BASE);
static DEFINE_SPINLOCK(jtag_state_lock);

static u32 g_sw_tdi;
static u32 g_sw_tck;
static u32 g_sw_tms;

#define JTAG_SW_MODE_VAL_MASK \
	(JTAG_SW_MODE_TDIO|JTAG_SW_MODE_TCK|JTAG_SW_MODE_TMS)

typedef enum {
    JtagTLR,
    JtagRTI,
    JtagSelDR,
    JtagCapDR,
    JtagShfDR,
    JtagEx1DR,
    JtagPauDR,
    JtagEx2DR,
    JtagUpdDR,
    JtagSelIR,
    JtagCapIR,
    JtagShfIR,
    JtagEx1IR,
    JtagPauIR,
    JtagEx2IR,
    JtagUpdIR
} JtagStates;

typedef struct
// this structure represents a TMS cycle, as expressed in a set of bits and a count of bits (note: there are no start->end state transitions that require more than 1 byte of TMS cycles)
{
    unsigned char tmsbits;
    unsigned char count;
} TmsCycle;

// these are the string representations of the TAP states corresponding to the enums literals in JtagStateEncode
const char* const c_statestr[] = {"TLR", "RTI", "SelDR", "CapDR", "ShfDR", "Ex1DR", "PauDR", "Ex2DR", "UpdDR", "SelIR", "CapIR", "ShfIR", "Ex1IR", "PauIR", "Ex2IR", "UpdIR"};

static struct ast_jtag_info *ast_jtag = NULL;

// this is the complete set TMS cycles for going from any TAP state to any other TAP state, following a “shortest path” rule
const TmsCycle _tmsCycleLookup[][16] = {
/*   start*/ /*TLR      RTI      SelDR    CapDR    SDR      Ex1DR    PDR      Ex2DR    UpdDR    SelIR    CapIR    SIR      Ex1IR    PIR      Ex2IR    UpdIR    destination*/
/*     TLR*/{ {0x00,0},{0x00,1},{0x02,2},{0x02,3},{0x02,4},{0x0a,4},{0x0a,5},{0x2a,6},{0x1a,5},{0x06,3},{0x06,4},{0x06,5},{0x16,5},{0x16,6},{0x56,7},{0x36,6} },
/*     RTI*/{ {0x07,3},{0x00,0},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5} },
/*   SelDR*/{ {0x03,2},{0x03,3},{0x00,0},{0x00,1},{0x00,2},{0x02,2},{0x02,3},{0x0a,4},{0x06,3},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4} },
/*   CapDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x00,0},{0x00,1},{0x01,1},{0x01,2},{0x05,3},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*     SDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x00,0},{0x01,1},{0x01,2},{0x05,3},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*   Ex1DR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x02,3},{0x00,0},{0x00,1},{0x02,2},{0x01,1},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6} },
/*     PDR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x01,2},{0x05,3},{0x00,0},{0x01,1},{0x03,2},{0x0f,4},{0x0f,5},{0x0f,6},{0x2f,6},{0x2f,7},{0xaf,8},{0x6f,7} },
/*   Ex2DR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x00,1},{0x02,2},{0x02,3},{0x00,0},{0x01,1},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6} },
/*   UpdDR*/{ {0x07,3},{0x00,1},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x00,0},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5} },
/*   SelIR*/{ {0x01,1},{0x01,2},{0x05,3},{0x05,4},{0x05,5},{0x15,5},{0x15,6},{0x55,7},{0x35,6},{0x00,0},{0x00,1},{0x00,2},{0x02,2},{0x02,3},{0x0a,4},{0x06,3} },
/*   CapIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x00,0},{0x00,1},{0x01,1},{0x01,2},{0x05,3},{0x03,2} },
/*     SIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x0f,5},{0x00,0},{0x01,1},{0x01,2},{0x05,3},{0x03,2} },
/*   Ex1IR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5},{0x07,3},{0x07,4},{0x02,3},{0x00,0},{0x00,1},{0x02,2},{0x01,1} },
/*     PIR*/{ {0x1f,5},{0x03,3},{0x07,3},{0x07,4},{0x07,5},{0x17,5},{0x17,6},{0x57,7},{0x37,6},{0x0f,4},{0x0f,5},{0x01,2},{0x05,3},{0x00,0},{0x01,1},{0x03,2} },
/*   Ex2IR*/{ {0x0f,4},{0x01,2},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x1b,5},{0x07,3},{0x07,4},{0x00,1},{0x02,2},{0x02,3},{0x00,0},{0x01,1} },
/*   UpdIR*/{ {0x07,3},{0x00,1},{0x01,1},{0x01,2},{0x01,3},{0x05,3},{0x05,4},{0x15,5},{0x0d,4},{0x03,2},{0x03,3},{0x03,4},{0x0b,4},{0x0b,5},{0x2b,6},{0x00,0} },
};

/******************************************************************************/
static inline u32
ast_jtag_read(struct ast_jtag_info *ast_jtag, u32 reg)
{
#if 0
	u32 val;
	val = readl(ast_jtag->reg_base + reg);
	JTAG_DBUG("reg = 0x%08x, val = 0x%08x\n", reg, val);
	return val;
#else
	return readl(ast_jtag->reg_base + reg);
#endif
}

static inline void
ast_jtag_write(struct ast_jtag_info *ast_jtag, u32 val, u32 reg)
{
	JTAG_DBUG("reg = 0x%08x, val = 0x%08x\n", reg, val);
	writel(val, ast_jtag->reg_base + reg);
}

/******************************************************************************/
void ast_jtag_set_tck(struct ast_jtag_info *ast_jtag, xfer_mode mode, unsigned int tck) {
    if (tck == 0)
        tck = 1;
    else if (tck > JTAG_TCK_DIVISOR_MASK)
        tck = JTAG_TCK_DIVISOR_MASK;
    ast_jtag_write(ast_jtag, ((ast_jtag_read(ast_jtag, AST_JTAG_TCK) & ~JTAG_TCK_DIVISOR_MASK) | tck),  AST_JTAG_TCK);
}

void ast_jtag_get_tck(struct ast_jtag_info *ast_jtag, xfer_mode mode, unsigned int *tck) {
    *tck = JTAG_GET_TCK_DIVISOR(ast_jtag_read(ast_jtag, AST_JTAG_TCK));
}

void ast_jtag_set_freq(struct ast_jtag_info *ast_jtag, unsigned int freq)
{
	u16 i;
	for(i = 0; i < 0x7ff; i++) {
//		printk("[%d] : freq : %d , target : %d \n", i, ast_get_pclk()/(i + 1), freq);
		if((ast_get_pclk()/(i + 1) ) <= freq)
			break;
	}
//	printk("div = %x \n", i);
	ast_jtag_write(ast_jtag, ((ast_jtag_read(ast_jtag, AST_JTAG_TCK) & ~JTAG_TCK_DIVISOR_MASK) | i),  AST_JTAG_TCK);

}

unsigned int ast_jtag_get_freq(struct ast_jtag_info *ast_jtag)
{
	return ast_get_pclk() / (JTAG_GET_TCK_DIVISOR(ast_jtag_read(ast_jtag, AST_JTAG_TCK)) + 1);
}
/******************************************************************************/
// Used only in SW mode to walk the JTAG state machine.
static u8 TCK_Cycle(struct ast_jtag_info *ast_jtag, u8 TMS, u8 TDI) {
    u32 regwriteval = JTAG_SW_MODE_EN | (TMS * JTAG_SW_MODE_TMS) | (TDI * JTAG_SW_MODE_TDIO);

    // TCK = 0
    ast_jtag_write(ast_jtag, regwriteval, AST_JTAG_SW);

    ast_jtag_read(ast_jtag, AST_JTAG_SW);

    // TCK = 1
    ast_jtag_write(ast_jtag, JTAG_SW_MODE_TCK | regwriteval, AST_JTAG_SW);

    return (ast_jtag_read(ast_jtag, AST_JTAG_SW) & JTAG_SW_MODE_TDIO) ? 1 : 0;
}

static void ast_jtag_wait_instruction(struct ast_jtag_info *ast_jtag, u32 flag)
{
#ifdef USE_INTERRUPTS
	wait_event_interruptible(ast_jtag->jtag_wq, (ast_jtag->flag == flag));
	ast_jtag->flag = 0;
#else
	u32 status = 0;
	u32 iterations = 0;
	while ((status & flag) == 0) {
		status = ast_jtag_read(ast_jtag, AST_JTAG_ISR);
		JTAG_DBUG("wait_instruction(%d) = 0x%08x\n", flag, status);
		iterations++;
		if (iterations > WAIT_ITERATIONS) {
			JTAG_ERR("ast_jtag driver timed out waiting for %d\n", flag);
			return;
		}
		if ((status & JTAG_DATA_COMPLETE) == 0) {
			if(iterations % 25 == 0)
				usleep_range(1 , 5);
			else
				udelay(1);
		}
	}
	usleep_range(1, 5);
	/* Clear the flag */
	ast_jtag_write(ast_jtag, flag | (status & 0xf), AST_JTAG_ISR);
#endif
}

void ast_jtag_wait_instruction_pause_complete(struct ast_jtag_info *ast_jtag)
{
	ast_jtag_wait_instruction(ast_jtag, JTAG_INST_PAUSE);
}

void ast_jtag_wait_instruction_complete(struct ast_jtag_info *ast_jtag)
{
	ast_jtag_wait_instruction(ast_jtag, JTAG_INST_COMPLETE);
}

void ast_jtag_wait_data_pause_complete(struct ast_jtag_info *ast_jtag)
{
	ast_jtag_wait_instruction(ast_jtag, JTAG_DATA_PAUSE);
}

void ast_jtag_wait_data_complete(struct ast_jtag_info *ast_jtag)
{
	ast_jtag_wait_instruction(ast_jtag, JTAG_DATA_COMPLETE);
}

/*************************************************************************************/
void ast_jtag_bitbang(struct ast_jtag_info *ast_jtag, struct tck_bitbang *bitbang)
{
    bitbang->tdo = TCK_Cycle(ast_jtag, bitbang->tms, bitbang->tdi);
}

void reset_tap(struct ast_jtag_info *ast_jtag, xfer_mode mode) {
    unsigned char i;
    if (mode == SW_MODE) {
        // clear tap state and go back to TLR
        g_sw_tms = JTAG_SW_MODE_TMS;
        g_sw_tdi = 0;
        for (i = 0; i < 9; i++) {
	        TCK_Cycle(ast_jtag, (g_sw_tms) ? 1 : 0, (g_sw_tdi) ? 1 : 0);
        }
        g_sw_tck = JTAG_SW_MODE_TCK;
    } else {
        ast_jtag_write(ast_jtag, 0, AST_JTAG_SW);  // disable sw mode
        mdelay(1);
        ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_FORCE_TMS, AST_JTAG_CTRL);
        mdelay(1);
        ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);
    }
}

static int ast_jtag_set_tapstate(struct ast_jtag_info *ast_jtag, xfer_mode mode,
                                 unsigned int from_state, unsigned int to_state) {
    unsigned char i;
    unsigned char tmsbits;
    unsigned char count;

    // ensure that the requested and current tap states are within 0 to 15.
    if ((from_state >= sizeof(_tmsCycleLookup[0]) / sizeof(_tmsCycleLookup[0][0])) ||  // Column
        (to_state >= sizeof(_tmsCycleLookup) / sizeof _tmsCycleLookup[0])) {  // row
        return -1;
    }

    JTAG_DBUG("Set TAP state: %s\n", c_statestr[to_state]);

    if (mode == SW_MODE) {
        ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);  // Eanble Clock

        if (to_state == JtagTLR) {
            reset_tap(ast_jtag, mode);
        } else {
            tmsbits = _tmsCycleLookup[from_state][to_state].tmsbits;
            count = _tmsCycleLookup[from_state][to_state].count;

            if (count == 0) return 0;

            for (i = 0; i < count; i++) {
                TCK_Cycle(ast_jtag, (tmsbits & 1), 0);
                tmsbits >>= 1;
            }
        }
    } else if (to_state == JtagTLR) {  // HW Mode and TLR
        reset_tap(ast_jtag, mode);
    }

    return 0;
}

/******************************************************************************/
void ast_jtag_readwrite_scan(struct ast_jtag_info *ast_jtag, struct scan_xfer *scan_xfer) {
    unsigned int chunk_len = 0;
    unsigned int bit_index = 0;
    unsigned char* tdi_p = scan_xfer->tdi;
    unsigned char* tdo_p = scan_xfer->tdo;
    u32* hw_tdi_p = (u32*) scan_xfer->tdi;
    u32* hw_tdo_p = (u32*) scan_xfer->tdo;
    unsigned int length = 0;
    u32 bits_to_send=0;
    u32 bits_received=0;
    int scan_end = 0;
    bool is_IR = (scan_xfer->tap_state == JtagShfIR);

    if ((scan_xfer->tap_state != JtagShfDR) && (scan_xfer->tap_state != JtagShfIR)) {
        if ((scan_xfer->tap_state >= 0) && (scan_xfer->tap_state < sizeof(c_statestr)/sizeof(c_statestr[0]))) {
            JTAG_ERR("readwrite_scan bad current tapstate = %s\n", c_statestr[scan_xfer->tap_state]);
        }
        return;
    }
    if (scan_xfer->length == 0) {
        printk(KERN_ERR "readwrite_scan bad length 0\n");
        return;
    } else {
        length = scan_xfer->length;
    }

    if (scan_xfer->tdi == NULL && scan_xfer->tdi_bytes != 0) {
        printk(KERN_ERR "readwrite_scan null tdi with nonzero length %u!\n", scan_xfer->tdi_bytes);
        return;
    }

    if (scan_xfer->tdo == NULL && scan_xfer->tdo_bytes != 0) {
        printk(KERN_ERR "readwrite_scan null tdo with nonzero length %u!\n", scan_xfer->tdo_bytes);
        return;
    }

    JTAG_DBUG("In %s SHIFT %s, length = %d, scan_end = %d\n",
              scan_xfer->mode == SW_MODE ? "SW" : "HW", is_IR ? "IR" : "DR", length, scan_end);

    if(scan_xfer->mode == SW_MODE) {
        ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);  // Eanble Clock
        while (bit_index < scan_xfer->length) {
            int bit_offset = (bit_index % 8);
            int this_input_bit = 0;
            int tms_high_or_low;
            int this_output_bit;
            if (bit_index / 8 < scan_xfer->tdi_bytes) {
                // If we are on a byte boundary, increment the byte pointers
                // Don't increment on 0, pointer is already on the first byte
                if (bit_index % 8 == 0 && bit_index != 0) {
                    tdi_p++;
                }
                this_input_bit = (*tdi_p >> bit_offset) & 1;
            }
            // If this is the last bit, leave TMS high
            tms_high_or_low = (bit_index == scan_xfer->length - 1) && (scan_xfer->end_tap_state != JtagShfDR) &&
                              (scan_xfer->end_tap_state != JtagShfIR);

            this_output_bit = TCK_Cycle(ast_jtag, tms_high_or_low, this_input_bit);
            // If it was the last bit in the scan and the end_tap_state is something other than shiftDR or shiftIR then go to Exit1.
            // IMPORTANT Note: if the end_tap_state is ShiftIR/DR and the next call to this function is a shiftDR/IR then the driver will not change state!
            if (tms_high_or_low) {
                scan_xfer->tap_state = (scan_xfer->tap_state == JtagShfDR) ? JtagEx1DR : JtagEx1IR;
            }
            if (bit_index / 8 < scan_xfer->tdo_bytes) {
                if (bit_index % 8 == 0) {
                    if (bit_index != 0) {
                        tdo_p++;
                    }
                    // Zero the output buffer before we write data to it
                    *tdo_p = 0;
                }
                *tdo_p |= this_output_bit << bit_offset;
            }
            bit_index++;
        }
        ast_jtag_set_tapstate(ast_jtag, scan_xfer->mode, scan_xfer->tap_state, scan_xfer->end_tap_state);
    } else {  // HW_MODE
        ast_jtag_write(ast_jtag, 0, AST_JTAG_SW);  // disable sw mode

        if (scan_xfer->end_tap_state == JtagPauDR || scan_xfer->end_tap_state == JtagPauIR ||
            scan_xfer->end_tap_state == JtagShfDR || scan_xfer->end_tap_state == JtagShfIR) {
            scan_end = 0;
        } else {
            scan_end = 1;
        }

        while (length > 0) {
            int is_last;

            if (length > 32) {
                chunk_len = 32;
                is_last = 0;
            } else {
                chunk_len = length;
                if (scan_end == 1) {
                    is_last = is_IR ? JTAG_LAST_INST : JTAG_LAST_DATA;
                } else {
                    is_last = 0;
                }
            }
            JTAG_DBUG("In SHIFT %s, length = %d, scan_end = %d, chunk_len=%d, is_last=%d\n", is_IR ? "IR" : "DR", length, scan_end, chunk_len, is_last);

            if (hw_tdi_p && ((scan_xfer->length - length)/8 < scan_xfer->tdi_bytes)) {
                bits_to_send = *hw_tdi_p++;
                ast_jtag_write(ast_jtag, bits_to_send, is_IR ? AST_JTAG_INST : AST_JTAG_DATA);
            } else {
                bits_to_send = 0;
                ast_jtag_write(ast_jtag, 0, is_IR ? AST_JTAG_INST : AST_JTAG_DATA);
            }
            JTAG_DBUG("In SHIFT %s, len: %d chunk_len: %d is_last: %x bits_to_send: %x\n", is_IR ? "IR" : "DR",
                   length, chunk_len, is_last, bits_to_send);

            if (is_last && scan_end) {
                ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN
                                         | is_last
                                         | (is_IR ? JTAG_SET_INST_LEN(chunk_len) : JTAG_DATA_LEN(chunk_len)),
                               AST_JTAG_CTRL);
                ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN
                                         | is_last
                                         | (is_IR ? JTAG_SET_INST_LEN(chunk_len) : JTAG_DATA_LEN(chunk_len))
                                         | (is_IR ? JTAG_INST_EN : JTAG_DATA_EN),
                               AST_JTAG_CTRL);
                if (is_IR)
                    ast_jtag_wait_instruction_complete(ast_jtag);
                else
                    ast_jtag_wait_data_complete(ast_jtag);
            } else {
                ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN
                                         | (is_IR ? JTAG_IR_UPDATE : JTAG_DR_UPDATE)
                                         | (is_IR ? JTAG_SET_INST_LEN(chunk_len) : JTAG_DATA_LEN(chunk_len)),
                               AST_JTAG_CTRL);
                ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN
                                         | (is_IR ? JTAG_IR_UPDATE : JTAG_DR_UPDATE)
                                         | (is_IR ? JTAG_SET_INST_LEN(chunk_len) : JTAG_DATA_LEN(chunk_len))
                                         | (is_IR ? JTAG_INST_EN : JTAG_DATA_EN),
                               AST_JTAG_CTRL);
                if (is_IR)
                    ast_jtag_wait_instruction_pause_complete(ast_jtag);
                else
                    ast_jtag_wait_data_pause_complete(ast_jtag);
            }

            if (hw_tdo_p) {
                bits_received = ast_jtag_read(ast_jtag, is_IR ? AST_JTAG_INST : AST_JTAG_DATA);
                bits_received >>= (32 - chunk_len);
                *hw_tdo_p++ = bits_received;
            }
            JTAG_DBUG("In SHIFT %s, len: %d chunk_len: %d is_last %x  bits_received %x\n", is_IR ? "IR" : "DR",
                      length, chunk_len, is_last, bits_received);
            length -= chunk_len;
        }
    }
}

/******************************************************************************/
/* JTAG_reset() is to generate at least 9 TMS high and
 * 1 TMS low to force devices into Run-Test/Idle State
 */
void ast_jtag_run_test_idle(struct ast_jtag_info *ast_jtag, struct runtest_idle *runtest)
{
	int i = 0;

	JTAG_DBUG(":%s mode\n", runtest->mode? "SW":"HW");

	if(runtest->mode) {
		//SW mode
		//from idle , from pause,  -- > to pause, to idle

		if(runtest->reset) {
			for(i=0;i<10;i++) {
				TCK_Cycle(ast_jtag, 1, 0);
			}
		}

		switch(ast_jtag->sts) {
			case 0:
				if(runtest->end == 1) {
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRSCan
					TCK_Cycle(ast_jtag, 1, 0);	 // go to IRSCan
					TCK_Cycle(ast_jtag, 0, 0);	 // go to IRCap
					TCK_Cycle(ast_jtag, 1, 0);	 // go to IRExit1
					TCK_Cycle(ast_jtag, 0, 0);	 // go to IRPause
					ast_jtag->sts = 1;
				} else if (runtest->end == 2) {
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRSCan
					TCK_Cycle(ast_jtag, 0, 0);	 // go to DRCap
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRExit1
					TCK_Cycle(ast_jtag, 0, 0);	 // go to DRPause
					ast_jtag->sts = 1;
				} else {
					TCK_Cycle(ast_jtag, 0, 0);	// go to IDLE
					ast_jtag->sts = 0;
				}
				break;
			case 1:
				//from IR/DR Pause
				if(runtest->end == 1) {
					TCK_Cycle(ast_jtag, 1, 0);	// go to Exit2 IR / DR
					TCK_Cycle(ast_jtag, 1, 0);	// go to Update IR /DR
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRSCan
					TCK_Cycle(ast_jtag, 1, 0);	 // go to IRSCan
					TCK_Cycle(ast_jtag, 0, 0);	 // go to IRCap
					TCK_Cycle(ast_jtag, 1, 0);	 // go to IRExit1
					TCK_Cycle(ast_jtag, 0, 0);	 // go to IRPause
					ast_jtag->sts = 1;
				} else if (runtest->end == 2) {
					TCK_Cycle(ast_jtag, 1, 0);	// go to Exit2 IR / DR
					TCK_Cycle(ast_jtag, 1, 0);	// go to Update IR /DR
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRSCan
					TCK_Cycle(ast_jtag, 0, 0);	 // go to DRCap
					TCK_Cycle(ast_jtag, 1, 0);	 // go to DRExit1
					TCK_Cycle(ast_jtag, 0, 0);	 // go to DRPause
					ast_jtag->sts = 1;
				} else {
					TCK_Cycle(ast_jtag, 1, 0);		// go to Exit2 IR / DR
					TCK_Cycle(ast_jtag, 1, 0);		// go to Update IR /DR
					TCK_Cycle(ast_jtag, 0, 0);	// go to IDLE
					ast_jtag->sts = 0;
				}
				break;
			default:
				printk("TODO check ERROR \n");
				break;
		}

		for(i=0; i<runtest->tck; i++)
			TCK_Cycle(ast_jtag, 0, 0);	// stay on IDLE for at lease  TCK cycle

	} else {
		ast_jtag_write(ast_jtag, 0 ,AST_JTAG_SW); //dis sw mode
		mdelay(1);
		if(runtest->reset)
			ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_FORCE_TMS ,AST_JTAG_CTRL);	// x TMS high + 1 TMS low
		else
			ast_jtag_write(ast_jtag, JTAG_GO_IDLE ,AST_JTAG_IDLE);
		mdelay(1);
		ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);
		ast_jtag->sts = 0;
	}
}



int ast_jtag_sir_xfer(struct ast_jtag_info *ast_jtag, struct sir_xfer *sir)
{
	int i = 0;
	JTAG_DBUG("%s mode, ENDIR : %d, len : %d \n", sir->mode? "SW":"HW", sir->endir, sir->length);

	if(sir->mode) {
		if(ast_jtag->sts) {
			//from IR/DR Pause
			TCK_Cycle(ast_jtag, 1, 0);		// go to Exit2 IR / DR
			TCK_Cycle(ast_jtag, 1, 0);		// go to Update IR /DR
		}

		TCK_Cycle(ast_jtag, 1, 0);		// go to DRSCan
		TCK_Cycle(ast_jtag, 1, 0);		// go to IRSCan
		TCK_Cycle(ast_jtag, 0, 0);		// go to CapIR
		TCK_Cycle(ast_jtag, 0, 0);		// go to ShiftIR

		sir->tdo = 0;
		for(i=0; i<sir->length; i++)
		{
			if(i == (sir->length - 1)) {
				sir->tdo |= TCK_Cycle(ast_jtag, 1, sir->tdi & 0x1);	// go to IRExit1
			} else {
				sir->tdo |= TCK_Cycle(ast_jtag, 0, sir->tdi & 0x1);	// go to ShiftIR
				sir->tdi >>= 1;
				sir->tdo <<= 1;
			}
		}

		TCK_Cycle(ast_jtag, 0, 0);		// go to IRPause

		//stop pause
		if(sir->endir == 0) {
			//go to idle
			TCK_Cycle(ast_jtag, 1, 0);		// go to IRExit2
			TCK_Cycle(ast_jtag, 1, 0);		// go to IRUpdate
			TCK_Cycle(ast_jtag, 0, 0);		// go to IDLE
		}
	}else {
		//HW MODE
#ifndef AST_SOC_G5
		//ast2300 , ast2400 not support end pause
		if(sir->endir)
			return 1;
#endif
		ast_jtag_write(ast_jtag, 0 , AST_JTAG_SW); //dis sw mode
		ast_jtag_write(ast_jtag, sir->tdi, AST_JTAG_INST);

		if(sir->endir) {
			ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_SET_INST_LEN(sir->length), AST_JTAG_CTRL);
			ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_SET_INST_LEN(sir->length) | JTAG_INST_EN, AST_JTAG_CTRL);
			ast_jtag_wait_instruction_pause_complete(ast_jtag);
		} else {
			ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_LAST_INST | JTAG_SET_INST_LEN(sir->length), AST_JTAG_CTRL);
			ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_LAST_INST | JTAG_SET_INST_LEN(sir->length) | JTAG_INST_EN, AST_JTAG_CTRL);
			ast_jtag_wait_instruction_complete(ast_jtag);
		}

		sir->tdo = ast_jtag_read(ast_jtag, AST_JTAG_INST);

		if(sir->endir == 0) {
			ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);
		}
	}
	ast_jtag->sts = sir->endir;
	return 0;
}

int ast_jtag_sdr_xfer(struct ast_jtag_info *ast_jtag, struct sdr_xfer *sdr)
{
	unsigned int index = 0;
	u32 shift_bits =0;
	u32 tdo = 0;
	u32 remain_xfer = sdr->length;

	JTAG_DBUG("%s mode, len : %d \n", sdr->mode? "SW":"HW", sdr->length);

	if(sdr->mode) {
		//SW mode
		if(ast_jtag->sts) {
			//from IR/DR Pause
			TCK_Cycle(ast_jtag, 1, 0);		// go to Exit2 IR / DR
			TCK_Cycle(ast_jtag, 1, 0);		// go to Update IR /DR
		}

		TCK_Cycle(ast_jtag, 1, 0);		// go to DRScan
		TCK_Cycle(ast_jtag, 0, 0);		// go to DRCap
		TCK_Cycle(ast_jtag, 0, 0);		// go to DRShift

		if(!sdr->direct)
			sdr->tdio[index] = 0;
		while (remain_xfer) {
			if(sdr->direct) {
				//write
				if((shift_bits % 32) == 0)
					JTAG_DBUG("W dr->dr_data[%d]: %x\n",index, sdr->tdio[index]);

				tdo = (sdr->tdio[index] >> (shift_bits % 32)) & (0x1);
				JTAG_DBUG("%d ",tdo);
				if(remain_xfer == 1) {
					TCK_Cycle(ast_jtag, 1, tdo);	// go to DRExit1
				} else {
					TCK_Cycle(ast_jtag, 0, tdo);	// go to DRShit
				}
			} else {
				//read
				if(remain_xfer == 1) {
					tdo = TCK_Cycle(ast_jtag, 1, tdo);	// go to DRExit1
				} else {
					tdo = TCK_Cycle(ast_jtag, 0, tdo);	// go to DRShit
				}
				JTAG_DBUG("%d ",tdo);
				sdr->tdio[index] |= (tdo << (shift_bits % 32));

				if((shift_bits % 32) == 0)
					JTAG_DBUG("R dr->dr_data[%d]: %x\n",index, sdr->tdio[index]);
			}
			shift_bits++;
			remain_xfer--;
			if((shift_bits % 32) == 0) {
				index ++;
				sdr->tdio[index] = 0;
			}

		}

		TCK_Cycle(ast_jtag, 0, 0);		// go to DRPause

		if(sdr->enddr == 0) {
			TCK_Cycle(ast_jtag, 1, 0);		// go to DRExit2
			TCK_Cycle(ast_jtag, 1, 0);		// go to DRUpdate
			TCK_Cycle(ast_jtag, 0, 0);		// go to IDLE
		}
	} else {
		//HW MODE
#ifndef AST_SOC_G5
		//ast2300 , ast2400 not support end pause
		if(sdr->enddr)
			return 1;
#endif
		ast_jtag_write(ast_jtag, 0, AST_JTAG_SW);
		while (remain_xfer) {
			if(sdr->direct) {
				JTAG_DBUG("W dr->dr_data[%d]: %x\n",index, sdr->tdio[index]);
				ast_jtag_write(ast_jtag, sdr->tdio[index], AST_JTAG_DATA);
			} else {
				ast_jtag_write(ast_jtag, 0, AST_JTAG_DATA);
			}

			if (remain_xfer > 32) {
				shift_bits = 32;
				// read bytes were not equals to column length ==> Pause-DR
				JTAG_DBUG("shit bits %d \n", shift_bits);
				ast_jtag_write(ast_jtag,
					JTAG_ENG_EN | JTAG_ENG_OUT_EN |
					JTAG_DATA_LEN(shift_bits), AST_JTAG_CTRL);
				ast_jtag_write(ast_jtag,
					JTAG_ENG_EN | JTAG_ENG_OUT_EN |
					JTAG_DATA_LEN(shift_bits) | JTAG_DATA_EN, AST_JTAG_CTRL);
				ast_jtag_wait_data_pause_complete(ast_jtag);
			} else {
				// read bytes equals to column length => Update-DR
				shift_bits = remain_xfer;
				JTAG_DBUG("shit bits %d with last \n", shift_bits);
				if(sdr->enddr) {
					JTAG_DBUG("DR Keep Pause \n");
					ast_jtag_write(ast_jtag,
						JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_DR_UPDATE |
						JTAG_DATA_LEN(shift_bits),AST_JTAG_CTRL);
					ast_jtag_write(ast_jtag,
						JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_DR_UPDATE |
						JTAG_DATA_LEN(shift_bits) | JTAG_DATA_EN, AST_JTAG_CTRL);
					ast_jtag_wait_data_pause_complete(ast_jtag);
				} else {
					JTAG_DBUG("DR go IDLE \n");
					ast_jtag_write(ast_jtag,
						JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_LAST_DATA |
						JTAG_DATA_LEN(shift_bits),AST_JTAG_CTRL);
					ast_jtag_write(ast_jtag,
						JTAG_ENG_EN | JTAG_ENG_OUT_EN | JTAG_LAST_DATA |
						JTAG_DATA_LEN(shift_bits) | JTAG_DATA_EN, AST_JTAG_CTRL);
					ast_jtag_wait_data_complete(ast_jtag);
				}
			}

			if(!sdr->direct) {
				//TODO check ....
				if(shift_bits < 32)
					sdr->tdio[index] = ast_jtag_read(ast_jtag, AST_JTAG_DATA)>> (32 - shift_bits);
				else
					sdr->tdio[index] = ast_jtag_read(ast_jtag, AST_JTAG_DATA);
				JTAG_DBUG("R dr->dr_data[%d]: %x\n", index, sdr->tdio[index]);
			}

			remain_xfer = remain_xfer - shift_bits;
			index ++;
			JTAG_DBUG("remain_xfer %d\n", remain_xfer);
		}
		ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO,AST_JTAG_SW);
	}

	ast_jtag->sts = sdr->enddr;
	return 0;
}

/*************************************************************************************/
#ifdef USE_INTERRUPTS
static irqreturn_t ast_jtag_interrupt (int this_irq, void *dev_id) {
    u32 status;
    struct ast_jtag_info *ast_jtag = dev_id;

    status = ast_jtag_read(ast_jtag, AST_JTAG_ISR);

    if (status & JTAG_INST_PAUSE) {
        ast_jtag_write(ast_jtag, JTAG_INST_PAUSE | (status & 0xf), AST_JTAG_ISR);
        ast_jtag->flag = JTAG_INST_PAUSE;
    }

    if (status & JTAG_INST_COMPLETE) {
        ast_jtag_write(ast_jtag, JTAG_INST_COMPLETE | (status & 0xf), AST_JTAG_ISR);
        ast_jtag->flag = JTAG_INST_COMPLETE;
    }

    if (status & JTAG_DATA_PAUSE) {
        ast_jtag_write(ast_jtag, JTAG_DATA_PAUSE | (status & 0xf), AST_JTAG_ISR);
        ast_jtag->flag = JTAG_DATA_PAUSE;
    }

    if (status & JTAG_DATA_COMPLETE) {
        ast_jtag_write(ast_jtag, JTAG_DATA_COMPLETE | (status & 0xf),AST_JTAG_ISR);
        ast_jtag->flag = JTAG_DATA_COMPLETE;
    }

    if (ast_jtag->flag) {
        wake_up_interruptible(&ast_jtag->jtag_wq);
        return IRQ_HANDLED;
    }
    else {
        return IRQ_NONE;
    }

}
#endif

/*************************************************************************************/
static inline void ast_jtag_slave(void) {
    u32 currReg = readl((void *)(ast_scu_base + AST_SCU_RESET));
    // unlock scu
    writel(SCU_PROTECT_UNLOCK, (void *)ast_scu_base);
    writel(currReg | SCU_RESET_JTAG, (void *)(ast_scu_base + AST_SCU_RESET));
    // lock scu
    writel(0xaa,(void *)ast_scu_base);
}

static inline void ast_jtag_master(xfer_mode mode) {
    ast_scu_init_jtag();
    ast_jtag_write(ast_jtag, JTAG_ENG_EN | JTAG_ENG_OUT_EN, AST_JTAG_CTRL);  // Eanble Clock
    ast_jtag_write(ast_jtag, JTAG_SW_MODE_EN | JTAG_SW_MODE_TDIO, AST_JTAG_SW);  // Enable SW Mode
    ast_jtag_write(ast_jtag, JTAG_INST_PAUSE | JTAG_INST_COMPLETE |
        JTAG_DATA_PAUSE | JTAG_DATA_COMPLETE |
        JTAG_INST_PAUSE_EN | JTAG_INST_COMPLETE_EN |
        JTAG_DATA_PAUSE_EN | JTAG_DATA_COMPLETE_EN,
        AST_JTAG_ISR);        // Enable Interrupt

    // When leaving Slave mode, we do not know what state the TAP is in, so
    // reset the state to TLR, regardless of the previous unknown state.
    reset_tap(ast_jtag, mode);
}

static long jtag_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;
	struct ast_jtag_info *ast_jtag = file->private_data;
	void __user *argp = (void __user *)arg;
	struct sir_xfer sir;
	struct sdr_xfer sdr;
	struct runtest_idle run_idle;
  struct tck_bitbang bitbang;
  struct scan_xfer scan_xfer;
    struct set_tck_param set_tck_param;
    struct get_tck_param get_tck_param;
    struct tap_state_param tap_state_param;
    struct controller_mode_param controller_mode_param;

	switch (cmd) {
		case AST_JTAG_GIOCFREQ:
			 ret = __put_user(ast_jtag_get_freq(ast_jtag), (unsigned int __user *)arg);
			break;
		case AST_JTAG_SIOCFREQ:
			if((unsigned int)arg > ast_get_pclk())
				ret = -EFAULT;
			else
				ast_jtag_set_freq(ast_jtag, (unsigned int)arg);

			break;
        case AST_JTAG_SET_TCK:
            if (copy_from_user(&set_tck_param, argp, sizeof(struct set_tck_param)))
                ret = -EFAULT;
            else {
                ast_jtag_set_tck(ast_jtag, set_tck_param.mode, set_tck_param.tck);
            }
            break;
        case AST_JTAG_GET_TCK:
            if (copy_from_user(&get_tck_param, argp, sizeof(struct get_tck_param)))
                ret = -EFAULT;
            else
                ast_jtag_get_tck(ast_jtag, get_tck_param.mode, &get_tck_param.tck);
            if (copy_to_user(argp, &get_tck_param, sizeof(struct get_tck_param)))
                ret = -EFAULT;
            break;

		case AST_JTAG_IOCRUNTEST:
			if (copy_from_user(&run_idle, argp, sizeof(struct runtest_idle)))
				ret = -EFAULT;
			else
				ast_jtag_run_test_idle(ast_jtag, &run_idle);
			break;
		case AST_JTAG_IOCSIR:
			if (copy_from_user(&sir, argp, sizeof(struct sir_xfer)))
				ret = -EFAULT;
			else
				ast_jtag_sir_xfer(ast_jtag, &sir);

			if (copy_to_user(argp, &sir, sizeof(struct sdr_xfer)))
				ret = -EFAULT;
			break;
		case AST_JTAG_IOCSDR:
			if (copy_from_user(&sdr, argp, sizeof(struct sdr_xfer)))
				ret = -EFAULT;
			else
				ast_jtag_sdr_xfer(ast_jtag, &sdr);

			if (copy_to_user(argp, &sdr, sizeof(struct sdr_xfer)))
				ret = -EFAULT;
			break;
        case AST_JTAG_BITBANG:
            if (copy_from_user(&bitbang, argp, sizeof(struct tck_bitbang)))
                ret = -EFAULT;
            else
                ast_jtag_bitbang(ast_jtag, &bitbang);
            if (copy_to_user(argp, &bitbang, sizeof(struct tck_bitbang)))
                ret = -EFAULT;
            break;
        case AST_JTAG_SET_TAPSTATE:
            if (copy_from_user(&tap_state_param, argp, sizeof(struct tap_state_param)))
                ret = -EFAULT;
            else
                ast_jtag_set_tapstate(ast_jtag, tap_state_param.mode, tap_state_param.from_state, tap_state_param.to_state);
            break;
        case AST_JTAG_READWRITESCAN:
            if (copy_from_user(&scan_xfer, argp, sizeof(struct scan_xfer)))
                ret = -EFAULT;
            else
                ast_jtag_readwrite_scan(ast_jtag, &scan_xfer);

            // no need to copy to user if only doing a write scan
            if(scan_xfer.tdo != NULL) {
                if (copy_to_user(argp, &scan_xfer, sizeof(struct scan_xfer)))
                    ret = -EFAULT;
            }
            break;
        case AST_JTAG_SLAVECONTLR:
            if (copy_from_user(&controller_mode_param, argp, sizeof(struct controller_mode_param)))
                ret = -EFAULT;
            else {
                if (controller_mode_param.controller_mode)
                    ast_jtag_slave();
                else
                    ast_jtag_master(controller_mode_param.mode);
            }
            break;
		default:
			return -ENOTTY;
	}

	return ret;
}

static int jtag_open(struct inode *inode, struct file *file)
{
	spin_lock(&jtag_state_lock);
	if (ast_jtag->is_open) {
		spin_unlock(&jtag_state_lock);
		return -EBUSY;
	}

	ast_jtag->is_open = true;
	file->private_data = ast_jtag;

	spin_unlock(&jtag_state_lock);

	return 0;
}

static int jtag_release(struct inode *inode, struct file *file)
{
	struct ast_jtag_info *drvdata = file->private_data;

	spin_lock(&jtag_state_lock);

	drvdata->is_open = false;

	spin_unlock(&jtag_state_lock);

	return 0;
}

static ssize_t show_tdo(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
		       ast_jtag_read(ast_jtag, AST_JTAG_SW) & JTAG_SW_MODE_TDIO
		       ? "1" : "0");
}

static DEVICE_ATTR(tdo, S_IRUGO, show_tdo, NULL);

static inline void ast_jtag_write_sw_reg(void) {
	u32 old_val = ast_jtag_read(ast_jtag, AST_JTAG_SW);
	u32 val = (old_val & ~JTAG_SW_MODE_VAL_MASK) | (g_sw_tdi|g_sw_tck|g_sw_tms);
	JTAG_DBUG("write SW register from value %#x -> %#x. tms=%d tck=%d tdi=%d\n",
		  old_val, val,
		  g_sw_tms ? 1 : 0, g_sw_tck ? 1 : 0, g_sw_tdi ? 1 : 0);
	ast_jtag_write(ast_jtag, val, AST_JTAG_SW);
}

#define STORE_COMMON(buf, count, tdx, true_value) do {	\
	unsigned long val;				\
	int err;					\
	err = kstrtoul(buf, 0, &val);			\
	if (err)					\
		return err;				\
	tdx = val ? true_value : 0;			\
	ast_jtag_write_sw_reg();			\
	return count;					\
} while (0);

static ssize_t store_tdi(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	STORE_COMMON(buf, count, g_sw_tdi, JTAG_SW_MODE_TDIO);
}

static DEVICE_ATTR(tdi, S_IWUSR, NULL, store_tdi);

static ssize_t store_tms(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	STORE_COMMON(buf, count, g_sw_tms, JTAG_SW_MODE_TMS);
}

static DEVICE_ATTR(tms, S_IWUSR, NULL, store_tms);

static ssize_t store_tck(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	STORE_COMMON(buf, count, g_sw_tck, JTAG_SW_MODE_TCK);
}

static DEVICE_ATTR(tck, S_IWUSR, NULL, store_tck);

static ssize_t show_sts(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ast_jtag_info *ast_jtag = dev_get_drvdata(dev);
  if ((ast_jtag->tapstate >= 0) && (ast_jtag->tapstate < sizeof(c_statestr)/sizeof(c_statestr[0]))) {
    return sprintf(buf, "%s\n", c_statestr[ast_jtag->tapstate]);
  } else {
    return sprintf(buf, "ERROR\n");
  }
	// return sprintf(buf, "%s\n", ast_jtag->sts? "Pause":"Idle");
}

static DEVICE_ATTR(sts, S_IRUGO, show_sts, NULL);

static ssize_t show_frequency(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ast_jtag_info *ast_jtag = dev_get_drvdata(dev);
//	printk("PCLK = %d \n", ast_get_pclk());
//	printk("DIV  = %d \n", JTAG_GET_TCK_DIVISOR(ast_jtag_read(ast_jtag, AST_JTAG_TCK)) + 1);
	return sprintf(buf, "Frequency : %d\n", ast_get_pclk() / (JTAG_GET_TCK_DIVISOR(ast_jtag_read(ast_jtag, AST_JTAG_TCK)) + 1));
}

static ssize_t store_frequency(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int err;
	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;
	ast_jtag_set_freq(ast_jtag, val);

	return count;
}

static DEVICE_ATTR(freq, S_IRUGO | S_IWUSR, show_frequency, store_frequency);

static struct attribute *jtag_sysfs_entries[] = {
	&dev_attr_freq.attr,
	&dev_attr_sts.attr,
	&dev_attr_tck.attr,
	&dev_attr_tms.attr,
	&dev_attr_tdi.attr,
	&dev_attr_tdo.attr,
	NULL
};

static struct attribute_group jtag_attribute_group = {
	.attrs = jtag_sysfs_entries,
};

static const struct file_operations ast_jtag_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= jtag_ioctl,
	.open		= jtag_open,
	.release		= jtag_release,
};

struct miscdevice ast_jtag_misc = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "ast-jtag",
	.fops 	= &ast_jtag_fops,
};

static int ast_jtag_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret=0;

	JTAG_DBUG("ast_jtag_probe\n");

	if (!(ast_jtag = kzalloc(sizeof(struct ast_jtag_info), GFP_KERNEL))) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res) {
		dev_err(&pdev->dev, "cannot get IORESOURCE_MEM\n");
		ret = -ENOENT;
		goto out;
	}

	if (!request_mem_region(res->start, resource_size(res), res->name)) {
		dev_err(&pdev->dev, "cannot reserved region\n");
		ret = -ENXIO;
		goto out;
	}

	ast_jtag->reg_base = ioremap(res->start, resource_size(res));
	if (!ast_jtag->reg_base) {
		ret = -EIO;
		goto out_region;
	}

#ifdef USE_INTERRUPTS
	ast_jtag->irq = platform_get_irq(pdev, 0);
	if (ast_jtag->irq < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		ret = -ENOENT;
		goto out_region;
	}

	ret = request_irq(ast_jtag->irq, ast_jtag_interrupt, 0, "ast-jtag", ast_jtag);
	if (ret) {
		printk("JTAG Unable to get IRQ");
		goto out_region;
	}
#endif

	ast_jtag_master(SW_MODE);

	ast_jtag->flag = 0;
	init_waitqueue_head(&ast_jtag->jtag_wq);

	ret = misc_register(&ast_jtag_misc);
	if (ret){
		printk(KERN_ERR "JTAG : failed to request interrupt\n");
		goto out_irq;
	}

	platform_set_drvdata(pdev, ast_jtag);

	ret = sysfs_create_group(&pdev->dev.kobj, &jtag_attribute_group);
	if (ret) {
		JTAG_ERR("ast_jtag: failed to create sysfs device attributes.\n");
		return -1;
	}

	JTAG_MSG("ast_jtag: driver successfully loaded.\n");

	return 0;

out_irq:
#ifdef USE_INTERRUPTS
	free_irq(ast_jtag->irq, NULL);
#endif
out_region:
	release_mem_region(res->start, res->end - res->start + 1);
out:
	if (ast_jtag) {
	  kfree(ast_jtag);
	  ast_jtag = NULL;
	}

	printk(KERN_WARNING "applesmc: driver init failed (ret=%d)!\n", ret);
	return ret;
}

static int ast_jtag_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct ast_jtag_info *ast_jtag = platform_get_drvdata(pdev);

	if (ast_jtag == NULL)
		return 0;

	JTAG_DBUG("ast_jtag_remove\n");

	sysfs_remove_group(&pdev->dev.kobj, &jtag_attribute_group);

	misc_deregister(&ast_jtag_misc);

#ifdef USE_INTERRUPTS
	free_irq(ast_jtag->irq, ast_jtag);
#endif

	iounmap(ast_jtag->reg_base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL)
		release_mem_region(res->start, res->end - res->start + 1);

	platform_set_drvdata(pdev, NULL);

	kfree(ast_jtag);

	ast_jtag_slave();

	JTAG_MSG("ast_jtag: JTAG driver removed successfully!\n");

	return 0;
}

#ifdef CONFIG_PM
static int
ast_jtag_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int
ast_jtag_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define ast_jtag_suspend        NULL
#define ast_jtag_resume         NULL
#endif

static struct platform_driver ast_jtag_driver = {
	.remove 		= ast_jtag_remove,
	.suspend        = ast_jtag_suspend,
	.resume         = ast_jtag_resume,
	.driver         = {
		.name   = "ast-jtag",
		.owner  = THIS_MODULE,
	},
};

module_platform_driver_probe(ast_jtag_driver, ast_jtag_probe);

MODULE_AUTHOR("Ryan Chen <ryan_chen@aspeedtech.com>");
MODULE_DESCRIPTION("AST JTAG LIB Driver");
MODULE_LICENSE("GPL");

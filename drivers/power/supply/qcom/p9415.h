/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __P9415_H__
#define __P9415_H__

/* Register definitions */

/* Chip ID registers: 0x00, 0x01 */
#define P9415_CHIP_ID_REG			(0x0)
#define P9415_ID_TAG				(0x9415)

/* HW Rev register */
#define P9415_HW_REV_REG			(0x2)

#define P9415_FW_REV_MAJOR_REG			(0x8)
#define P9415_FW_REV_MINOR_REG			(0x9)
#define P9415_FW_REV_BETA_REG			(0xA)

/* TRX interrupt clear registers: 0x28, 0x29, 0x2A, 0x2B */
#define P9415_INT_CLEAR_TRX_REG		(0x28)
#define P9415_INT_CLEAR_RX_OTP_BIT	BIT(2)
#define P9415_INT_CLEAR_RX_OV_BIT	BIT(1)
#define P9415_INT_CLEAR_RX_OC_BIT	BIT(0)

/* TX interrupt clear registers: 0x28, 0x29, 0x2A, 0x2B */
#define P9415_INT_CLEAR_TX_CSPREC_BIT	BIT(8)
#define P9415_INT_CLEAR_TX_TXINIT_BIT	BIT(7)
#define P9415_INT_CLEAR_TX_GETDPING_BIT	BIT(6)
#define P9415_INT_CLEAR_TX_MODECHG_BIT	BIT(5)
#define P9415_INT_CLEAR_TX_GETCFG_BIT	BIT(4)
#define P9415_INT_CLEAR_TX_GETID_BIT	BIT(3)
#define P9415_INT_CLEAR_TX_GETSS_BIT	BIT(2)
#define P9415_INT_CLEAR_TX_STARTDPING_BIT	BIT(1)
#define P9415_INT_CLEAR_TX_EPTTYPE_BIT	BIT(0)

/* status register for TRX mode:  0x2C, 0x2D, 0x2E, 0x2F */
#define P9415_STATUS_TRX_REG	(0x2C)

/* status register for Tx mode:  0x2C, 0x2D, 0x2E, 0x2F */
#define P9415_STATUS_TX_TXINIT_BIT	BIT(7)
#define P9415_STATUS_TX_GETDPING_BIT	BIT(6)
#define P9415_STATUS_TX_MODECHG_BIT	BIT(5)
#define P9415_STATUS_TX_GETCFG_BIT	BIT(4)
#define P9415_STATUS_TX_GETID_BIT	BIT(3)
#define P9415_STATUS_TX_GETSS_BIT	BIT(2)
#define P9415_STATUS_TX_STARTDPING_BIT	BIT(1)
#define P9415_STATUS_TX_EPTTYPE_BIT	BIT(0)

/* interrupt register for TRX mode: 0x30, 0x31, 0x32, 0x33 */
#define P9415_INT_TRX_REG	(0x30)
#define P9415_INT_RX_OCW_BIT	BIT(23)
#define P9415_INT_RX_ADTERR_BIT	BIT(22)
#define P9415_INT_RX_ADTRCV_BIT	BIT(21)
#define P9415_INT_RX_ADTSENT_BIT	BIT(20)
#define P9415_INT_RX_NTCOT_BIT	BIT(19)
#define P9415_INT_RX_VRECTON_BIT	BIT(18)
#define P9415_INT_RX_NOP_BIT	GENMASK(17, 16)
#define P9415_INT_RX_VSWITCHNOK_BIT	BIT(15)
#define P9415_INT_RX_SLEEP_BIT	BIT(14)
#define P9415_INT_RX_IDAUTHOK_BIT	BIT(13)
#define P9415_INT_RX_IDAUTHNOK_BIT	BIT(12)
#define P9415_INT_RX_BCOK_BIT	BIT(11)
#define P9415_INT_RX_BCTIMEOUT_BIT	BIT(10)
#define P9415_INT_RX_TXAUTHOK_BIT	BIT(9)
#define P9415_INT_RX_TXAUTHNOK_BIT	BIT(8)
#define P9415_INT_RX_LDODIS_BIT	BIT(7)
#define P9415_INT_RX_LDOENA_BIT	BIT(6)
#define P9415_INT_RX_MODECHG_BIT	BIT(5)
#define P9415_INT_RX_TXDATA_BIT	BIT(4)
#define P9415_INT_RX_VSWITCHOK_BIT	BIT(3)
#define P9415_INT_RX_OT_BIT	BIT(2)
#define P9415_INT_RX_OV_BIT	BIT(1)
#define P9415_INT_RX_OC_BIT	BIT(0)

/* interrupt register for TX mode: 0x30, 0x31, 0x32, 0x33 */
#define P9415_INT_TX_CSPREC_BIT	BIT(8)
#define P9415_INT_TX_TXINIT_BIT	BIT(7)
#define P9415_INT_TX_GETDPING_BIT	BIT(6)
#define P9415_INT_TX_MODECHG_BIT	BIT(5)
#define P9415_INT_TX_GETCFG_BIT		BIT(4)
#define P9415_INT_TX_GETID_BIT		BIT(3)
#define P9415_INT_TX_GETSS_BIT		BIT(2)
#define P9415_INT_TX_STARTDPING_BIT	BIT(1)
#define P9415_INT_TX_EPTTYPE_BIT	BIT(0)

/* interrupt enable register for RX mode: 0x34, 0x35, 0x36, 0x37 */
#define P9415_INT_EN_RX_REG	(0x34)
#define P9415_INT_EN_RX_OCW_BIT	BIT(23)
#define P9415_INT_EN_RX_ADTERR_BIT	BIT(22)
#define P9415_INT_EN_RX_ADTRCV_BIT	BIT(21)
#define P9415_INT_EN_RX_ADTSENT_BIT	BIT(20)
#define P9415_INT_EN_RX_NTCOT_BIT	BIT(19)
#define P9415_INT_EN_RX_VRECTON_BIT	BIT(18)
#define P9415_INT_EN_RX_VSWITCHNOK_BIT	BIT(15)
#define P9415_INT_EN_RX_SLEEP_BIT	BIT(14)
#define P9415_INT_EN_RX_IDAUTHOK_BIT	BIT(13)
#define P9415_INT_EN_RX_IDAUTHNOK_BIT	BIT(12)
#define P9415_INT_EN_RX_BCOK_BIT	BIT(11)
#define P9415_INT_EN_RX_BCTIMEOUT_BIT	BIT(10)
#define P9415_INT_EN_RX_TXAUTHOK_BIT	BIT(9)
#define P9415_INT_EN_RX_TXAUTHNOK_BIT	BIT(8)
#define P9415_INT_EN_RX_LDODIS_BIT	BIT(7)
#define P9415_INT_EN_RX_LDOENA_BIT	BIT(6)
#define P9415_INT_EN_RX_MODECHG_BIT	BIT(5)
#define P9415_INT_EN_RX_TXDATA_BIT	BIT(4)
#define P9415_INT_EN_RX_VSWITCHOK_BIT	BIT(3)
#define P9415_INT_EN_RX_OT_BIT	BIT(2)
#define P9415_INT_EN_RX_OV_BIT	BIT(1)
#define P9415_INT_EN_RX_OC_BIT	BIT(0)

/* interrupt enable register for TX mode: 0x34, 0x35, 0x36, 0x37 */
#define P9415_INT_EN_TX_REG	(0x34)
#define P9415_INT_EN_TX_CSPREC_BIT	BIT(8)
#define P9415_INT_EN_TX_TXINIT_BIT	BIT(7)
#define P9415_INT_EN_TX_GETDPING_BIT	BIT(6)
#define P9415_INT_EN_TX_MODECHG_BIT	BIT(5)
#define P9415_INT_EN_TX_GETCFG_BIT		BIT(4)
#define P9415_INT_EN_TX_GETID_BIT		BIT(3)
#define P9415_INT_EN_TX_GETSS_BIT		BIT(2)
#define P9415_INT_EN_TX_STARTDPING_BIT	BIT(1)
#define P9415_INT_EN_TX_EPTTYPE_BIT	BIT(0)

#define P9415_X_AXIS_REG	(0x38)
#define P9415_Y_AXIS_REG	(0x39)

/*  12-bit of current main LDO Vout ADC value. registers: 0x3c, 0x3d
Vout = adc_code * 10 * 2.1 / 4095 (V)
*/
#define P9415_VOUT_ADC_REG	(0x3c)
#define P9415_RAW_TO_VOUT_UV(raw)	(((raw) * 21000) / 4095 * 1000)

/*  Set the output voltage of the main LDO. registers: 0x3E, 0x3F
register value = (V_out_mV - 2800mV) * 10 / 84
*/
#define P9415_VOUT_MIN_UV	(2800000)
#define P9415_VOUT_SET_REG	(0x3E)
#define P9415_VOUT_UV_TO_RAW(vout_uv)	((vout_uv - P9415_VOUT_MIN_UV)/8400)

/* 12 LSB of current Vrect ADC value. registers: 0x40, 0x41
Vrect = adc_code * 10 * 2.1 / 4095 (V)
*/
#define P9415_VRECT_ADC_REG	(0x40)
#define P9415_RAW_TO_VRECT_UV(raw)	(((raw) * 21000) / 4095 * 1000)

/* 12-bit raw data of the thermistor ADC reading on GP2 pin. regs: 0x42,0x43
V_GP2 = NTCTemp * 10 * 2.1 / 4095 (V)
*/
#define P9415_NTC_TEMP_REG	(0x42)
#define P9415_NTC_TEMP_MASK	GENMASK(11, 0)
#define P9415_NTC_TEMP_SCALED_MV(raw)	(((raw) * 21000) / 4095)

/* 16-bit raw data, read from 0x44 and 0x45 registers.
In RX mode the AP may read this register to get current Iout level in mA.
In TX mode the register holds the Power Supply current Iin in mA.
*/
#define P9415_IOUT_REG		(0x44)

/* 12bit of current Die Temperature ADC value from two registers: 0x46, 0x47 
die temp degree = adc_code * 0.075 - 174
*/
#define P9415_DIE_TEMP_REG	(0x46)
#define P9415_DIE_TEMP_SCALED_DEG_C(raw)	((((raw) * 75) / 1000) - 174)

/* AC frequency: F(KHz) = 64 * 6000 / raw,
default val: reg0x48=0x7e, reg0x49=0x0b => 
F = 64 * 6000 / 0x0b7e (KHz) = 384000 / 2942 = 130KHz */
#define P9415_ACP_REG	(0x48)
#define P9415_ACP_SCALED_KHZ(raw)	(384000 / (raw))

/* Set main LDO current limit in 0.1A steps. ILIM = val * 100000 uA */
#define P9415_ILIM_REG		(0x4A)
#define P9415_ILIM_STEP_UA	(100000)

/* Signal Strength Packet Sent by 9415 at start of power transfer */
#define P9415_SSP_REG		(0x4B)

/* Mode register, RX mode:0x09 */
#define P9415_MODE_REG		(0x4D)
#define P9415_MODE_BACKPOWERD_BIT	BIT(7)
#define P9415_MODE_EPP_BIT	BIT(3)
#define P9415_MODE_TX_BIT	BIT(2)
#define P9415_MODE_RX_BIT	BIT(0)

/* Cmd registers: 0x4E, 0x4F */
#define P9415_CMD_REG	(0x4E)
#define P9415_CMD_RENEG_BIT			BIT(15)
#define P9415_CMD_SETIDAUTH_BIT		BIT(14)
#define P9415_CMD_CCACTIVATE_BIT	BIT(9)
#define P9415_CMD_SOFTRESTART_BIT	BIT(8)
#define P9415_CMD_VSWITCH_BIT	BIT(7)
#define P9415_CMD_CLRINT_BIT	BIT(5)
#define P9415_CMD_SENDCSP_BIT	BIT(4)
#define P9415_CMD_SENDEOP_BIT	BIT(3)
#define P9415_CMD_SHA1AUTH_BIT	BIT(2)
#define P9415_CMD_LDOTGL_BIT	BIT(1)
#define P9415_CMD_SENDPP_BIT	BIT(0)

#define P9415_FOD_RX_GAIN_OFFSET_REG0	(0x68)
#define P9415_FOD_RX_GAIN_OFFSET_REG1	(0x6A)
#define P9415_FOD_RX_GAIN_OFFSET_REG2	(0x6C)
#define P9415_FOD_RX_GAIN_OFFSET_REG3	(0x6E)
#define P9415_FOD_RX_GAIN_OFFSET_REG4	(0x70)
#define P9415_FOD_RX_GAIN_OFFSET_REG5	(0x72)
#define P9415_FOD_RX_GAIN_OFFSET_REG6	(0x74)
#define P9415_FOD_RX_GAIN_OFFSET_REG7	(0x76)

#define P9415_TXPOWER_REG	(0x84)
#define P9415_RAW_TO_POWER_UW(raw)	(raw >> 1)

#define P9415_COMCAPSEL_REG	(0xB6)
#define P9415_COMCAP_MASK	GENMASK(7, 4)
#define P9415_CMA_EN_BIT	BIT(4)
#define P9415_CMB_EN_BIT	BIT(5)
#define P9415_COM1_EN_BIT	BIT(6)
#define P9415_COM2_EN_BIT	BIT(7)

/* TX mode registers only valid when already in TX mode */
/* 16 bits long, 0x70, 0x71 */
#define P9415_TX_VIN_REG	(0x70)

/* 16 bits long, 0x72, 0x73 */
#define P9415_TX_VRECT_REG	(0x72)

/* 16 bits long, 0x74, 0x75 */
#define P9415_TX_EPTR_REG	(0x74)
#define P9415_EPT_POCP_BIT	BIT(15)
#define P9415_EPT_OTP_BIT	BIT(14)
#define P9415_EPT_FOD_BIT	BIT(13)
#define P9415_EPT_LVP_BIT	BIT(12)
#define P9415_EPT_OVP_BIT	BIT(11)
#define P9415_EPT_OCP_BIT	BIT(10)
#define P9415_EPT_CEPT_BIT	BIT(8)
#define P9415_EPT_WDGT_BIT	BIT(7)
#define P9415_EPT_CMD_BIT	BIT(0)

/* 16 bits long, 0x76, 0x77(reserved) */
#define P9415_TX_SCR_REG	(0x76)
#define P9415_SCR_CTCCMD_BIT	BIT(6)
#define P9415_SCR_TX_CLRINT_BIT	BIT(5)
#define P9415_SCR_TX_BC_BIT		BIT(3)
#define P9415_SCR_TX_DIS_BIT	BIT(2)
#define P9415_SCR_TX_EN_BIT		BIT(0)

/* 16 bits long, 0x98, 0x99, mV */
#define P9415_TX_OVPTHD_REG	(0x98)

/* 16 bits long, 0x9A, 0x9B, mA */
#define P9415_TX_OCPTHD_REG	(0x9A)

/* 16 bits long, 0xA8, 0xA9, mW, default 0x0320, 800mW */
#define P9415_TX_FODTHDL_REG	(0xA8)

/* 16 bits long, 0xAA, 0xAB, mW, default 0xFF9C, -100mW */
#define P9415_TX_FODTHDH_REG	(0xAA)

/* 16 bits long, 0xAC, 0xAD, mW, default 0x0E74, 3700mW */
#define P9415_TX_FODSEGTHD_REG	(0xAC)

/* 16 bits long, 0xBA, 0xBB, mS, default 0x038E, 910mS */
#define P9415_TX_PING_INTVAL_REG	(0xBA)

#endif

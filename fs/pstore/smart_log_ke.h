/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
  *
  * MediaTek Inc. (C) 2010. All rights reserved.
  *
  * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
  * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
  * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
  * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
  * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
  * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
  * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
  * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
  * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
  * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
  * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
  * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
  * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
  * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
  * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
  * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
  * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
  * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
  *
  * The following software/firmware and/or related documentation ("MediaTek
  * Software") have been modified by MediaTek Inc. All revisions are subject to
  * any receiver's applicable license agreements with MediaTek Inc.
  */

#ifndef SMART_LOG_KE_H
#define SMART_LOG_KE_H

/*=============================================================================

                            INCLUDE FILES FOR MODULE

=============================================================================*/

#include <linux/types.h>
/*===========================================================================

                      PUBLIC FUNCTION DECLARATIONS

===========================================================================*/
typedef unsigned char  uint8;
typedef unsigned int   uint32;
typedef unsigned short uint16;
/*
 * pstore address  0x9B000000
 * SMARTLOG 512k, 10 circles in total
 * log_emmc header   512  //only first 512 need write emmc header
 * SMARTLOG header   512  //each log need a SMARTLOG header
 * pl lk log         32K
 * kernel log        512K-64K
 */
#define SCL_TCT_SMARTLOG_RAM_BASE (CONFIG_SMARTLOG_MEM_ADDR)
#define SCL_TCT_SMARTLOG_RAM_SIZE (0x100000)
//max write circle
#define SMARTLOG_MAX_INDEX (10)

//smartlog status
#define BUFF_NOT_READY 0x10
#define BUFF_READY     0x20
#define BUFF_FULL      0x40
#define BUFF_ERROR     0x80

//smartlog magic
#define SMARTLOG_EMMC_MAGIC (0x5678ef90)
#define SMARTLOG_HDR_MAGIC  (0xabcd1234)
//log sig
#define LOG_HEADER_SIG (0xcdab3412)

//SMARTLOG in 8M offset with smart
#define SMARTLOG_EMMC_OFFSET  (0x00)
//offset in DRAM
#define SMARTLOG_EMMC_HEADER_SIZE  (0x200)  //512
#define SMARTLOG_HEADER_SIZE  (0x200)  //512
#define SMARTLOG_BOOTLOG_OFFSET (SMARTLOG_EMMC_HEADER_SIZE + SMARTLOG_HEADER_SIZE)
#define SMARTLOG_BOOTLOG_SIZE (0x8000)
//kernel offset
#define SMARTLOG_KERNEL_OFFSET (SMARTLOG_BOOTLOG_OFFSET + SMARTLOG_BOOTLOG_SIZE)
#define SMARTLOG_KERNEL_SIZE (0x70000) //512K-64K

//flag
enum {
    LOG_EARLY_PRINTK  = 0x1,
    LOG_BUFF_FULL     = 0x2,
    LOG_SBL_FINISH    = 0x10,
    LOG_LK_FINISH     = 0x20,
    LOG_KERNEL_FINISH = 0x40,
    RESERVED,
    SMARTLOG_TOKEN    = 0x100,
    BOOT_EXCEPTION    = 0x200,
    NEED_WRITE_TO_EMMC = 0x400,
}SMARTLOG_FLAG;

struct boot_log_header
{
  uint32 boot_log_start;

  /**
   * total size of the logging buffer in bytes
   */
  uint32 boot_log_size;

  /**
    *the sig of this struct, normal is LOG_HEADER_SIG
    */
  uint32 sig;
  /**
    * log flag to indicate the log status
    */
  uint32 flag;
};


struct kernel_log_header {
    uint32 sig; //LOG_HEADER_SIG
    uint32 flag;
    uint32 klog_size;
    uint32 klog_start;
    uint32 sz_dump;
    uint32 sz_console;
};

struct smartlog_emmc_header {
    uint32 magic;
    uint16 version_major;
    uint16 version_minor;
    uint32 index;  //current index
    uint32 flag;
    uint32 reserve;
};
//log header in dram
struct smartlog_header {
    uint32 magic;
    uint32 index; //current index
    uint32 log_size;//total log size
    uint32 flag;
    uint32 reboot_count;
    uint32 save_to_emmc;
    struct boot_log_header   blog_hdr;
    struct kernel_log_header klog_hdr;
  //to store header from emmc
    struct smartlog_emmc_header emmc_header;
};

#endif /* SMART_LOG_KE_H */




/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_EEPROM_CORE_H_
#define _CAM_EEPROM_CORE_H_

#include "cam_eeprom_dev.h"

int32_t cam_eeprom_driver_cmd(struct cam_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t cam_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct cam_eeprom_ctrl_t *e_ctrl);
/**
 * @e_ctrl: EEPROM ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */
void cam_eeprom_shutdown(struct cam_eeprom_ctrl_t *e_ctrl);

ssize_t calibration_flag_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t calibration_flag_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count);
ssize_t calibration_data_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t calibration_data_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count);
/*[bug-fix]-mod-begin,by jinghuang@tcl.com*/
/*add eeprom r/w for ois cali*/
#if defined(CONFIG_TCT_LITO_OTTAWA) || defined(CONFIG_TCT_LITO_CHICAGO)
ssize_t ois_cali_eeprom_data_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t ois_cali_eeprom_data_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count);
/*[bug-fix]-mod-end,by jinghuang@tcl.com*/
#endif
#endif
/* _CAM_EEPROM_CORE_H_ */

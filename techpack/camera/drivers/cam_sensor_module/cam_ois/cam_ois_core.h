/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_OIS_CORE_H_
#define _CAM_OIS_CORE_H_

#include <linux/cma.h>
#include "cam_ois_dev.h"

#define OIS_NAME_LEN 32

/**
 * @power_info: power setting info to control the power
 *
 * This API construct the default ois power setting.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int32_t cam_ois_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info);


int cam_ois_driver_cmd(struct cam_ois_ctrl_t *e_ctrl, void *arg);

/**
 * @o_ctrl: OIS ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */
void cam_ois_shutdown(struct cam_ois_ctrl_t *o_ctrl);

/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9220454 on 20200413*/
/*add function ois r/w  */
ssize_t ois_cali_data_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t ois_cali_data_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count);
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9220454 on 20200413*/


#endif
/* _CAM_OIS_CORE_H_ */

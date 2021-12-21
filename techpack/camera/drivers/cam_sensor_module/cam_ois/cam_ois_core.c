// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/dma-contiguous.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_sensor_util.h"
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
int32_t cam_ois_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 1;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 2;

	power_info->power_down_setting_size = 1;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}


/**
 * cam_ois_get_dev_handle - get device handle
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int cam_ois_get_dev_handle(struct cam_ois_ctrl_t *o_ctrl,
	void *arg)
{
	struct cam_sensor_acquire_dev    ois_acq_dev;
	struct cam_create_dev_hdl        bridge_params;
	struct cam_control              *cmd = (struct cam_control *)arg;

	if (o_ctrl->bridge_intf.device_hdl != -1) {
		CAM_ERR(CAM_OIS, "Device is already acquired");
		return -EFAULT;
	}
	if (copy_from_user(&ois_acq_dev, u64_to_user_ptr(cmd->handle),
		sizeof(ois_acq_dev)))
		return -EFAULT;

	bridge_params.session_hdl = ois_acq_dev.session_handle;
	bridge_params.ops = &o_ctrl->bridge_intf.ops;
	bridge_params.v4l2_sub_dev_flag = 0;
	bridge_params.media_entity_flag = 0;
	bridge_params.priv = o_ctrl;

	ois_acq_dev.device_handle =
		cam_create_device_hdl(&bridge_params);
	o_ctrl->bridge_intf.device_hdl = ois_acq_dev.device_handle;
	o_ctrl->bridge_intf.session_hdl = ois_acq_dev.session_handle;

	CAM_DBG(CAM_OIS, "Device Handle: %d", ois_acq_dev.device_handle);
	if (copy_to_user(u64_to_user_ptr(cmd->handle), &ois_acq_dev,
		sizeof(struct cam_sensor_acquire_dev))) {
		CAM_ERR(CAM_OIS, "ACQUIRE_DEV: copy to user failed");
		return -EFAULT;
	}
	return 0;
}

static int cam_ois_power_up(struct cam_ois_ctrl_t *o_ctrl)
{
	int                             rc = 0;
	struct cam_hw_soc_info          *soc_info =
		&o_ctrl->soc_info;
	struct cam_ois_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t  *power_info;

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_OIS,
			"Using default power settings");
		rc = cam_ois_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Construct default ois power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"failed to fill vreg params for power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_OIS, "failed in ois power up rc %d", rc);
		return rc;
	}

	rc = camera_io_init(&o_ctrl->io_master_info);
	if (rc)
		CAM_ERR(CAM_OIS, "cci_init failed: rc: %d", rc);

	return rc;
}

/**
 * cam_ois_power_down - power down OIS device
 * @o_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int cam_ois_power_down(struct cam_ois_ctrl_t *o_ctrl)
{
	int32_t                         rc = 0;
	struct cam_sensor_power_ctrl_t  *power_info;
	struct cam_hw_soc_info          *soc_info =
		&o_ctrl->soc_info;
	struct cam_ois_soc_private *soc_private;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &o_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_OIS, "failed: power_info %pK", power_info);
		return -EINVAL;
	}

	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_OIS, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&o_ctrl->io_master_info);

	return rc;
}

static int cam_ois_apply_settings(struct cam_ois_ctrl_t *o_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;
	uint32_t i, size;

	if (o_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_OIS, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		if (i2c_list->op_code ==  CAM_SENSOR_I2C_WRITE_RANDOM) {
			rc = camera_io_dev_write(&(o_ctrl->io_master_info),
				&(i2c_list->i2c_settings));
			if (rc < 0) {
				CAM_ERR(CAM_OIS,
					"Failed in Applying i2c wrt settings");
				return rc;
			}
		} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
			size = i2c_list->i2c_settings.size;
			for (i = 0; i < size; i++) {
				rc = camera_io_dev_poll(
				&(o_ctrl->io_master_info),
				i2c_list->i2c_settings.reg_setting[i].reg_addr,
				i2c_list->i2c_settings.reg_setting[i].reg_data,
				i2c_list->i2c_settings.reg_setting[i].data_mask,
				i2c_list->i2c_settings.addr_type,
				i2c_list->i2c_settings.data_type,
				i2c_list->i2c_settings.reg_setting[i].delay);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
						"i2c poll apply setting Fail");
					return rc;
				}
			}
		}
	}

	return rc;
}

static int cam_ois_slaveInfo_pkt_parser(struct cam_ois_ctrl_t *o_ctrl,
	uint32_t *cmd_buf, size_t len)
{
	int32_t rc = 0;
	struct cam_cmd_ois_info *ois_info;

	if (!o_ctrl || !cmd_buf || len < sizeof(struct cam_cmd_ois_info)) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	ois_info = (struct cam_cmd_ois_info *)cmd_buf;
	if (o_ctrl->io_master_info.master_type == CCI_MASTER) {
		o_ctrl->io_master_info.cci_client->i2c_freq_mode =
			ois_info->i2c_freq_mode;
		o_ctrl->io_master_info.cci_client->sid =
			ois_info->slave_addr >> 1;
		o_ctrl->ois_fw_flag = ois_info->ois_fw_flag;
		o_ctrl->is_ois_calib = ois_info->is_ois_calib;
		memcpy(o_ctrl->ois_name, ois_info->ois_name, OIS_NAME_LEN);
		o_ctrl->ois_name[OIS_NAME_LEN - 1] = '\0';
		o_ctrl->io_master_info.cci_client->retries = 3;
		o_ctrl->io_master_info.cci_client->id_map = 0;
		memcpy(&(o_ctrl->opcode), &(ois_info->opcode),
			sizeof(struct cam_ois_opcode));
		CAM_DBG(CAM_OIS, "Slave addr: 0x%x Freq Mode: %d",
			ois_info->slave_addr, ois_info->i2c_freq_mode);
	} else if (o_ctrl->io_master_info.master_type == I2C_MASTER) {
		o_ctrl->io_master_info.client->addr = ois_info->slave_addr;
		CAM_DBG(CAM_OIS, "Slave addr: 0x%x", ois_info->slave_addr);
	} else {
		CAM_ERR(CAM_OIS, "Invalid Master type : %d",
			o_ctrl->io_master_info.master_type);
		rc = -EINVAL;
	}

	return rc;
}

static int cam_ois_fw_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, cnt;
	uint32_t                           fw_size;
	const struct firmware             *fw = NULL;
	const char                        *fw_name_prog = NULL;
	const char                        *fw_name_coeff = NULL;
	char                               name_prog[32] = {0};
	char                               name_coeff[32] = {0};
	struct device                     *dev = &(o_ctrl->pdev->dev);
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	snprintf(name_coeff, 32, "%s.coeff", o_ctrl->ois_name);

	snprintf(name_prog, 32, "%s.prog", o_ctrl->ois_name);

	/* cast pointer as const pointer*/
	fw_name_prog = name_prog;
	fw_name_coeff = name_coeff;

	/* Load FW */
	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_prog);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	for (cnt = 0, ptr = (uint8_t *)fw->data; cnt < total_bytes;
		cnt++, ptr++) {
		i2c_reg_setting.reg_setting[cnt].reg_addr =
			o_ctrl->opcode.prog;
		i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
		i2c_reg_setting.reg_setting[cnt].delay = 0;
		i2c_reg_setting.reg_setting[cnt].data_mask = 0;
	}
    /*[bug-fix]-mod-begin,by jinghuang	task 9481054 on 20200526*/
    /*modify i2c SEQ MODE*/
	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&i2c_reg_setting, 0);
    /*[bug-fix]-mod-end,by jinghuang  task 9481054 on 20200526*/
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
		goto release_firmware;
	}
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	page = NULL;
	fw_size = 0;
	release_firmware(fw);

	rc = request_firmware(&fw, fw_name_coeff, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_coeff);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	for (cnt = 0, ptr = (uint8_t *)fw->data; cnt < total_bytes;
		cnt++, ptr++) {
		i2c_reg_setting.reg_setting[cnt].reg_addr =
			o_ctrl->opcode.coeff;
		i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
		i2c_reg_setting.reg_setting[cnt].delay = 0;
		i2c_reg_setting.reg_setting[cnt].data_mask = 0;
	}
    /*[bug-fix]-mod-begin,by jinghuang	task 9481054 on 20200526*/
    /*modify i2c SEQ MODE*/
	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&i2c_reg_setting,0);
    /*[bug-fix]-mod-end,by jinghuang  task 9481054 on 20200526*/
	if (rc < 0)
		CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);

    CAM_ERR(CAM_OIS, "OIS FW download OK");

release_firmware:
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	release_firmware(fw);

	return rc;
}

/*[bug-fix]-mod-begin,by jinghuang  task 9049013 on 20200408*/
/*add for ois*/
typedef struct REGSETTING{
	uint16_t reg ;
	uint16_t val ;
}REGSETTING ;
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9220454 on 20200413*/
/*disable/enable ois center*/
static int center_flag=0;
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9220454 on 20200413*/

//add by jinghuang for mmi apk
static int mmi_flag=0;

static int cam_ois_preinitsetting_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	int32_t                            rc = 0, cnt;
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
  uint32_t                           fw_size;
  const REGSETTING preinit_setting[] = {
    {0x8262 ,0xBF03} ,
    {0x8263 ,0x9F05} ,
    {0x8264 ,0x6040} ,
    {0x8260 ,0x1130} ,
    {0x8265 ,0x8000}  ,
    {0x8261 ,0x0280} ,
    {0x8261 ,0x0380} ,
    {0x8261 ,0x0988} ,
  } ;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}


	total_bytes = sizeof(preinit_setting)/sizeof(preinit_setting[0]) ;

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *	total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),fw_size, 0, GFP_KERNEL);

	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (page_address(page));
/*[bug-fix]-mod-begin,by jinghuang ,task 9466923 on20200520*/
/*add delay for modify  continue address mode to single address.OIS ic only support  single address.*/
	for (cnt = 0 ; cnt < total_bytes ; cnt++) {
		i2c_reg_setting.reg_setting[cnt].reg_addr = preinit_setting[cnt].reg ;
		i2c_reg_setting.reg_setting[cnt].reg_data = preinit_setting[cnt].val ;
		i2c_reg_setting.reg_setting[cnt].delay = 1;
		i2c_reg_setting.reg_setting[cnt].data_mask = 0;
		CAM_ERR(CAM_OIS ,"REG_0x%x : 0x%x",i2c_reg_setting.reg_setting[cnt].reg_addr,
		                                   i2c_reg_setting.reg_setting[cnt].reg_data);
	}
/*[bug-fix]-mod-end,by jinghuang ,task 9466923 on20200520*/

	rc = camera_io_dev_write(&(o_ctrl->io_master_info),	&i2c_reg_setting);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "Init Setting download failed %d", rc);
//		return -ENOMEM;
	}
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),	page, fw_size);
	page = NULL;

	return rc;
}

static int cam_ois_initsetting_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	int32_t                            rc = 0, cnt;
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
  uint32_t                           fw_size;
  const REGSETTING initsetting[] = {
    {0x8205 ,0x0c00} ,
    {0x8205 ,0x0d00} ,
    {0x8c01 ,0x0101} ,
  } ;
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
    const REGSETTING initsetting_mmi[] = {
    {0x8205 ,0x0c00} ,
    {0x8205 ,0x0d00} ,
    {0x8c01 ,0x0101} ,
  } ;
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
    if(mmi_flag==1)
      total_bytes = sizeof(initsetting_mmi)/sizeof(initsetting_mmi[0]) ;
    else
	  total_bytes = sizeof(initsetting)/sizeof(initsetting[0]) ;
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *	total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),fw_size, 0, GFP_KERNEL);

	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (page_address(page));

  CAM_ERR(CAM_OIS ,"initsetting : total_bytes 0x%d",total_bytes);
 /*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
	for (cnt = 0 ; cnt < total_bytes;cnt++) {
		if(mmi_flag==1){
          i2c_reg_setting.reg_setting[cnt].reg_addr = initsetting_mmi[cnt].reg ;
		  i2c_reg_setting.reg_setting[cnt].reg_data = initsetting_mmi[cnt].val ;
		}else{
          i2c_reg_setting.reg_setting[cnt].reg_addr = initsetting[cnt].reg ;
          i2c_reg_setting.reg_setting[cnt].reg_data = initsetting[cnt].val ;
		}
		i2c_reg_setting.reg_setting[cnt].delay = 0;
		i2c_reg_setting.reg_setting[cnt].data_mask = 0;
    CAM_ERR(CAM_OIS ,"REG_0x%x : 0x%x",i2c_reg_setting.reg_setting[cnt].reg_addr,i2c_reg_setting.reg_setting[cnt].reg_data);
	}
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/

	rc = camera_io_dev_write(&(o_ctrl->io_master_info),	&i2c_reg_setting);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "Init Setting download failed %d", rc);
//		return -ENOMEM;
	}
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),	page, fw_size);
	page = NULL;

	return rc;
	}

static int cam_ois_enablesetting_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	int32_t                            rc = 0, cnt;
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
  uint32_t                           fw_size;
/*[bug-fix]-mod-beging,by jinghuang@tcl.com,task 9049013 on 20200409*/
/*enablesetting for old module*/
/*[bug-fix]-mod-beging,by jinghuang@tcl.com,task 9336600 on 20200428*/
/*config 1 degree to 1.3 degree  for ois cali*/
  const REGSETTING enablesetting[]= {
    {0x847f ,0x0c0c} ,
    {0x8436 ,0xfd7f} ,
    {0x8440 ,0xf03f} ,
    {0x8443 ,0xa023} ,

    {0x841b ,0x8000} ,
    {0x84b6 ,0xfd7f} ,
    {0x84c0 ,0xf03f} ,
    {0x84c3 ,0xa023} ,

    {0x849b ,0x8000} ,
    {0x8438 ,0x000e} ,
    {0x84b8 ,0x000e} ,
    {0x8447 ,0x811f} ,
    {0x84c7 ,0x811f} ,

    {0x8290 ,0xfd7f} ,
    {0x8296 ,0x8001} ,
    {0x8291 ,0x0500} ,
    {0x8292 ,0x0200} ,
    {0x8299 ,0x8004} ,

    {0x847f ,0x0d0d} , //center register disable
  };
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9336600 on 20200428*/
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9049013 on 20200409*/

/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
  const REGSETTING enablesetting_mmi[]= {
    {0x847f ,0x0c0c} ,
    {0x8436 ,0xff7f} ,
    {0x8440 ,0xff7f} ,
    {0x8443 ,0xff7f} ,

    {0x841b ,0x8000} ,
    {0x84b6 ,0xff7f} ,
    {0x84c0 ,0xff7f} ,
    {0x84c3 ,0xff7f} ,

    {0x849b ,0x8000} ,
    {0x8438 ,0xa412} ,
    {0x84b8 ,0xa412} ,
    {0x8447 ,0x000d} ,

    {0x84c7 ,0x000d} ,
    {0x847f ,0x0d0d} , //center register disable
  };
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/


	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
    if(mmi_flag==1)
	  total_bytes = sizeof(enablesetting_mmi)/sizeof(enablesetting_mmi[0]);
	else
      total_bytes = sizeof(enablesetting)/sizeof(enablesetting[0]);
	CAM_ERR(CAM_OIS, "total_bytes=%d",total_bytes);
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *	total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),fw_size, 0, GFP_KERNEL);

	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (page_address(page));
/*[bug-fix]-mod-beging,by jinghuang@tcl.com,9459312 on 20200518*/
/*compatible with old and new ois fw*/
/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*add setting only for mmi ois cali*/
	for (cnt = 0  ; cnt < total_bytes ;cnt++) {
          if(mmi_flag==1){
            i2c_reg_setting.reg_setting[cnt].reg_addr = enablesetting_mmi[cnt].reg;
            i2c_reg_setting.reg_setting[cnt].reg_data = enablesetting_mmi[cnt].val;
          }else{
            i2c_reg_setting.reg_setting[cnt].reg_addr = enablesetting[cnt].reg;
            i2c_reg_setting.reg_setting[cnt].reg_data = enablesetting[cnt].val;
          }
        

/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9483613 on 20200528*/
/*[bug-fix]-mod-begin,by jinghuang ,task 9466923 on20200520*/
/*add delay for modify	continue address mode to single address.OIS ic only support  single address.*/
        i2c_reg_setting.reg_setting[cnt].delay = 1;
/*[bug-fix]-mod-end,by jinghuang ,task 9466923 on20200520*/
        i2c_reg_setting.reg_setting[cnt].data_mask = 0;
        //CAM_ERR(CAM_OIS ,"REG_0x%x : 0x%x",i2c_reg_setting.reg_setting[cnt].reg_addr,i2c_reg_setting.reg_setting[cnt].reg_data);
    }
/*[bug-fix]-mod-end,by jinghuang@tcl.com,9459312 on 20200518*/

/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9220454 on 20200413*/
/*/*disable/enable ois center*/
    CAM_ERR(CAM_OIS, "ois current center_flag=%d,mmi_flag=%d",center_flag,mmi_flag);
    if(center_flag==1){
      i2c_reg_setting.reg_setting[total_bytes-1].reg_data=0x0c0c;
    }else if(center_flag==0){
      i2c_reg_setting.reg_setting[total_bytes-1].reg_data=0x0d0d;
	}
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9220454 on 20200413*/

        rc = camera_io_dev_write(&(o_ctrl->io_master_info),	&i2c_reg_setting);
        if (rc < 0) {
            CAM_ERR(CAM_OIS, "enable Setting download failed %d", rc);
//return -ENOMEM;
        }
        cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),	page, fw_size);
        page = NULL;

        return rc;
}

static int cam_ois_calidata_download(struct cam_ois_ctrl_t *o_ctrl)
{
    uint16_t                           total_bytes = 0;
    int32_t                            rc = 0, cnt;
    struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
    struct page                       *page = NULL;
    uint32_t                           fw_size;

  const REGSETTING calibrationdata[]= {
    {0x8230 ,0xda01} ,
    {0x8231 ,0xfc01} ,
    {0x8232 ,0xe801} ,

    {0x841E ,0x0f00} ,
    {0x849E ,0xa000} ,

    {0x8239 ,0x7f00} ,
    {0x823B ,0x7d00} ,

    {0x8406 ,0x1000} ,
    {0x8486 ,0x1000} ,

    {0x8446 ,0x00a0} ,
    {0x84c6 ,0x00a0} ,
    {0x840f ,0x0080} ,
    {0x848f ,0x0080} ,

    //{0x846A ,0x21} ,
    //{0x846B ,0x82} ,
    //{0x8470 ,0x08} ,
    //{0x8472 ,0x20} ,
  };
 //   return 0;//adb by jinghuang for test disable ois cali data
    if (!o_ctrl) {
        CAM_ERR(CAM_OIS, "Invalid Args");
        return -EINVAL;
    }

    total_bytes = sizeof(calibrationdata)/sizeof(calibrationdata[0]) ;

    i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
    i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
    i2c_reg_setting.size = total_bytes;
    i2c_reg_setting.delay = 0;
    fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *	total_bytes) >> PAGE_SHIFT;
    page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),fw_size, 0, GFP_KERNEL);

    if (!page) {
        CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
        return -ENOMEM;
    }
    i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (page_address(page));

    for (cnt = 0  ; cnt < total_bytes ;cnt++) {
        i2c_reg_setting.reg_setting[cnt].reg_addr = calibrationdata[cnt].reg ;
        i2c_reg_setting.reg_setting[cnt].reg_data = calibrationdata[cnt].val ;
        i2c_reg_setting.reg_setting[cnt].delay = 1;
        i2c_reg_setting.reg_setting[cnt].data_mask = 0;
        CAM_ERR(CAM_OIS ,"REG_0x%x : 0x%x",i2c_reg_setting.reg_setting[cnt].reg_addr,i2c_reg_setting.reg_setting[cnt].reg_data);
    }

    rc = camera_io_dev_write(&(o_ctrl->io_master_info),	&i2c_reg_setting);
    if (rc < 0) {
        CAM_ERR(CAM_OIS, "enable Setting download failed %d", rc);
 //       return -ENOMEM;
    }
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),	page, fw_size);
	page = NULL;

	return rc;
}
/*[bug-fix]-mod-end,by jinghuang  task 9049013 on 20200408*/


/*[bug-fix]-mod-begin,by jinghuang@tcl.com,task 9220454 on 20200413*/
/*add function ois r/w  */
static  char ois_read_cmd_buf[256];
ssize_t ois_cali_data_show(struct device *dev, struct device_attribute *attr, char *buf){

  //return data to user
  strcpy(buf,ois_read_cmd_buf);
  return sizeof(ois_read_cmd_buf);

}

ssize_t ois_cali_data_store(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count){

  struct cam_ois_ctrl_t *o_ctrl = NULL;
  int32_t                            rc = 0;
  struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
  struct page                       *page = NULL;
  uint32_t                           fw_size;
  char cmd_buf[32];
  uint32_t cmd_adress=0,cmd_data=0,num_bytes=0;
  char flag;
  uint8_t seqr_buff[64]={0},i=0;

  struct platform_device *pdev = container_of(dev, struct platform_device, dev);
  memset(cmd_buf,0,32);
  memset(ois_read_cmd_buf,0,sizeof(ois_read_cmd_buf));
  o_ctrl = platform_get_drvdata(pdev);


  if (!o_ctrl) {
      CAM_ERR(CAM_OIS, "Invalid Args");
      return count;
  }
  //cpy user cmd to kernel 0x:0x:r  0x:0x:w
  strcpy(cmd_buf,buf);
  sscanf(cmd_buf,"%x:%x:%c",&cmd_adress,&cmd_data,&flag);
  
  if(flag=='w'){
  CAM_ERR(CAM_OIS, "ois write:adress=0x%x,data=0x%x",cmd_adress,cmd_data);

  //add flag for center enable/disable
  if((cmd_adress==0x847f)&&(cmd_data==0x0c0c)){
    center_flag=1;
  }else if((cmd_adress==0x847f)&&(cmd_data==0x0d0d)){
    center_flag=0;
  }else{
    CAM_ERR(CAM_OIS, "ois center_flag=%d",center_flag);
  }
  i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
  i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
  i2c_reg_setting.size = 1;
  i2c_reg_setting.delay = 0;
  fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *	(i2c_reg_setting.size)) >> PAGE_SHIFT;
  page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),fw_size, 0, GFP_KERNEL);

  if (!page) {
      CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
      return count;
  }
  i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (page_address(page));
  i2c_reg_setting.reg_setting[0].reg_addr = cmd_adress ;
  i2c_reg_setting.reg_setting[0].reg_data =cmd_data ;
  i2c_reg_setting.reg_setting[0].delay = 1;
  i2c_reg_setting.reg_setting[0].data_mask = 0;

  rc = camera_io_dev_write(&(o_ctrl->io_master_info),	&i2c_reg_setting);
  if (rc < 0) {
      CAM_ERR(CAM_OIS, "enable Setting download failed %d", rc);
  }
  cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),	page, fw_size);
  page = NULL;

    }else if(flag=='r'){
     rc = camera_io_dev_read(&(o_ctrl->io_master_info),cmd_adress,&cmd_data,CAMERA_SENSOR_I2C_TYPE_WORD,CAMERA_SENSOR_I2C_TYPE_WORD);
     if (rc < 0) {
      CAM_ERR(CAM_OIS, "ois Failed: random read I2C settings: %d",rc);
      return count;
       } else{
            CAM_ERR(CAM_OIS,"ois read::address: 0x%x  reg_data: 0x%x",cmd_adress,cmd_data);
            sprintf(ois_read_cmd_buf,"%x\n",cmd_data);
            CAM_ERR(CAM_OIS, "ois ois_read_cmd_buf=%s",ois_read_cmd_buf);
       }
   }else if(flag=='s'){//seq read
     //mdelay(10);
     //[bug-fix]-add,by jinghuang@tcl.com,10594634  10562901 on 20201229,not allowed EIS read ois when ois is not ready
     if(o_ctrl->cam_ois_state==CAM_OIS_START){
     num_bytes=cmd_data;//read iic data count bytes
     rc = camera_io_dev_read_seq(&(o_ctrl->io_master_info),cmd_adress,seqr_buff,CAMERA_SENSOR_I2C_TYPE_BYTE,CAMERA_SENSOR_I2C_TYPE_BYTE,num_bytes);
     if (rc < 0) {
      CAM_ERR(CAM_OIS, "ois Failed: seqr read I2C settings: %d",rc);
      return count;
       } else{
            for(i=0;i<num_bytes;i++){
              CAM_DBG(CAM_OIS,"ois read::address: 0x%x  reg_data[%d]: 0x%x",cmd_adress,i,seqr_buff[i]);
              sprintf(&ois_read_cmd_buf[3*i],"%.2x:",seqr_buff[i]);
            }
            CAM_DBG(CAM_OIS,"ois read::address: 0x%x  reg_data: 0x%x",cmd_adress,cmd_data);
            CAM_DBG(CAM_OIS, "ois ois_read_cmd_buf=%s",ois_read_cmd_buf);
       }
      }
   }
else if(flag=='m'){//'m':enter mmi set flag 1
     CAM_ERR(CAM_OIS, "enter mmi ois cali set flag 1");
     mmi_flag=1;
   }else if(flag=='c'){//'c':enter mmi clear flag
    CAM_ERR(CAM_OIS, "exit mmi ois cali clear flag");
    mmi_flag=0;
   }else{
    CAM_ERR(CAM_OIS, "ois unknow cmd!!!");
   }
  return count;
}
/*[bug-fix]-mod-end,by jinghuang@tcl.com,task 9220454 on 20200413*/



/**
 * cam_ois_pkt_parse - Parse csl packet
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int cam_ois_pkt_parse(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int32_t                         rc = 0;
	int32_t                         i = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uintptr_t                       generic_ptr;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	struct i2c_settings_array      *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc        *cmd_desc = NULL;
	uintptr_t                       generic_pkt_addr;
	size_t                          pkt_len;
	size_t                          remain_len = 0;
	struct cam_packet              *csl_packet = NULL;
	size_t                          len_of_buff = 0;
	uint32_t                       *offset = NULL, *cmd_buf;
	struct cam_ois_soc_private     *soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t  *power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&dev_config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(dev_config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(dev_config.packet_handle,
		&generic_pkt_addr, &pkt_len);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"error in converting command Handle Error: %d", rc);
		return rc;
	}

	remain_len = pkt_len;
	if ((sizeof(struct cam_packet) > pkt_len) ||
		((size_t)dev_config.offset >= pkt_len -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_OIS,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), pkt_len);
		return -EINVAL;
	}

	remain_len -= (size_t)dev_config.offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_OIS, "Invalid packet params");
		return -EINVAL;
	}


	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_OIS_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		/* Loop through multiple command buffers */
		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;

			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
				&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "Failed to get cpu buf : 0x%x",
					cmd_desc[i].mem_handle);
				return rc;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_OIS, "invalid cmd buf");
				return -EINVAL;
			}

			if ((len_of_buff < sizeof(struct common_header)) ||
				(cmd_desc[i].offset > (len_of_buff -
				sizeof(struct common_header)))) {
				CAM_ERR(CAM_OIS,
					"Invalid length for sensor cmd");
				return -EINVAL;
			}
			remain_len = len_of_buff - cmd_desc[i].offset;
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				rc = cam_ois_slaveInfo_pkt_parser(
					o_ctrl, cmd_buf, remain_len);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
					"Failed in parsing slave info");
					return rc;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_OIS,
					"Received power settings buffer");
				rc = cam_sensor_update_power_settings(
					cmd_buf,
					total_cmd_buf_in_bytes,
					power_info, remain_len);
				if (rc) {
					CAM_ERR(CAM_OIS,
					"Failed: parse power settings");
					return rc;
				}
				break;
			default:
			if (o_ctrl->i2c_init_data.is_settings_valid == 0) {
				CAM_DBG(CAM_OIS,
				"Received init settings");
				i2c_reg_settings =
					&(o_ctrl->i2c_init_data);
				i2c_reg_settings->is_settings_valid = 1;
				i2c_reg_settings->request_id = 0;
				rc = cam_sensor_i2c_command_parser(
					&o_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1, NULL);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
					"init parsing failed: %d", rc);
					return rc;
				}
			} else if ((o_ctrl->is_ois_calib != 0) &&
				(o_ctrl->i2c_calib_data.is_settings_valid ==
				0)) {
				CAM_DBG(CAM_OIS,
					"Received calib settings");
				i2c_reg_settings = &(o_ctrl->i2c_calib_data);
				i2c_reg_settings->is_settings_valid = 1;
				i2c_reg_settings->request_id = 0;
				rc = cam_sensor_i2c_command_parser(
					&o_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1, NULL);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
						"Calib parsing failed: %d", rc);
					return rc;
				}
			}
			break;
			}
		}

		if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
			rc = cam_ois_power_up(o_ctrl);
			if (rc) {
				CAM_ERR(CAM_OIS, " OIS Power up failed");
				return rc;
			}
			o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
		}
        /*[bug-fix]-mod-begin,by jinghuang	task 9049013 on 20200408*/
        /*add for ois*/
		//step1 add by jinghuang  pre init setting
	    cam_ois_preinitsetting_download(o_ctrl);
	    mdelay(10);
        //step 2 add by jinghuang  download fw
		if (o_ctrl->ois_fw_flag) {
			rc = cam_ois_fw_download(o_ctrl);
			if (rc) {
				CAM_ERR(CAM_OIS, "Failed OIS FW Download");
				goto pwr_dwn;
			}else{
				CAM_ERR(CAM_OIS, " OIS FW Download ok");
			}
		}
        mdelay(5);
		//step 3 add by jinghuang  init setting
		cam_ois_initsetting_download(o_ctrl);
		mdelay(10);
#if 0
	//	rc = cam_ois_apply_settings(o_ctrl, &o_ctrl->i2c_init_data);
		if ((rc == -EAGAIN) &&
			(o_ctrl->io_master_info.master_type == CCI_MASTER)) {
			CAM_ERR(CAM_OIS,
				"CCI HW is restting: Reapplying INIT settings");
			usleep_range(1000, 1010);
			rc = cam_ois_apply_settings(o_ctrl,
				&o_ctrl->i2c_init_data);
		}

#endif
//step 4 add by jinghuang  calidata
	  CAM_ERR(CAM_OIS, "jinghuang is_ois_calib=%d",o_ctrl->is_ois_calib);
	#if 1
	  if (o_ctrl->is_ois_calib) {
		  rc = cam_ois_apply_settings(o_ctrl,
			  &o_ctrl->i2c_calib_data);
		  if (rc) {
			  CAM_ERR(CAM_OIS, "Cannot apply calib data");
			  goto pwr_dwn;
		  }
	  }else{
	cam_ois_calidata_download(o_ctrl);
	  }
#endif	  
    mdelay(10);
//step 5 add by jinghuang   enablesetting
    rc =  cam_ois_enablesetting_download(o_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Cannot apply Init settings: rc = %d",
				rc);
			goto pwr_dwn;
		}
#if 0
		CAM_ERR(CAM_OIS, "jinghuang is_ois_calib=%d",o_ctrl->is_ois_calib);

		if (o_ctrl->is_ois_calib) {
			rc = cam_ois_apply_settings(o_ctrl,
				&o_ctrl->i2c_calib_data);
			if (rc) {
				CAM_ERR(CAM_OIS, "Cannot apply calib data");
				goto pwr_dwn;
			}
		}else {
      cam_ois_calidata_download(o_ctrl);
		}
#endif
/*[bug-fix]-mod-end,by jinghuang  task 9049013 on 20200408*/

		rc = delete_request(&o_ctrl->i2c_init_data);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Fail deleting Init data: rc: %d", rc);
			rc = 0;
		}
		rc = delete_request(&o_ctrl->i2c_calib_data);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Fail deleting Calibration data: rc: %d", rc);
			rc = 0;
		}
		break;
	case CAM_OIS_PACKET_OPCODE_OIS_CONTROL:
		if (o_ctrl->cam_ois_state < CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
				"Not in right state to control OIS: %d",
				o_ctrl->cam_ois_state);
			return rc;
		}
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		i2c_reg_settings = &(o_ctrl->i2c_mode_data);
		i2c_reg_settings->is_settings_valid = 1;
		i2c_reg_settings->request_id = 0;
		rc = cam_sensor_i2c_command_parser(&o_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1, NULL);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS pkt parsing failed: %d", rc);
			return rc;
		}
/*[bug-fix]-mod-begin,by jinghuang	defect 9372059 9342038 on 20200518*/
/*add for custom control:EIS need disable ois*/
#if 1
		rc = cam_ois_apply_settings(o_ctrl, i2c_reg_settings);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "Cannot apply mode settings");
			return rc;
		}
#endif
/*[bug-fix]-mod-end,by jinghuang	defect 9372059 9342038 on 20200518*/
		rc = delete_request(i2c_reg_settings);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Fail deleting Mode data: rc: %d", rc);
			return rc;
		}
		break;
	case CAM_OIS_PACKET_OPCODE_READ: {
		struct cam_buf_io_cfg *io_cfg;
		struct i2c_settings_array i2c_read_settings;

		if (o_ctrl->cam_ois_state < CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
				"Not in right state to read OIS: %d",
				o_ctrl->cam_ois_state);
			return rc;
		}
		CAM_DBG(CAM_OIS, "number of I/O configs: %d:",
			csl_packet->num_io_configs);
		if (csl_packet->num_io_configs == 0) {
			CAM_ERR(CAM_OIS, "No I/O configs to process");
			rc = -EINVAL;
			return rc;
		}

		INIT_LIST_HEAD(&(i2c_read_settings.list_head));

		io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload +
			csl_packet->io_configs_offset);

		if (io_cfg == NULL) {
			CAM_ERR(CAM_OIS, "I/O config is invalid(NULL)");
			rc = -EINVAL;
			return rc;
		}

		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		i2c_read_settings.is_settings_valid = 1;
		i2c_read_settings.request_id = 0;
		rc = cam_sensor_i2c_command_parser(&o_ctrl->io_master_info,
			&i2c_read_settings,
			cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS read pkt parsing failed: %d", rc);
			return rc;
		}

		rc = cam_sensor_i2c_read_data(
			&i2c_read_settings,
			&o_ctrl->io_master_info);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "cannot read data rc: %d", rc);
			delete_request(&i2c_read_settings);
			return rc;
		}

		rc = delete_request(&i2c_read_settings);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Failed in deleting the read settings");
			return rc;
		}
		break;
	}
	default:
		CAM_ERR(CAM_OIS, "Invalid Opcode: %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		return -EINVAL;
	}

	if (!rc)
		return rc;
pwr_dwn:
	cam_ois_power_down(o_ctrl);
	return rc;
}

void cam_ois_shutdown(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	struct cam_ois_soc_private *soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	if (o_ctrl->cam_ois_state == CAM_OIS_INIT)
		return;

	if (o_ctrl->cam_ois_state >= CAM_OIS_CONFIG) {
		rc = cam_ois_power_down(o_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "OIS Power down failed");
		o_ctrl->cam_ois_state = CAM_OIS_ACQUIRE;
	}

	if (o_ctrl->cam_ois_state >= CAM_OIS_ACQUIRE) {
		rc = cam_destroy_device_hdl(o_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "destroying the device hdl");
		o_ctrl->bridge_intf.device_hdl = -1;
		o_ctrl->bridge_intf.link_hdl = -1;
		o_ctrl->bridge_intf.session_hdl = -1;
	}

	if (o_ctrl->i2c_mode_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_mode_data);

	if (o_ctrl->i2c_calib_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_calib_data);

	if (o_ctrl->i2c_init_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_init_data);

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;

	o_ctrl->cam_ois_state = CAM_OIS_INIT;
}

/**
 * cam_ois_driver_cmd - Handle ois cmds
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int cam_ois_driver_cmd(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int                              rc = 0;
	struct cam_ois_query_cap_t       ois_cap = {0};
	struct cam_control              *cmd = (struct cam_control *)arg;
	struct cam_ois_soc_private      *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

	if (!o_ctrl || !cmd) {
		CAM_ERR(CAM_OIS, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_OIS, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	mutex_lock(&(o_ctrl->ois_mutex));
	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		ois_cap.slot_info = o_ctrl->soc_info.index;

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&ois_cap,
			sizeof(struct cam_ois_query_cap_t))) {
			CAM_ERR(CAM_OIS, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		CAM_DBG(CAM_OIS, "ois_cap: ID: %d", ois_cap.slot_info);
		break;
	case CAM_ACQUIRE_DEV:
		rc = cam_ois_get_dev_handle(o_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_OIS, "Failed to acquire dev");
			goto release_mutex;
		}

		o_ctrl->cam_ois_state = CAM_OIS_ACQUIRE;
		break;
	case CAM_START_DEV:
		if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
			"Not in right state for start : %d",
			o_ctrl->cam_ois_state);
			goto release_mutex;
		}
		o_ctrl->cam_ois_state = CAM_OIS_START;
		break;
	case CAM_CONFIG_DEV:
		rc = cam_ois_pkt_parse(o_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_OIS, "Failed in ois pkt Parsing");
			goto release_mutex;
		}
		break;
	case CAM_RELEASE_DEV:
		if (o_ctrl->cam_ois_state == CAM_OIS_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
				"Cant release ois: in start state");
			goto release_mutex;
		}

		if (o_ctrl->cam_ois_state == CAM_OIS_CONFIG) {
			rc = cam_ois_power_down(o_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "OIS Power down failed");
				goto release_mutex;
			}
		}

		if (o_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_OIS, "link hdl: %d device hdl: %d",
				o_ctrl->bridge_intf.device_hdl,
				o_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(o_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "destroying the device hdl");
		o_ctrl->bridge_intf.device_hdl = -1;
		o_ctrl->bridge_intf.link_hdl = -1;
		o_ctrl->bridge_intf.session_hdl = -1;
		o_ctrl->cam_ois_state = CAM_OIS_INIT;

		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_down_setting_size = 0;
		power_info->power_setting_size = 0;

		if (o_ctrl->i2c_mode_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_mode_data);

		if (o_ctrl->i2c_calib_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_calib_data);

		if (o_ctrl->i2c_init_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_init_data);

		break;
	case CAM_STOP_DEV:
		if (o_ctrl->cam_ois_state != CAM_OIS_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
			"Not in right state for stop : %d",
			o_ctrl->cam_ois_state);
		}
		o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
		break;
	default:
		CAM_ERR(CAM_OIS, "invalid opcode");
		goto release_mutex;
	}
release_mutex:
	mutex_unlock(&(o_ctrl->ois_mutex));
	return rc;
}

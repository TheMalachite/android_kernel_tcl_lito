/**
 *  History 1.0 added by hailong.chen for task 7787483 on 2019-05-23
 *  History 2.0 added by hailong.chen for task 7805046 on 2019-05-25
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/module.h>

struct class *deviceinfo_class = NULL;
static DEFINE_MUTEX(devicelock);
static struct device *deviceinfo_dev = NULL;
struct device* get_deviceinfo_dev(void)
{

	mutex_lock(&devicelock);
	printk("the funciton get_deviceinfo_dev :mutex_lock(&devicelock)!");
	if(!deviceinfo_dev){
		if(!deviceinfo_class){
			deviceinfo_class = class_create(THIS_MODULE, "deviceinfo");
			if (IS_ERR(deviceinfo_class))
			{
				pr_err("Failed to create class(deviceinfo)!\n");
				goto get_info_err;
			}
		}
		deviceinfo_dev = device_create(deviceinfo_class,NULL, 0, NULL, "device_info");
		if (IS_ERR(deviceinfo_dev))
		{
			pr_err("Failed to create device(device_info)!\n");
			goto get_dev_err;
		}
	}

	mutex_unlock(&devicelock);
	return deviceinfo_dev;

get_dev_err:
	class_destroy(deviceinfo_class);
get_info_err:
	mutex_unlock(&devicelock);
	return NULL;

}
EXPORT_SYMBOL_GPL(get_deviceinfo_dev);

#define DEVICEINFO_TAG          "DEVICEINFO"
#define DEVICEINFO_LOG_ERR(fmt, args...) \
	pr_err("%s %s: "fmt, DEVICEINFO_TAG, __func__, ##args)
#define DEVICEINFO_FUN          pr_err(DEVICEINFO_TAG "%s\n", __func__)

#define DEVICEINFO_FUNC(sensor) \
char sensor##_module_name[256] = "NA:NA:NA"; \
EXPORT_SYMBOL(sensor##_module_name); \
static ssize_t sensor##_deviceinfo_show(struct device *dev, \
    struct device_attribute *attr, char *buf) { \
	if (!dev) { \
		DEVICEINFO_LOG_ERR("dev is NULL\n"); \
		return 0; \
	} \
	DEVICEINFO_LOG_ERR("%s \n", sensor##_module_name); \
	return snprintf(buf, PAGE_SIZE, "%s\n", sensor##_module_name); \
} \
static ssize_t sensor##_deviceinfo_store(struct device* dev, \
    struct device_attribute *attr, const char *buf, size_t count) { \
    snprintf(sensor##_module_name, count, "%s", buf);\
	return count; \
} \
DEVICE_ATTR(sensor, S_IWUSR | S_IRUGO, sensor##_deviceinfo_show, sensor##_deviceinfo_store); \
void Create_##sensor##_node_ForMMI(void) { \
	struct device * sensor; \
	sensor = get_deviceinfo_dev(); \
	if (device_create_file(sensor, &dev_attr_##sensor) < 0) \
		DEVICEINFO_LOG_ERR("Failed to create device file(%s)!\n", dev_attr_##sensor.attr.name); \
	return; \
}

/*Begin jiwu.haung for [Task][9781245] add new camera sensor and otp node info for mmi test on 20200812*/
#define CAMERAOTP_FUNC(sensor) \
char sensor##_module_name[256] = "NA:NA:NA"; \
EXPORT_SYMBOL(sensor##_module_name); \
static ssize_t sensor##_deviceinfo_show(struct device *dev, \
    struct device_attribute *attr, char *buf) { \
	if (!dev) { \
		DEVICEINFO_LOG_ERR("dev is NULL\n"); \
		return 0; \
	} \
	DEVICEINFO_LOG_ERR("%s \n", sensor##_module_name); \
	return snprintf(buf, PAGE_SIZE, "%s\n", sensor##_module_name); \
} \
static ssize_t sensor##_deviceinfo_store(struct device* dev, \
    struct device_attribute *attr, const char *buf, size_t count) { \
	DEVICEINFO_LOG_ERR("%s \n", buf); \
	strcpy(sensor##_module_name,buf);\
	return count; \
} \
DEVICE_ATTR(sensor, S_IWUSR | S_IRUGO, sensor##_deviceinfo_show, sensor##_deviceinfo_store); \
void Create_##sensor##_node_ForMMI(void) { \
	struct device * sensor; \
	sensor = get_deviceinfo_dev(); \
	if (device_create_file(sensor, &dev_attr_##sensor) < 0) \
		DEVICEINFO_LOG_ERR("Failed to create device file(%s)!\n", dev_attr_##sensor.attr.name); \
	return; \
}
/*End   jiwu.haung for [Task][9781245] add new camera sensor and otp node info for mmi test on 20200812*/

#if 0
DEVICEINFO_FUNC(gsensor)
DEVICEINFO_FUNC(compass)
DEVICEINFO_FUNC(gyroscope)
DEVICEINFO_FUNC(psensor)
DEVICEINFO_FUNC(lsensor)

//Begin modified by yaoting.wei for task 7955428 on 2019/06/26
DEVICEINFO_FUNC(wifi)
DEVICEINFO_FUNC(gps)
DEVICEINFO_FUNC(bluetooth)
DEVICEINFO_FUNC(FM)
//End modified by yaoting.wei for task 7955428 on 2019/06/26

//Begin add by xiongbo.huang for task 8189432 on 2019/08/03
DEVICEINFO_FUNC(LCM)
//end add by xiongbo.huang for task 8189432 on 2019/08/03

#endif
DEVICEINFO_FUNC(ctp)
DEVICEINFO_FUNC(sub_ctp)
//Begin add by yuduan.xie for task 8177729 on 2019/08/07
DEVICEINFO_FUNC(fp)
//End add by yuduan.xie for task 8177729 on 2019/08/07
DEVICEINFO_FUNC(iris)
DEVICEINFO_FUNC(lcd)

/*Begin jiwu.haung for [Task][10017385] add camera and otp node for mmi test on 20201003*/
DEVICEINFO_FUNC(CamNameB)
DEVICEINFO_FUNC(CamNameB2)
DEVICEINFO_FUNC(CamNameB3)
DEVICEINFO_FUNC(CamNameB4)
DEVICEINFO_FUNC(CamNameF)
CAMERAOTP_FUNC(CamOTPB)
CAMERAOTP_FUNC(CamOTPF)
CAMERAOTP_FUNC(CamOTPB2)
CAMERAOTP_FUNC(CamOTPB3)
CAMERAOTP_FUNC(CamOTPB4)
/*End   jiwu.haung for [Task][10017385] add camera and otp node for mmi test on 20201003*/
//Begin add by na.long for PR10560314 on 2020/12/24
DEVICEINFO_FUNC(NFC)
//End add by na.long for PR10560314 on 2020/12/24


//Begin add by dingting.meng for IRVR-3724 on 2021.03.12
DEVICEINFO_FUNC(eMMC)
DEVICEINFO_FUNC(CPU)
//End add by dingting.meng for IRVR-3724 on 2021.03.12

static int __init deviceinfo_init(void) {
	DEVICEINFO_FUN;

	//Create_gsensor_node_ForMMI();
	//Create_compass_node_ForMMI();
	//Create_gyroscope_node_ForMMI();
	//Create_psensor_node_ForMMI();
	//Create_lsensor_node_ForMMI();
//Begin add by xiongbo.huang for task 8189432 on 2019/08/03
	Create_lcd_node_ForMMI();
	Create_ctp_node_ForMMI();
	Create_sub_ctp_node_ForMMI();
	Create_iris_node_ForMMI();
//end add by xiongbo.huang for task 8189432 on 2019/08/03

//Begin modified by yaoting.wei for task 8209902 on 2019/08/06
	//sprintf(wifi_module_name, "MT6631:MediaTek:0.1");
	//sprintf(gps_module_name, "MT6631:MediaTek:Default");
	//sprintf(bluetooth_module_name, "MT6631:MediaTek:Default");
	//sprintf(FM_module_name, "MT6631:MediaTek:Default");

	//Create_wifi_node_ForMMI();
	//Create_gps_node_ForMMI();
	//Create_bluetooth_node_ForMMI();
	//Create_FM_node_ForMMI();
//End modified by yaoting.wei for task 8209902 on 2019/08/06

	//Begin add by yuduan.xie for task 8177729 on 2019/08/07
	Create_fp_node_ForMMI();
	//End add by yuduan.xie for task 8177729 on 2019/08/07

/*Begin jiwu.haung for [Task][10017385] add camera and otp node for mmi test on 20201003*/
	Create_CamNameB_node_ForMMI();
	Create_CamNameB2_node_ForMMI();
	Create_CamNameB3_node_ForMMI();
	Create_CamNameB4_node_ForMMI();
	Create_CamNameF_node_ForMMI();
	Create_CamOTPB_node_ForMMI();
	Create_CamOTPB2_node_ForMMI();
	Create_CamOTPB3_node_ForMMI();
	Create_CamOTPB4_node_ForMMI();
	Create_CamOTPF_node_ForMMI();
/*End   jiwu.haung for [Task][10017385] add camera and otp node for mmi test on 20201003*/
	//Begin add by na.long for PR10560314 on 2020/12/24
    Create_NFC_node_ForMMI();
	//End add by na.long for PR10560314 on 2020/12/24

    //Begin add by dingting.meng for IRVR-3724 on 2021.03.12
    Create_eMMC_node_ForMMI();
    Create_CPU_node_ForMMI();
    //End add by dingting.meng for IRVR-3724 on 2021.03.12

	return 0;
}
/*----------------------------------------------------------------------------*/

static void __exit deviceinfo_exit(void) {
	DEVICEINFO_FUN;
}
/*----------------------------------------------------------------------------*/
module_init(deviceinfo_init);
module_exit(deviceinfo_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("hailong.chen");
MODULE_DESCRIPTION("deviceinfo driver");
MODULE_LICENSE("GPL");

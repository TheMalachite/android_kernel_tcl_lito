/**
 *  History 1.0 added by hailong.chen for task 7805046 on 2019-05-25
 */

#include <linux/device.h>
#include <linux/module.h>

typedef struct robust_node_info {
	char* dir;
	char* node;
	ssize_t (*show)(struct device *, struct device_attribute *, char *);
	ssize_t (*store)(struct device*, struct device_attribute *, const char *, size_t);
}robust_node_info;

#define ROBUST_DEV_NAME     "ROBUST"
#define ROBUST_TAG          "ROBUST"
#define ROBUST_LOG_ERR(fmt, args...) \
	pr_err("%s %s: "fmt, ROBUST_TAG, __func__, ##args)
#define ROBUST_FUN          pr_err(ROBUST_TAG "%s\n", __func__)

#define MAX_ROBUST_NODE_NUM 9
#define I2C_CHECK_DEVNAME "i2c_check"

#define ROBUST_FUNC(sensor, init) \
int sensor##_load_status = init; \
EXPORT_SYMBOL(sensor##_load_status); \
static ssize_t sensor##_status_show(struct device *dev, \
    struct device_attribute *attr, char *buf) { \
	int status=0; \
	if(1 == sensor##_load_status) \
		status = 1; \
	if (!dev) { \
		ROBUST_LOG_ERR("dev is NULL\n"); \
		return 0; \
	} \
	ROBUST_LOG_ERR("%d \n", status); \
	return snprintf(buf, PAGE_SIZE, "%d\n", status); \
} \
static ssize_t sensor##_status_store(struct device* dev, \
    struct device_attribute *attr, const char *buf, size_t count) { \
	return count; \
}

ROBUST_FUNC(accelerometer, 0)
ROBUST_FUNC(magnetometer, 0)
ROBUST_FUNC(gyroscope, 0)
ROBUST_FUNC(proximity, 0)
ROBUST_FUNC(light, 0)
ROBUST_FUNC(led, 0)
ROBUST_FUNC(smartpa, 0)
ROBUST_FUNC(hifi, 0)

extern int tpd_load_status;
static ssize_t touch_panel_status_show(struct device *dev,
    struct device_attribute *attr, char *buf) {
	if (!dev ) {
		return 0;
	}

	ROBUST_LOG_ERR("%d \n", tpd_load_status);
	return snprintf(buf, PAGE_SIZE, "%d\n", tpd_load_status);
}

static ssize_t touch_panel_status_store(struct device* dev,
    struct device_attribute *attr, const char *buf, size_t count) {
	return count;
}

static const robust_node_info robust_node[MAX_ROBUST_NODE_NUM] = {
	{"AccelerometerSensor", "status", accelerometer_status_show , accelerometer_status_store},
	{"MagnetometerSensor", "status", magnetometer_status_show , magnetometer_status_store},
	{"GyroscopeSensor", "status", gyroscope_status_show , gyroscope_status_store},
	{"ProximitySensor", "status", proximity_status_show , proximity_status_store},
	{"LightSensor", "status", light_status_show , light_status_store},
	{"indicator_led", "status", led_status_show , led_status_store},
	{"audio_smpart_pa", "status", smartpa_status_show , smartpa_status_store},
	{"audio_hifi", "status", hifi_status_show , hifi_status_store},
	{"touch_panel", "status", touch_panel_status_show , touch_panel_status_store},
};

struct device_attribute dev_attr[MAX_ROBUST_NODE_NUM];
static const struct file_operations cdevOper = {
	.owner = THIS_MODULE,
};

static int __init robust_init(void) {
	int i = 0, retval = 0;
	struct class *i2cdev_class = NULL;
	struct device *i2cdev_dev = NULL;

	ROBUST_FUN;

	i2cdev_class = class_create(THIS_MODULE, I2C_CHECK_DEVNAME);
	if (IS_ERR(i2cdev_class)){
		ROBUST_LOG_ERR("Failed to create class(sys/class/i2c_check/)!\n");
		return 0;
	}

	for(i = 0 ; i < ARRAY_SIZE(robust_node); i++){
		i2cdev_dev = device_create(i2cdev_class, NULL, 0, NULL, robust_node[i].dir);
		if (IS_ERR(i2cdev_dev)){
			ROBUST_LOG_ERR("Failed to create device(sys/class/i2c_check/%s)!\n", robust_node[i].dir);
			retval = PTR_ERR(i2cdev_dev);
			break;
		}

		dev_attr[i].attr.name = robust_node[i].node;
		dev_attr[i].attr.mode = S_IWUSR | S_IRUGO;
		dev_attr[i].show = robust_node[i].show;
		dev_attr[i].store = robust_node[i].store;
		if(device_create_file(i2cdev_dev, &dev_attr[i]) < 0){
			ROBUST_LOG_ERR("Failed to create device file(sys/class/i2c_check/%s/%s)!\n",
			    robust_node[i].dir, robust_node[i].node);
			retval = -1;
			break;
		}
	}	

	return retval;
}
/*----------------------------------------------------------------------------*/

static void __exit robust_exit(void) {
	ROBUST_FUN;
}
/*----------------------------------------------------------------------------*/
module_init(robust_init);
module_exit(robust_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("ersen.shang");
MODULE_DESCRIPTION("robust driver");
MODULE_LICENSE("GPL");

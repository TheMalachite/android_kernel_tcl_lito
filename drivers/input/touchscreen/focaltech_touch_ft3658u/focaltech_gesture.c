/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_gestrue.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"

/******************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define KEY_GESTURE_U                           249
#define KEY_GESTURE_FOD                         251
#define KEY_GESTURE_SINGLECLICK                 250
#define KEY_GESTURE_PALMIN                      238
#define KEY_GESTURE_PALMOUT                     239
#define KEY_GESTURE_UP                          KEY_UP
#define KEY_GESTURE_DOWN                        KEY_DOWN
#define KEY_GESTURE_LEFT                        KEY_LEFT
#define KEY_GESTURE_RIGHT                       KEY_RIGHT
#define KEY_GESTURE_O                           KEY_O
#define KEY_GESTURE_E                           KEY_E
#define KEY_GESTURE_M                           KEY_M
#define KEY_GESTURE_L                           KEY_L
#define KEY_GESTURE_W                           KEY_W
#define KEY_GESTURE_S                           KEY_S
#define KEY_GESTURE_V                           KEY_V
#define KEY_GESTURE_C                           KEY_C
#define KEY_GESTURE_Z                           KEY_Z

#define GESTURE_LEFT                            0x20
#define GESTURE_RIGHT                           0x21
#define GESTURE_UP                              0x22
#define GESTURE_DOWN                            0x23
#define GESTURE_DOUBLECLICK                     0x24
#define GESTURE_O                               0x30
#define GESTURE_W                               0x31
#define GESTURE_M                               0x32
#define GESTURE_E                               0x33
#define GESTURE_L                               0x44
#define GESTURE_S                               0x46
#define GESTURE_V                               0x54
#define GESTURE_Z                               0x41
#define GESTURE_C                               0x34
#define GESTURE_FOD                             0x26
#define GESTURE_SINGLECLICK                     0x25
#define GESTURE_PALMIN                          0x01
#define GESTURE_PALMOUT                         0x02

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/*
* gesture_id    - mean which gesture is recognised
* point_num     - points number of this gesture
* coordinate_x  - All gesture point x coordinate
* coordinate_y  - All gesture point y coordinate
* mode          - gesture enable/disable, need enable by host
*               - 1:enable gesture function(default)  0:disable
* active        - gesture work flag,
*                 always set 1 when suspend, set 0 when resume
*/
struct fts_gesture_st {
    u8 gesture_id;
    u8 point_num;
    u16 coordinate_x[FTS_GESTURE_POINTS_MAX];
    u16 coordinate_y[FTS_GESTURE_POINTS_MAX];
};

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct fts_gesture_st fts_gesture_data;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static ssize_t fts_gesture_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val[2] = {0};
    struct fts_ts_data *ts_data = fts_data;

    mutex_lock(&ts_data->input_dev->mutex);
    fts_read_reg(FTS_REG_GESTURE_EN, &val[0]);
    fts_read_reg(FTS_REG_GESTURE_MODE, &val[1]);
    count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
                     ts_data->gesture_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d Reg(0xCF)=%d\n", val[0], val[1]);
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

#ifdef CONFIG_TCT_LITO_CHICAGO
extern bool bt541_gesture_enable;
#endif
static ssize_t fts_gesture_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct fts_ts_data *ts_data = fts_data;

    mutex_lock(&ts_data->input_dev->mutex);
    if (FTS_SYSFS_ECHO_ON(buf)) {
        FTS_DEBUG("enable gesture");
        ts_data->gesture_mode = ENABLE;
#ifdef CONFIG_TCT_LITO_CHICAGO
        bt541_gesture_enable = true;
#endif
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        FTS_DEBUG("disable gesture");
        ts_data->gesture_mode = DISABLE;
#ifdef CONFIG_TCT_LITO_CHICAGO
        bt541_gesture_enable = false;
#endif
    }
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_fod_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val[2] = {0};
    struct fts_ts_data *ts_data = fts_data;

    mutex_lock(&ts_data->input_dev->mutex);
    fts_read_reg(FTS_REG_GESTURE_EN, &val[0]);
    fts_read_reg(FTS_REG_GESTURE_MODE, &val[1]);
    count = snprintf(buf, PAGE_SIZE, "Fod Mode:%s\n",
                     ts_data->fod_mode ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d, Reg(0xCF)=%d\n", val[0], val[1]);
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_fod_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct fts_ts_data *ts_data = fts_data;

    mutex_lock(&ts_data->input_dev->mutex);
    if (FTS_SYSFS_ECHO_ON(buf)) {
        FTS_DEBUG("enable fod");
        ts_data->fod_mode = ENABLE;
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        FTS_DEBUG("disable fod");
        ts_data->fod_mode = DISABLE;
    }
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    int i = 0;
    struct input_dev *input_dev = fts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    mutex_lock(&input_dev->mutex);
    count = snprintf(buf, PAGE_SIZE, "Gesture ID:%d\n", gesture->gesture_id);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture PointNum:%d\n",
                      gesture->point_num);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture Points Buffer:\n");

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        count += snprintf(buf + count, PAGE_SIZE, "%3d(%4d,%4d) ", i,
                          gesture->coordinate_x[i], gesture->coordinate_y[i]);
        if ((i + 1) % 4 == 0)
            count += snprintf(buf + count, PAGE_SIZE, "\n");
    }
    count += snprintf(buf + count, PAGE_SIZE, "\n");
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}


/* sysfs gesture node
 *   read example: cat  fts_gesture_mode       ---read gesture mode
 *   write example:echo 1 > fts_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR(fts_gesture_mode, S_IRUGO | S_IWUSR, fts_gesture_show,
                   fts_gesture_store);

static DEVICE_ATTR(double_wakeup_enable, S_IRUGO | S_IWUSR, fts_gesture_show,
                   fts_gesture_store);

static DEVICE_ATTR(fod_enable, S_IRUGO | S_IWUSR, fts_fod_show,
                   fts_fod_store);
/*
 *   read example: cat fts_gesture_buf        --- read gesture buf
 */
static DEVICE_ATTR(fts_gesture_buf, S_IRUGO | S_IWUSR,
                   fts_gesture_buf_show, fts_gesture_buf_store);

static struct attribute *fts_gesture_mode_attrs[] = {
    &dev_attr_fts_gesture_mode.attr,
    &dev_attr_fts_gesture_buf.attr,
    NULL,
};

static struct attribute_group fts_gesture_group = {
    .attrs = fts_gesture_mode_attrs,
};

static int fts_create_gesture_sysfs(struct device *dev)
{
    int ret = 0;

    ret = sysfs_create_group(&dev->kobj, &fts_gesture_group);
    if (ret) {
        FTS_ERROR("gesture sys node create fail");
        sysfs_remove_group(&dev->kobj, &fts_gesture_group);
        return ret;
    }

    return 0;
}

static void fts_gesture_report(struct input_dev *input_dev, int gesture_id)
{
    int gesture;

    FTS_DEBUG("gesture_id:0x%x", gesture_id);
    switch (gesture_id) {
    case GESTURE_LEFT:
        gesture = KEY_GESTURE_LEFT;
        break;
    case GESTURE_RIGHT:
        gesture = KEY_GESTURE_RIGHT;
        break;
    case GESTURE_UP:
        gesture = KEY_GESTURE_UP;
        break;
    case GESTURE_DOWN:
        gesture = KEY_GESTURE_DOWN;
        break;
    case GESTURE_DOUBLECLICK:
        gesture = KEY_GESTURE_U;
        break;
    case GESTURE_O:
        gesture = KEY_GESTURE_O;
        break;
    case GESTURE_W:
        gesture = KEY_GESTURE_W;
        break;
    case GESTURE_M:
        gesture = KEY_GESTURE_M;
        break;
    case GESTURE_E:
        gesture = KEY_GESTURE_E;
        break;
    case GESTURE_L:
        gesture = KEY_GESTURE_L;
        break;
    case GESTURE_S:
        gesture = KEY_GESTURE_S;
        break;
    case GESTURE_V:
        gesture = KEY_GESTURE_V;
        break;
    case GESTURE_Z:
        gesture = KEY_GESTURE_Z;
        break;
    case  GESTURE_C:
        gesture = KEY_GESTURE_C;
        break;
    case  GESTURE_SINGLECLICK:
        gesture = KEY_GESTURE_SINGLECLICK;
        break;
    case  GESTURE_PALMIN:
        gesture = KEY_GESTURE_PALMIN;
        break;
    case  GESTURE_PALMOUT:
        gesture = KEY_GESTURE_PALMOUT;
        break;
    default:
        gesture = -1;
        break;
    }
    /* report event key */
    if (gesture != -1) {
        FTS_DEBUG("Gesture Code=%d", gesture);
        input_report_key(input_dev, gesture, 1);
        input_sync(input_dev);
        input_report_key(input_dev, gesture, 0);
        input_sync(input_dev);
    }
}

/*****************************************************************************
* Name: fts_gesture_readdata
* Brief: Read information about gesture: enable flag/gesture points..., if ges-
*        ture enable, save gesture points' information, and report to OS.
*        It will be called this function every intrrupt when FTS_GESTURE_EN = 1
*
*        gesture data length: 1(enable) + 1(reserve) + 2(header) + 6 * 4
* Input: ts_data - global struct data
*        data    - gesture data buffer if non-flash, else NULL
* Output:
* Return: 0 - read gesture data successfully, the report data is gesture data
*         1 - tp not in suspend/gesture not enable in TP FW
*         -Exx - error
*****************************************************************************/
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data)
{
    int ret = 0;
    int i = 0;
    int index = 0;
    u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
    struct input_dev *input_dev = ts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    if (!ts_data->suspended || !ts_data->gesture_mode) {
        return 1;
    }


    ret = fts_read_reg(FTS_REG_GESTURE_EN, &buf[0]);
    if ((ret < 0) || (buf[0] != ENABLE)) {
        FTS_DEBUG("gesture not enable in fw, don't process gesture");
        return 1;
    }

    buf[2] = FTS_REG_GESTURE_OUTPUT_ADDRESS;
    ret = fts_read(&buf[2], 1, &buf[2], FTS_GESTURE_DATA_LEN - 2);
    if (ret < 0) {
        FTS_ERROR("read gesture header data fail");
        return ret;
    }

    /* init variable before read gesture point */
    memset(gesture->coordinate_x, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    memset(gesture->coordinate_y, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    gesture->gesture_id = buf[2];
    gesture->point_num = buf[3];
    FTS_DEBUG("gesture_id=%d, point_num=%d",
              gesture->gesture_id, gesture->point_num);

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        index = 4 * i + 4;
        gesture->coordinate_x[i] = (u16)(((buf[0 + index] & 0x0F) << 8)
                                         + buf[1 + index]);
        gesture->coordinate_y[i] = (u16)(((buf[2 + index] & 0x0F) << 8)
                                         + buf[3 + index]);
    }

    /* report gesture to OS */
    fts_gesture_report(input_dev, gesture->gesture_id);
    return 0;
}

int tct_fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data)
{
    int ret = 0, i = 0;
    u8 buf[9] = { 0 };
    u8 cmd = 0;
    u8 state = 0xFF;
    struct input_dev *input_dev = ts_data->input_dev;

    if ((!ts_data->suspended && !ts_data->fod_pressed) || (!ts_data->gesture_mode && !ts_data->fod_mode)) {
        return 1;
    }

    ret = fts_read_reg(FTS_REG_GESTURE_EN, &buf[0]);
    if ((ret < 0) || (buf[0] != ENABLE)) {
        FTS_DEBUG("gesture not enable in fw, don't process gesture");
        return 1;
    }

    cmd = 0xE1;
    ret = fts_read(&cmd, 1, &buf[0], 9);
    if (ret < 0) {
        FTS_ERROR("read gesture header data fail");
        return ret;
    }

    FTS_INFO("gesture data %x %x %x %x %x %x %x %x %x", 
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

    if (buf[1] == GESTURE_DOUBLECLICK) {
        fts_gesture_report(input_dev, GESTURE_DOUBLECLICK);
    } else if (buf[1] == GESTURE_SINGLECLICK) {
        fts_gesture_report(input_dev, GESTURE_SINGLECLICK);
    } else if (buf[1] == GESTURE_FOD) {
        if (buf[8] == 0) {
            input_report_key(input_dev, KEY_GESTURE_FOD, 1);
            input_sync(input_dev);
            ts_data->fod_pressed = 1;
        } else {
            input_report_key(input_dev, KEY_GESTURE_FOD, 0);
            input_sync(input_dev);
            ts_data->fod_pressed = 0;
            // if suspend is off or tp is in resuming, fod up, close gesture
            if (ts_data->suspended == false || ts_data->is_resuming == true) {
                for (i = 0; i < 5; i++) {
                    fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
                    msleep(1);
                    fts_read_reg(FTS_REG_GESTURE_EN, &state);
                    if (state == DISABLE)
                        break;
                }
                if (i >= 5)
                    FTS_ERROR("make IC exit gesture(resume) fail,state:%x", state);
                else
                    FTS_INFO("resume from gesture successfully");
            }

        }
    } else {
        FTS_ERROR("unkown gesture id:%d", buf[0]);
    }

    return 0;
}

void tct_fts_palm_report(struct fts_ts_data *ts_data, u8 data)
{
    int palm_state;
    struct input_dev *input_dev = ts_data->input_dev;

    palm_state = data >> 6;

    if (palm_state == 1) {
        fts_gesture_report(input_dev, 0x01);
    } else if (palm_state == 2) {
        fts_gesture_report(input_dev, 0x02);
    }
}

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
    u8 cmd_data = 0;

    if ((ts_data->gesture_mode || ts_data->fod_mode)&& ts_data->suspended) {
        FTS_DEBUG("gesture recovery...");
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
    }
    if (ts_data->gesture_mode)
        cmd_data |= 0x01;
    if (ts_data->fod_mode)
        cmd_data |= 0x06;

    fts_write_reg(FTS_REG_GESTURE_MODE, cmd_data);
}

int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
    int i = 0;
    u8 state = 0xFF;
    u8 cmd_data = 0;

    FTS_FUNC_ENTER();
    if (enable_irq_wake(ts_data->irq)) {
        FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);
    }

    for (i = 0; i < 5; i++) {
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
        msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state == ENABLE)
            break;
    }

    if (i >= 5)
        FTS_ERROR("make IC enter into gesture(suspend) fail,state:%x", state);
    else
        FTS_INFO("Enter into gesture(suspend) successfully");

    if (ts_data->gesture_mode)
        cmd_data |= 0x01;
    if (ts_data->fod_mode)
        cmd_data |= 0x06;

    fts_write_reg(FTS_REG_GESTURE_MODE, cmd_data);
    msleep(1);
    fts_read_reg(FTS_REG_GESTURE_MODE, &state);
    FTS_INFO("FTS_REG_GESTURE_MODE: 0x%x", state);

    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
    int i = 0;
    u8 state = 0xFF;

    FTS_FUNC_ENTER();
    if (disable_irq_wake(ts_data->irq)) {
        FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);
    }

    if (ts_data->fod_pressed)
        goto exit;

    for (i = 0; i < 5; i++) {
        fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
        msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state == DISABLE)
            break;
    }

    if (i >= 5)
        FTS_ERROR("make IC exit gesture(resume) fail,state:%x", state);
    else
        FTS_INFO("resume from gesture successfully");

exit:
    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_init(struct fts_ts_data *ts_data)
{
    struct input_dev *input_dev = ts_data->input_dev;
    int error;

    FTS_FUNC_ENTER();
    input_set_capability(input_dev, EV_KEY, KEY_POWER);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_FOD);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_SINGLECLICK);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_PALMIN);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_PALMOUT);

    __set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
    __set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
    __set_bit(KEY_GESTURE_UP, input_dev->keybit);
    __set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
    __set_bit(KEY_GESTURE_U, input_dev->keybit);
    __set_bit(KEY_GESTURE_O, input_dev->keybit);
    __set_bit(KEY_GESTURE_E, input_dev->keybit);
    __set_bit(KEY_GESTURE_M, input_dev->keybit);
    __set_bit(KEY_GESTURE_W, input_dev->keybit);
    __set_bit(KEY_GESTURE_L, input_dev->keybit);
    __set_bit(KEY_GESTURE_S, input_dev->keybit);
    __set_bit(KEY_GESTURE_V, input_dev->keybit);
    __set_bit(KEY_GESTURE_C, input_dev->keybit);
    __set_bit(KEY_GESTURE_Z, input_dev->keybit);
    __set_bit(KEY_GESTURE_FOD, input_dev->keybit);
    __set_bit(KEY_GESTURE_SINGLECLICK, input_dev->keybit);
    __set_bit(KEY_GESTURE_PALMIN, input_dev->keybit);
    __set_bit(KEY_GESTURE_PALMOUT, input_dev->keybit);

    fts_create_gesture_sysfs(ts_data->dev);

    if (ts_data->tct_touch_class) {
		ts_data->tct_touch_gesture =
			device_create(ts_data->tct_touch_class, NULL, 0, NULL, "gesture");
		if (ts_data->tct_touch_gesture) {
			error = device_create_file(ts_data->tct_touch_gesture, &dev_attr_double_wakeup_enable);
			if (error < 0)
                FTS_DEBUG("Failed to create file double_wakeup_enable!");
		} else
            FTS_DEBUG("Failed to create tct_touch_gesture!");

        if (ts_data->tct_touch_dev) {
			error = device_create_file(ts_data->tct_touch_dev, &dev_attr_fod_enable);
			if (error < 0)
                FTS_DEBUG("Failed to create file fod_enable!");
		} else
			FTS_DEBUG("tct_touch_dev is not exit!\n");
    }

    memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
    ts_data->gesture_mode = FTS_GESTURE_EN;

    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_exit(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
    FTS_FUNC_EXIT();
    return 0;
}

/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Author: Andi Shyti <andi.shyti@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SPI driven IR LED device driver
 */
//#define DEBUG
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <media/rc-core.h>

#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#define IR_SPI_DRIVER_NAME		"ir-spi"
 
/* pulse value for different duty cycles */
#define IR_SPI_PULSE_DC_50		0xff00
#define IR_SPI_PULSE_DC_60		0xfc00
#define IR_SPI_PULSE_DC_70		0xf800
#define IR_SPI_PULSE_DC_75		0xf000
#define IR_SPI_PULSE_DC_80		0xc000
#define IR_SPI_PULSE_DC_90		0x8000

#define IR_SPI_DEFAULT_FREQUENCY	38000
#define IR_SPI_BIT_PER_WORD		    8
#define IR_SPI_MAX_BUFSIZE		0x2000

#define KOOKONG_DEVICE_NAME		"kookong"
struct ir_spi_data *kookong_rc_data;

struct ir_spi_data {
    u32 freq;
    u8 duty_cycle;
    bool negated;

    u16 tx_buf[IR_SPI_MAX_BUFSIZE];
    u16 pulse;
    u16 space;

    struct rc_dev *rc;
    struct spi_device *spi;
    struct regulator *regulator;
};

struct ir_spi_split_data {
    u32 freq;
    unsigned int start_h;
    unsigned int start_l;
    unsigned int end_t;
    u16 split_mode;
};

static int ir_spi_tx(struct rc_dev *dev,
             unsigned int *buffer, unsigned int count)
{
    int i;
    int ret;
    unsigned int len = 0;
    struct ir_spi_data *idata = dev->priv;
    struct spi_transfer xfer;

    /* convert the pulse/space signal to raw binary signal */
    for (i = 0; i < count; i++) {
        unsigned int periods;
        int j;
        u16 val;

        periods = DIV_ROUND_CLOSEST(buffer[i] * idata->freq, 1000000);

        if (len + periods >= IR_SPI_MAX_BUFSIZE)
            return -EINVAL;

        /*
         * the first value in buffer is a pulse, so that 0, 2, 4, ...
         * contain a pulse duration. On the contrary, 1, 3, 5, ...
         * contain a space duration.
         */
        val = (i % 2) ? idata->space : idata->pulse;
        for (j = 0; j < periods; j++)
            idata->tx_buf[len++] = val;
    }

    memset(&xfer, 0, sizeof(xfer));

    xfer.speed_hz = idata->freq * 16;
    xfer.len = len * sizeof(*idata->tx_buf);
    xfer.tx_buf = idata->tx_buf;

    ret = regulator_enable(idata->regulator);
    if (ret)
        return ret;

    ret = spi_sync_transfer(idata->spi, &xfer, 1);
    if (ret)
        dev_err(&idata->spi->dev, "unable to deliver the signal\n");

    regulator_disable(idata->regulator);

    return ret ? ret : count;
}

static int ir_spi_tx_by_package(struct rc_dev *dev,
             unsigned int *buffer, unsigned int count)
{
    int i;
    int ret;
    struct ir_spi_data *idata = dev->priv;
    unsigned int package_len[5] = {0},p_num = 0;
    unsigned int len_temp = 0;
    unsigned int data_len = 0;
    int *cmd_p,cmd_num = 0,sent_num = 0;

    /* convert the pulse/space signal to raw binary signal */
    for (i = 0; i < count; i++) {
        unsigned int periods;

        periods = DIV_ROUND_CLOSEST(buffer[i] * idata->freq, 1000000);

        len_temp += periods;
        if(buffer[i]>=10000)
        {
            pr_debug(KOOKONG_DEVICE_NAME" package i = %d len_temp = %d \n",i,len_temp);
            if(len_temp > IR_SPI_MAX_BUFSIZE)
            {
                pr_debug(KOOKONG_DEVICE_NAME" package p_num = %d  len = %d data_len = %d \n",p_num,package_len[p_num],data_len);
                p_num ++ ;
                len_temp = len_temp - data_len;
            }
            data_len = len_temp;
            package_len[p_num] = i+1;
        }
    }
    package_len[p_num] = i;
    p_num ++;

    pr_info(KOOKONG_DEVICE_NAME" p_num = %d count = %d \n",p_num,count);
    cmd_p = buffer;
    cmd_num = 0 ;
    for(i = 0; i < p_num; i++)
    {
        cmd_num = package_len[i] - sent_num;
        pr_debug(KOOKONG_DEVICE_NAME" cmd_p = %d cmd_num = %d \n",*cmd_p,cmd_num);
        ret = ir_spi_tx(dev,cmd_p,cmd_num);
        sent_num += cmd_num;
        cmd_p = &buffer[sent_num];

    }
    return ret ? ret : count;
}

static int ir_spi_set_tx_carrier(struct rc_dev *dev, u32 carrier)
{
    struct ir_spi_data *idata = dev->priv;

    if (!carrier)
        return -EINVAL;

    idata->freq = carrier;

    return 0;
}

static int ir_spi_set_duty_cycle(struct rc_dev *dev, u32 duty_cycle)
{
    struct ir_spi_data *idata = dev->priv;

    if (duty_cycle >= 90)
        idata->pulse = IR_SPI_PULSE_DC_90;
    else if (duty_cycle >= 80)
        idata->pulse = IR_SPI_PULSE_DC_80;
    else if (duty_cycle >= 75)
        idata->pulse = IR_SPI_PULSE_DC_75;
    else if (duty_cycle >= 70)
        idata->pulse = IR_SPI_PULSE_DC_70;
    else if (duty_cycle >= 60)
        idata->pulse = IR_SPI_PULSE_DC_60;
    else
        idata->pulse = IR_SPI_PULSE_DC_50;

    if (idata->negated) {
        idata->pulse = ~idata->pulse;
        idata->space = 0xffff;
    } else {
        idata->space = 0;
    }

    return 0;
}

static int kookong_spi_open(struct inode *inode, struct file *file) {
    return 0;
}

static int kookong_spi_close(struct inode *inode, struct file *file) {
    return 0;
}

struct ir_spi_split_data ir_special_code[]= {
    {38000, 9000,   4500,     0,  1},
    {37910, 8516,   4276,     0,  1},
/*  */
    {37910, 9010,      0,     0,  1},
    {37910, 9014,      0,     0,  1},
/*  */
    {38030,    0,      0, 90884,  2},
    {0,        0,      0,     0,  0},
};

struct ir_spi_split_data * ir_spi_special_data( u32 freq,unsigned int *buffer,unsigned int count)
{
    u8 i = 0;
    int j,end_cnt = 0;

    while(ir_special_code[i].freq)
    {
        if((freq == ir_special_code[i].freq)  && ir_special_code[i].start_h)
        {
            if(ir_special_code[i].start_h == buffer[0] && 
               ir_special_code[i].start_l == buffer[1])
               return  &ir_special_code[i];
            else if(ir_special_code[i].start_h == buffer[0] && 
               ir_special_code[i].start_l == 0)
               return  &ir_special_code[i];
        } else if((freq == ir_special_code[i].freq)  && 
                        ir_special_code[i].end_t){
            end_cnt = 0;
            for(j=0;j<count;j++)
            {
                if(buffer[j] == ir_special_code[i].end_t)
                    end_cnt++;
            }
            if(end_cnt)
            {
                pr_info(KOOKONG_DEVICE_NAME" end_cnt = %d : %d \n",end_cnt,ir_special_code[i].end_t);
                return  &ir_special_code[i]; 
            }
        }
        i++;
    }

    return NULL;

}

static int ir_spi_write_longcmd(struct rc_dev *dev,
             unsigned int *buffer, unsigned int count)
{
    struct ir_spi_data *idata = dev->priv;
    struct ir_spi_split_data *ir_code = NULL;
    int *cmd_p,cmd_num = 0;
    int ret ,i;

    ir_code = ir_spi_special_data(idata->freq,buffer,count);
    if(ir_code)
    {
        pr_info(KOOKONG_DEVICE_NAME" split_mode = %d \n",ir_code->split_mode);
        if(ir_code->split_mode == 1)
        {
            cmd_p = buffer;
            cmd_num = 1;
            for(i=1;i<count;i++)
            {
                if( buffer[i] == ir_code->start_h)
                {
                    if(((i+1)<count && buffer[i+1] == ir_code->start_l) ||
                    (ir_code->start_l == 0))
                    {
                        pr_debug(KOOKONG_DEVICE_NAME" cmd_num = %d : %d \n",cmd_num,*cmd_p);
                        ret = ir_spi_tx(dev,cmd_p,cmd_num);
                        if(ret <= 0)
                            return ret;

                        cmd_p = &buffer[i];
                        cmd_num = 0;
                    }
                }
                cmd_num++;
            }
            pr_debug(KOOKONG_DEVICE_NAME" S num = %d : %d \n",cmd_num,*cmd_p);
            ret = ir_spi_tx(dev,cmd_p,cmd_num);
            return ret;
        } else if (ir_code->split_mode == 2){
            cmd_p = buffer;
            cmd_num = 1;
            for(i=1;i<count;i++)
            {
                if(buffer[i-1] == ir_code->end_t)
                {
                        pr_debug(KOOKONG_DEVICE_NAME" cmd_num = %d : %d \n",cmd_num,*cmd_p);
                        ret = ir_spi_tx(dev,cmd_p,cmd_num);
                        if(ret <= 0)
                            return ret;

                        cmd_p = &buffer[i];
                        cmd_num = 0;
                }
                cmd_num++;
            }
            pr_debug(KOOKONG_DEVICE_NAME"  E num = %d : %d \n",cmd_num,*cmd_p);
            ret = ir_spi_tx(dev,cmd_p,cmd_num);
            return ret;
        }
    }

    ret = ir_spi_tx_by_package(dev,buffer,count);
    return ret;
}

static ssize_t kookong_spi_write(struct file *filp,const char __user *buf_user,size_t size,loff_t * offp) {
    int ret,i;
    static int buf[1024]={0};
    int count=0;
    unsigned int periods,len = 0;
    ktime_t edge;
    unsigned long tx_time = 0;
    long delta;

    pr_info(KOOKONG_DEVICE_NAME" kookong_spi_write start\n");

    edge = ktime_get();
    ret=copy_from_user(buf,buf_user,size);
    if(ret!=0) {
        return -EINVAL;
    }
    count=size/(sizeof(int))-1;
    pr_info(KOOKONG_DEVICE_NAME" count = %d\n",count);

    kookong_rc_data->freq = (unsigned long)buf[0];

    pr_info(KOOKONG_DEVICE_NAME" carrier = %d\n",kookong_rc_data->freq);
    for(i=0;i<count;i++)
    {
        tx_time += buf[i+1];
        periods = DIV_ROUND_CLOSEST(buf[i+1] * kookong_rc_data->freq, 1000000);
        len += periods;
        pr_debug(KOOKONG_DEVICE_NAME" data[%d] = %d\n",i,buf[i+1]);
    }
    
    pr_info(KOOKONG_DEVICE_NAME" len = %d tx_time = %ld us\n",len,tx_time);	

    if(len >= IR_SPI_MAX_BUFSIZE)
        ret = ir_spi_write_longcmd(kookong_rc_data->rc,&buf[1],count);
    else
        ret = ir_spi_tx(kookong_rc_data->rc,&buf[1],count);

    edge = ktime_add_us(edge, tx_time);
    delta = ktime_us_delta(edge, ktime_get());
    if (delta > 0)
    {
            udelay(delta);
    }
    
    pr_info(KOOKONG_DEVICE_NAME" kookong_spi_write end delta = %ld\n",delta);
    return ret;
}

static long kookong_spi_ioctl(struct file *filep, unsigned int cmd,
        unsigned long arg)
{
    pr_info(KOOKONG_DEVICE_NAME" kookong_spi_ioctl cmd = %d\n",cmd);
    switch (cmd) {
        case 2:
            break;
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
        case 1:
            break;
        case 0:
        default:
            break;
    }

    return 8;
}

static struct file_operations kookong_spi_ops = {
    .owner			= THIS_MODULE,
    .open			= kookong_spi_open,
    .release		= kookong_spi_close,
    .write			= kookong_spi_write,
    .unlocked_ioctl		= kookong_spi_ioctl,
};

static struct miscdevice kookong_misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = KOOKONG_DEVICE_NAME,
    .fops = &kookong_spi_ops,
};

static int ir_spi_probe(struct spi_device *spi)
{
    int ret;
    u8 dc;
    struct ir_spi_data *idata;

    pr_info(KOOKONG_DEVICE_NAME" ir_spi_probe\n");

    idata = devm_kzalloc(&spi->dev, sizeof(*idata), GFP_KERNEL);
    if (!idata)
        return -ENOMEM;

    idata->regulator = devm_regulator_get(&spi->dev, "vdd");
    if (IS_ERR(idata->regulator))
        return PTR_ERR(idata->regulator);

    idata->rc = devm_rc_allocate_device(&spi->dev, RC_DRIVER_IR_RAW_TX);
    if (!idata->rc)
        return -ENOMEM;

    idata->rc->tx_ir           = ir_spi_tx;
    idata->rc->s_tx_carrier    = ir_spi_set_tx_carrier;
    idata->rc->s_tx_duty_cycle = ir_spi_set_duty_cycle;
    idata->rc->device_name	   = "IR SPI";
    idata->rc->driver_name     = IR_SPI_DRIVER_NAME;
    idata->rc->priv            = idata;
    idata->spi                 = spi;
    kookong_rc_data = idata;

    idata->negated = of_property_read_bool(spi->dev.of_node,
                            "led-active-low");
    ret = of_property_read_u8(spi->dev.of_node, "duty-cycle", &dc);
    if (ret)
        dc = 50;

    /* ir_spi_set_duty_cycle cannot fail,
     * it returns int to be compatible with the
     * rc->s_tx_duty_cycle function
     */
    ir_spi_set_duty_cycle(idata->rc, dc);

    idata->freq = IR_SPI_DEFAULT_FREQUENCY;

    ret = misc_register(&kookong_misc_dev);

    pr_info(KOOKONG_DEVICE_NAME" probe done\n");

    return devm_rc_register_device(&spi->dev, idata->rc);
}

static int ir_spi_remove(struct spi_device *spi)
{
    return 0;
}

static const struct of_device_id ir_spi_of_match[] = {
    { .compatible = "kookong-ir-spi" },
    {},
};

static struct spi_driver ir_spi_driver = {
    .probe = ir_spi_probe,
    .remove = ir_spi_remove,
    .driver = {
        .name = IR_SPI_DRIVER_NAME,
        .of_match_table = ir_spi_of_match,
    },
};

module_spi_driver(ir_spi_driver);

MODULE_AUTHOR("Andi Shyti <andi.shyti@samsung.com>");
MODULE_DESCRIPTION("SPI IR LED");
MODULE_LICENSE("GPL v2");

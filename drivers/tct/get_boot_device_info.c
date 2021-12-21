/*
   Add a new file.c by dingting.meng on 2019/9/30,for task-8325724,to add dev_info_emmc
   Update by dingting.meng for T-9869946 on 3/9/2020
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#define TCT_BOOT_DEV_MAX    32
#define TCT_BOOT_DEV_ERR    (-1)
#define TCT_BOOT_DEV_EMMC   1
#define TCT_BOOT_DEV_UFS    2
#define QCOM_BOOT_EMMC      "sdhci"
#define QCOM_BOOT_UFS       "ufshc"

extern void get_dev_info_emmc(char *pdest);
extern void get_dev_info_ufs(char *pdest);

extern char eMMC_module_name[256];

static char tct_boot_dev[TCT_BOOT_DEV_MAX] = {'\0'};

static u8 *emmc_spt_tab[][5] =
{
        {"150100525036344D42","KMRP60014M-B614","Samsung","64G+4G","AMD0000635C1"},
        {"8F0100555936435330","UNPVN6G5CACA4CS","UNIC","64G+4G","AMD0000663C1"},
        {"13014E47314A395238","MT29VZZZAD8GQFSL_046W_9R8","Micron","64G+4G","AMD0000700C1"},
        {"150100444836444142", "KMDH6001DA_B422", "Samsung", "64G+4G", "AMD0000607C1"},
        {NULL,NULL,NULL,NULL,NULL},
};

static u8 *ufs_spt_tab[][5] =
{
        {"KM2V8001CM-B707","KM2V8001CM-B707","Samsung","128G+6G","AMD0000678C1"},
        {"KM2B8001CM-BB01","KM2B8001CM-BB01","Samsung","256G+6G","AMD0000712C1"},
        {"KM5V8001DM-B622","KM5V8001DM-B622","Samsung","128G+4G","AMD0000717C1"},
        {"MT128GASAO4U21","MT29VZZZBD9FQKPR-046 W.G9J","Micron","128G+6G","AMD0000729C1"},
        {NULL,NULL,NULL,NULL,NULL},
};

static int __init tct_get_boot_dev(char *str)
{
    strlcpy(tct_boot_dev, str, TCT_BOOT_DEV_MAX);
    return 1;
}

__setup("androidboot.boot_devices=",tct_get_boot_dev);

static int check_boot_dev(const char *str)
{
    printk("#### %s | %d %s ####\n",__func__,__LINE__,str);

    if(strnstr(str,QCOM_BOOT_EMMC,strlen(str)))
        return TCT_BOOT_DEV_EMMC;

    if(strnstr(str,QCOM_BOOT_UFS,strlen(str)))
        return TCT_BOOT_DEV_UFS;

    return TCT_BOOT_DEV_ERR;
}

static void remap_emmc_dev_info(char *src,char *dst)
{
    int index = 0;

    while(emmc_spt_tab[index][0] != NULL)
    {
        if(!strncmp(emmc_spt_tab[index][0],src,18))
        {
            sprintf(dst, "%s:%s:%s:%s",emmc_spt_tab[index][1],emmc_spt_tab[index][2],emmc_spt_tab[index][3],emmc_spt_tab[index][4]);
 
            return;
        }

        index++;
    }

    sprintf(dst, "%s:Unknown:Unknown:Unknown:Unknown",src);

    return;
}

static void remap_ufs_dev_info(char *src,char *dst)
{
    int index = 0,len = 0;

    len = strlen(src);

    while(ufs_spt_tab[index][0] != NULL)
    {
        if(!strncmp(ufs_spt_tab[index][0],src,len))
        {
            sprintf(dst, "%s:%s:%s:%s",ufs_spt_tab[index][1],ufs_spt_tab[index][2],ufs_spt_tab[index][3],ufs_spt_tab[index][4]);
            return;
        }

        index++;
    }

    sprintf(dst, "%s:Unknown:Unknown:Unknown:Unknown",src);

    return;
}

static int get_boot_dev_init(void)
{
    int ret = -1;

    char tmp_buf[64] = {'\0'};

    ret = check_boot_dev(tct_boot_dev);

    if(ret == TCT_BOOT_DEV_ERR)
    {
        printk("#### The Boot Dev Is Neither EMMC Nor UFS %s | %d ####\n",__func__,__LINE__);

        return -ENODEV;
    }

    if(ret == TCT_BOOT_DEV_EMMC)
    {
        printk("#### The Boot Dev Is EMMC: %s | %d ####\n",__func__,__LINE__);

        get_dev_info_emmc(tmp_buf);
        printk("#### %s | %d %s ####\n",__func__,__LINE__,tmp_buf);
 
        remap_emmc_dev_info(tmp_buf,eMMC_module_name);

        return 0;
    }

    if(ret == TCT_BOOT_DEV_UFS)
    {
        printk("#### The Boot Dev Is UFS: %s | %d ####\n",__func__,__LINE__);

        get_dev_info_ufs(tmp_buf);
        printk("#### %s | %d %s ####\n",__func__,__LINE__,tmp_buf);

        remap_ufs_dev_info(tmp_buf,eMMC_module_name);

        return 0;
    }

    return 0;
}

static void get_boot_dev_exit(void)
{
    ;
}

late_initcall(get_boot_dev_init);
module_exit(get_boot_dev_exit);
MODULE_LICENSE("GPL");

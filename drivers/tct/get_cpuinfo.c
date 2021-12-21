/*
   Add a new file.c by dingting.meng on 26/09/2020,for T-xxxx
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

extern char CPU_module_name[256];

extern char *socinfo_get_id_string(void);

static u8 *cpuinfo_tab[][2] = 
{
    {"BENGAL","SM4250"},   
    {"LAGOON","SM6350"},   
    {NULL,NULL},
};

static void remap_cpuinfo(const char *src,char *dst)
{
    int index = 0;
   
    printk("##### %s | %d %s #####\n",__func__,__LINE__,src);
 
    while(cpuinfo_tab[index][0] != NULL)
    {
        if(strstr(src,cpuinfo_tab[index][0]))
        {
            sprintf(dst,"%s",cpuinfo_tab[index][1]);
            
            return;
        }

        index++;
    }

    sprintf(dst,"Unknown");

    return;
}

static int get_cpuinfo_init(void)
{
    char *ptmp = NULL;

    ptmp = socinfo_get_id_string();

    remap_cpuinfo(ptmp,CPU_module_name);

    return 0;
}

static void get_cpuinfo_exit(void)
{
    ;
}

late_initcall(get_cpuinfo_init);
module_exit(get_cpuinfo_exit);
MODULE_LICENSE("GPL");

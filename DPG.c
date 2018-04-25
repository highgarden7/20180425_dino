#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>


#define MODULE_NAME "DPG"

int SKELETONDIA_MAJOR = 221;

#define M_INPUT     0 //gpio function select macro
#define M_OUTPUT    1

#define S_LOW        0//gpio status macro
#define S_HIGH       1

//I/O base address in kernel mode
#define BCM2835_PERI_BASE    0x3F000000
//GPIO base address in kernel mode
#define GPIO_BASE (BCM2835_PERI_BASE + 0x00200000)

static const int LedGpioPin = 16;           // Pin LED is connected to
static int led_status = 0;                    // 0-off, 1-on
static struct kobject *led_kobj;

typedef struct GpioRegisters{
    volatile uint32_t GPFSEL[6];
    volatile uint32_t Reserved1;
    volatile uint32_t GPSET[2];
    volatile uint32_t Reserved2;
    volatile uint32_t GPCLR[2];
}GPIO;
GPIO *s_pGpioRegisters;

static ssize_t led_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", led_status);
}
static void SetGPIOFunction(int GPIO, int functionCode)
{
	int registerIndex = GPIO / 10;
	int bit = (GPIO%10)*3;
	unsigned oldValue = s_pGpioRegisters->GPFSEL[registerIndex];
	unsigned mask = 0b111 << bit;
	s_pGpioRegisters->GPFSEL[registerIndex] = (oldValue & ~mask) | ((functionCode << bit) & mask);
}
static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
	if(outputValue)
	{
		s_pGpioRegisters->GPSET[GPIO/32] = (1<<(GPIO%32));
	}
	else
	{
		s_pGpioRegisters->GPCLR[GPIO/32] = (1<<(GPIO%32));
	}
}
static ssize_t led_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%du", &led_status);
    
    if(led_status)
    {
        SetGPIOOutputValue(LedGpioPin, true); // Turn LED on.
    }
    else
    {
        SetGPIOOutputValue(LedGpioPin, false); // Turn LED off.
    }
    return count;
}
static struct kobj_attribute led_attr = __ATTR(led_pwr, 0660, led_show, led_store);

int DPG_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "DIAGPIO : DPG_open() is called..\n");
    SetGPIOOutputValue(LedGpioPin,1);
    printk("DPG open success!!!\n");
    return 0;
}

int DPG_release(struct inode *inode, struct file *flip)
{
    printk("DIAGPIO : DPG_release() is called.. \n");
    SetGPIOOutputValue(LedGpioPin,0);
    printk("DPG release success!!!!\n");
    return 0;
}

struct file_operations DPG_fops = {
    .owner   = THIS_MODULE,
    .open    = DPG_open,
    .release = DPG_release,
    //.read    = DPG_read,
    //.write   = DPG_write,
    //.unlocked_ioctl   = DPG_unlocked_ioctl,
};

static int __init DPG_init(void)
{
    int result;
    int error =0;
    printk("DPG init called\n");
    result = register_chrdev(SKELETONDIA_MAJOR,"DPG",&DPG_fops);
    if(result <0 )
    {
	    printk("DPG : Can't get major number!\n:");
	    return result;
    }
    if(SKELETONDIA_MAJOR == 0) SKELETONDIA_MAJOR = result;
				
    s_pGpioRegisters = (struct GpioRegisters *)ioremap(GPIO_BASE, sizeof(struct GpioRegisters));         // Map physical address to virtual address space.
    SetGPIOFunction(LedGpioPin, 0b001);                    // Configure the pin as output.
    led_kobj = kobject_create_and_add("led_ctrl_pwr", kernel_kobj);
    if(!led_kobj) {
        return -ENOMEM;
    }
    error = sysfs_create_file(led_kobj, &led_attr.attr);
    if(error) {
        printk(KERN_INFO "Failed to register sysfs for LED");
    }
    return error;
}
static void __exit DPG_exit(void)
{
    SetGPIOFunction(LedGpioPin, 0);                     // Configure the pin as input.
    iounmap(s_pGpioRegisters);                          // Unmap physical to virtual addresses.
    kobject_put(led_kobj);
    printk("<1>Bye Bye Mr.Blue\n");
    unregister_chrdev(SKELETONDIA_MAJOR,"DPG");
}

module_init(DPG_init);
module_exit(DPG_exit);

MODULE_LICENSE("Dual BSD/GPL");

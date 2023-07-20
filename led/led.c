#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/timer.h>
#include "led.h"

#define CLOSE_TIMER_CMD (_IO(0XEF, 0x1)) /* 关闭定时器 */
#define OPEN_TIMER_CMD (_IO(0XEF, 0x2)) /* 打开定时器 */
#define SET_TIME_PERIOD_CMD (_IO(0XEF, 0x3)) /* 设置定时器周期命令 */

static void __iomem *CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

static struct led_desc desc;

void led_on(void)
{
	unsigned int val;
	val = readl(GPIO1_DR);
	pr_info("<%s> old: val=0x%x\n", __func__, val);
	val &= ~(1 << 3);
	writel(val, GPIO1_DR);

	val = readl(GPIO1_DR);
	pr_info("<%s> new: val=0x%x\n", __func__, val);
}
EXPORT_SYMBOL(led_on);

void led_off(void)
{
	unsigned int val;
	val = readl(GPIO1_DR);
	pr_info("<%s> old: val=0x%x\n", __func__, val);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	val = readl(GPIO1_DR);
	pr_info("<%s> new: val=0x%x\n", __func__, val);
}
EXPORT_SYMBOL(led_off);

static void set_led_status(unsigned char status)
{
	if (status == 0)
		led_off();
	else
		led_on();
}

static void restart_timer(void)
{
	unsigned long flags = 0;
	int timerperiod;

	spin_lock_irqsave(&desc.lock, flags);
	timerperiod = desc.timeperiod;
	spin_unlock_irqrestore(&desc.lock, flags);
	/* 重启定时器 */
	(void)mod_timer(&desc.timer, jiffies + msecs_to_jiffies(timerperiod));
}

static void set_time_period(unsigned long arg)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&desc.lock, flags);
	desc.timeperiod = arg;
	spin_unlock_irqrestore(&desc.lock, flags);
}

void timer_function(unsigned long arg)
{
	struct led_desc *desc = (struct led_desc *)arg;

	desc->status = !desc->status;
	set_led_status(desc->status);
	restart_timer();
}

static void init_led_timer(void)
{
	spin_lock_init(&desc.lock);
	init_timer(&desc.timer);
	desc.timer.function = timer_function;
	desc.timer.data = (unsigned long)&desc;
	desc.timeperiod = 1000;
}

static long timer_unlocked_ioctl(struct file* flip, unsigned int cmd, unsigned long arg)
{
	struct led_desc *desc = (struct led_desc *)flip->private_data;
	pr_info("[zrc] cmd=0x%x\n", cmd);

	switch (cmd) {
	case CLOSE_TIMER_CMD:
		del_timer_sync(&desc->timer);
		break;
	case OPEN_TIMER_CMD:
		restart_timer();
		break;
	case SET_TIME_PERIOD_CMD:
		set_time_period(arg);
		break;
	default:
		break;
	}
	return 0;
}

static ssize_t led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &desc;
	return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
	int cnt, loff_t *offt)
{
	unsigned long ret;
	unsigned char onoff;

	ret = copy_from_user(&onoff, buf, cnt);
	if (ret < 0) {
		pr_err("write led failed\n");
		return -EFAULT;
	}

	if (onoff == 1)
		led_on();
	else if (onoff == 0)
		led_off();
	else
		pr_info("please input 1 to turn on led, 0 to turn off led\n");

	return cnt;
}

static void led_reg_init(struct device_node *np)
{
	unsigned int val;

	// 初始化LED
	// 映射寄存器
	CCM_CCGR1 = of_iomap(np, 4);
	SW_MUX_GPIO1_IO03 = of_iomap(np, 0);
	SW_PAD_GPIO1_IO03 = of_iomap(np, 3);
	GPIO1_DR = of_iomap(np, 1);
	GPIO1_GDIR = of_iomap(np, 2);

	// 使能GPIO01时钟 [27:26] 设置成11 时钟在所有模式下都处于打开状态，但停止模式除外。
	val = readl(CCM_CCGR1);
	val &= ~(0x3 << 26);
	val |= (0x3 << 26);
	writel(val, CCM_CCGR1);

	// 设置复用
	writel(0x5, SW_MUX_GPIO1_IO03);

	// 设置PAD 1 0000 1011 0000 (100MHZ  Hysteresis Enabled)
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	// 设置为输出 第三位表示GPIO3
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO1_GDIR);

	// 设置为高电平, 关闭
	val = readl(GPIO1_DR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO1_DR);
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
	.unlocked_ioctl = timer_unlocked_ioctl,
};

static int __init led_init(void)
{
	struct device_node *np;
	struct property *proper;
	int ret;
	struct led_desc *pdesc = &desc;

	// 获取led设备节点
	np = of_find_node_by_path("/led");
	if (!np) {
		pr_err("No sysrq node found\n");
		return -1;
	}

	// 获取compatible属性
	proper = of_find_property(np, "compatible", NULL);
	if (!proper) {
		pr_err("find led's compatible failed\n");
	}

	led_reg_init(np);

	// 注册字符设备驱动
	ret = alloc_chrdev_region(&pdesc->dev_id, 0, 1, "led");
	if (ret != 0) {
		pr_err("alloc_chrdev_region failed\n");
		kfree(pdesc);
		return -1;
	}

	pdesc->major = MAJOR(pdesc->dev_id);
	pdesc->minor = MINOR(pdesc->dev_id);
	pr_info("devid=0x%x, major=%d, minor=%d\n", pdesc->dev_id, pdesc->major, pdesc->minor);

	// 初始化cdev
	(void)cdev_init(&pdesc->cdev, &led_fops);

	// 添加cdev
	ret = cdev_add(&pdesc->cdev, pdesc->dev_id, 1);
	if (ret != 0) {
		pr_err("cdev_add failed\n");
		goto out;
	}

	//创建类
	pdesc->class = class_create(THIS_MODULE, "led");
	if (IS_ERR(pdesc->class)) {
		pr_err("create class: led failed\n");
		goto out1;
	}

	pdesc->device = device_create(pdesc->class, NULL, pdesc->dev_id, NULL, "led");
	if (IS_ERR(pdesc->device)) {
		pr_err("create device led failed\n");
		goto out2;
	}

	init_led_timer();
	pr_info("probe led success\n");
	return 0;

out2:
	class_destroy(pdesc->class);
	kfree(pdesc);
out1:
	cdev_del(&pdesc->cdev);
	kfree(pdesc);
out:
	unregister_chrdev_region(pdesc->dev_id, 1);
	kfree(pdesc);
	return ret;
	
}

static void __exit led_exit(void)
{
	struct led_desc *pdesc = &desc;
	// 取消映射
	iounmap(CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	/* 注销字符设备驱动 */
	cdev_del(&pdesc->cdev);/* 删除 cdev */
	unregister_chrdev_region(pdesc->dev_id, 1);/*注销设备号*/
	device_destroy(pdesc->class, pdesc->dev_id);
	class_destroy(pdesc->class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");


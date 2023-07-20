#ifndef __LED_H
#define __LED_H

struct led_desc {
	dev_t dev_id;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *np;
	int timeperiod; // 定时器周期,单位MS
	struct timer_list timer; // 定时器结构体
	spinlock_t lock;
	unsigned char status; // 0:关闭LED
};

void led_off(void);
void led_on(void);

#endif

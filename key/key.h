#ifndef __KEY_H
#define __KEY_H

struct key_irq {
	char name[10];
	unsigned int irqnum;
	irqreturn_t (*handler)(int, void *);
};

struct key_dev {
	dev_t dev_id;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *np;
	int gpio;
	int timeperiod;
	struct timer_list timer;
	spinlock_t lock;
	unsigned char value;
	struct key_irq irq;
};

extern void led_on(void);
extern void led_off(void);

#endif

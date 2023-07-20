#ifndef __MISCBEEP_H
#define __MISCBEEP_H

struct miscbeep_dev {
	dev_t dev_id;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *np;
	int gpio;
};

#endif
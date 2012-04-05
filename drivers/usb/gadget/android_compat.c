/*
 * android_compat.c - backwards interface compatibility for Android USB gadget
 *
 * Copyright (C) Paul Kocialkowski <contact@paulk.fr>
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include "android.h"


static void android_compat_toggle(struct android_dev *dev)
{
	if (list_empty(&dev->enabled_functions) == 1) {
		android_enable(dev, 0);
	} else {
		android_enable(dev, 0);
		android_enable(dev, 1);
	}
}

static int android_compat_enable_function(struct android_dev *dev, char *name)
{
	int enabled = android_check_function_enabled(dev, name);
	int err;

	if(enabled) {
		printk(KERN_INFO "android_compat_enable_function: %s function already enabled!\n", name);
		return 0;
	}

	err = android_enable_function(dev, name);
	android_compat_toggle(dev);

	return err;
}

static int android_compat_disable_function(struct android_dev *dev, char *name)
{
	int enabled = android_check_function_enabled(dev, name);
	int err;

	if(!enabled) {
		printk(KERN_INFO "android_compat_enable_function: %s function already disabled!\n", name);
		return 0;
	}

	err = android_disable_function(dev, name);

	android_compat_toggle(dev);

	return err;
}

/*-------------------------------------------------------------------------*/
/* android_adb_enable dev node */

static int adb_enable_open(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "adb_enable_open: enabling adb function\n");
	return android_compat_enable_function(dev, "adb");
}

static int adb_enable_release(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "adb_enable_release: disabling adb function\n");
	return android_compat_disable_function(dev, "adb");
}

static const struct file_operations adb_enable_fops = {
	.owner =   THIS_MODULE,
	.open =    adb_enable_open,
	.release = adb_enable_release,
};

static struct miscdevice adb_enable_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_adb_enable",
	.fops = &adb_enable_fops,
};

/*-------------------------------------------------------------------------*/
/* usb_composite sys node */

static struct class *composite_class;

static ssize_t composite_enable_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function *function = dev_get_drvdata(pdev);

	int enabled = android_check_function_enabled(dev, function->name);

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t composite_enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function *function = dev_get_drvdata(pdev);

	int enable;

	sscanf(buff, "%d", &enable);
	if(enable) {
		android_compat_enable_function(dev, function->name);
	} else {
		android_compat_disable_function(dev, function->name);
	}

	return size;
}

static struct device_attribute composite_dev_attr_enable = {
	.attr = {
		.name = "enable",
		.mode = S_IWUSR | S_IRUGO,

	},
	.show = composite_enable_show,
	.store = composite_enable_store,
};

static int composite_functions_init(void)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function **functions = dev->functions;
	struct android_usb_function *f;

	int index = 0;
	int err;

	for (index = 0; (f = *functions++); index++) {
		f->compat_dev = device_create(composite_class, NULL,
				MKDEV(0, index), f, f->name);
		if (IS_ERR(f->compat_dev))
			return PTR_ERR(f->compat_dev);

		err = device_create_file(f->compat_dev, &composite_dev_attr_enable);
		if (err < 0) {
			device_destroy(composite_class, f->compat_dev->devt);
			return err;
		}

		dev_set_drvdata(f->compat_dev, f);
	}

	return 0;
}

static void composite_functions_cleanup(void)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_function **functions = dev->functions;
	struct android_usb_function *f;

	while (*functions) {
		f = *functions++;

		if (f->compat_dev) {
			device_destroy(composite_class, f->compat_dev->devt);
			device_remove_file(f->compat_dev, &composite_dev_attr_enable);
		}
	}
}

static int android_compat_init(void)
{
	int ret;

	ret = misc_register(&adb_enable_device);
	if (ret)
		goto error;

	composite_class = class_create(THIS_MODULE, "usb_composite");
	if (IS_ERR(composite_class))
		return PTR_ERR(composite_class);
//	composite_class->dev_uevent = composite_uevent;

	composite_functions_init();

	return 0;

error:
	printk(KERN_ERR "android_compat_init: error\n");
	return ret;
}

static void android_compat_cleanup(void)
{
	composite_functions_cleanup();

	misc_deregister(&adb_enable_device);
}

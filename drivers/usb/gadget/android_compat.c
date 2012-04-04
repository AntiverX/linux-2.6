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

static struct class *android_compat_class;

static int adb_enable_open(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "adb_enable_open: enabling adb function\n");

	return android_enable_function(dev, "adb");
}

static int adb_enable_release(struct inode *ip, struct file *fp)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "adb_enable_release: disabling adb function\n");

	return android_disable_function(dev, "adb");
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

static int android_compat_init(void)
{
	int ret;

	ret = misc_register(&adb_enable_device);
	if (ret)
		goto error;

	return 0;

error:
	printk(KERN_ERR "android_compat_init: error\n");
	return ret;
}

static void android_compat_cleanup(void)
{
	misc_deregister(&adb_enable_device);
}

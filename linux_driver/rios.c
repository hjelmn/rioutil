/*
 * USB Diamond MM(r)/Sonic Blue(r)/DNNA(r) Rio(tm) linux driver 
 *
 * Nathan Hjelm <hjelmn@users.sourceforge.net>
 *
 * Feb 2005: v1.2
 *    - Fuse/Chiba/Cali support
 * Dec 2002: v1.1
 *   - Riot and S-Series support
 * May 20 2001: v1.0
 *   - rio 500, 600, 800, 900 and psaplay support is complete
 *   - added support for multiple rio connections
 *   - devfs support is now more complete
 *
 * Based of rio500.c by Cesar Miquel <miquel@df.uba.ar>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Based upon rio500.c (Cesar Miquel)
 * Based upon scanner.c
 * Based upon mouse.c (Brad Keryan) and printer.c (Michael Gee).
 *
 * */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/smp_lock.h>
#include "rio_usb.h"

#define RIO_MINOR_BASE   64
#define RIO_MINORS       16

/* stall/wait timeout for rio */
#define NAK_TIMEOUT (HZ)


#define IBUF_SIZE 0x1000

/* Size of the rio buffer */
#define OBUF_SIZE 0x10000

typedef 
struct rio_usb_data {
        struct usb_device *dev;                       /* init: probe_rio */
	devfs_handle_t	   devfs;                     /* devfs */
        unsigned int       ifnum;                     /* Interface number of the USB device */
        int                isopen;                    /* nz if open */
	int                minor;                     /* minor of rio */
        char              *obuf, *ibuf;               /* transfer buffers */
        char               bulk_in_ep, bulk_out_ep;   /* Endpoint assignments */
        wait_queue_head_t  wait_q;                    /* for timeouts */
}rio_usb_data;

static struct rio_usb_data *rio_table[RIO_MINORS];

extern devfs_handle_t usb_devfs_handle;

static int open_rio(struct inode *inode, struct file *file){
        int minor = MINOR(inode->i_rdev) - RIO_MINOR_BASE;
        rio_usb_data *rio;

	if ((minor < 0) || (minor >= RIO_MINORS))
	        return -ENODEV;

	lock_kernel();
	rio = rio_table[minor];
	file->private_data = rio;

	if (!rio || !rio->dev){
	        unlock_kernel();
		return -ENODEV;
	}

	if (rio->isopen) {
		unlock_kernel();
		return -EBUSY;
	}

	rio->isopen = 1;
	info("open_rio: rio opened.");

	MOD_INC_USE_COUNT;
	unlock_kernel();
	return 0;
}

static int read_endpoint_rio (rio_usb_data *rio) {
	int id_product = rio->dev->descriptor.idProduct;

	if ( (id_product == PRODUCT_RIO600) ||
	     (id_product == PRODUCT_RIO800) ||
	     (id_product == PRODUCT_RIO900) ||
	     (id_product == PRODUCT_RIORIOT) ||
	     (id_product == PRODUCT_PSAPLAY) )
		return 2;
	else
		return 1;
}

static int write_endpoint_rio (rio_usb_data *rio) {
	int id_product = rio->dev->descriptor.idProduct;

	if (id_product == PRODUCT_RIORIOT)
		return 1;
	else
		return 2;
}

static int close_rio(struct inode *inode, struct file *file){
        rio_usb_data *rio = file->private_data;

	if (!rio && !rio->dev)
	  return -ENODEV;

	MOD_DEC_USE_COUNT;
	rio->isopen = 0;
	info("close_rio: rio closed.");

	return 0;
}

static int
ioctl_rio(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg){
	struct RioCommand rio_cmd;
	void *data;
	unsigned char *buffer;
	int result, requesttype;
	int retries;
	int retval;
        rio_usb_data *rio = file->private_data;

        /* Sanity check to make sure rio is connected, powered, etc */
        if (!rio || !rio->dev)
                return -ENODEV;

	switch (cmd) {
	case RIO_RECV_COMMAND:
		data = (void *) arg;
		if (!data)
			break;
		if (copy_from_user(&rio_cmd, data, sizeof(struct RioCommand))) {
			retval = -EFAULT;
			goto err_out;
		}
		if (rio_cmd.length > PAGE_SIZE) {
			retval = -EINVAL;
			goto err_out;
		}
		buffer = (unsigned char *) __get_free_page(GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		if (copy_from_user(buffer, rio_cmd.buffer, rio_cmd.length)) {
			retval = -EFAULT;
			free_page((unsigned long) buffer);
			goto err_out;
		}

		requesttype = rio_cmd.requesttype | USB_DIR_IN |
		    USB_TYPE_VENDOR | USB_RECIP_DEVICE;
		dbg
		    ("sending command:reqtype=%0x req=%0x value=%0x index=%0x len=%0x",
		     requesttype, rio_cmd.request, rio_cmd.value,
		     rio_cmd.index, rio_cmd.length);
		/* Send rio control message */
		retries = 3;
		while (retries) {
			result = usb_control_msg(rio->dev,
						 usb_rcvctrlpipe(rio-> dev, 0),
						 rio_cmd.request,
						 requesttype,
						 rio_cmd.value,
						 rio_cmd.index, buffer,
						 rio_cmd.length,
						 rio_cmd.timeout);
			if (result == -ETIMEDOUT)
				retries--;
			else if (result < 0) {
				err("Error executing ioctrl. code = %d",
				     le32_to_cpu(result));
				retries = 0;
			} else {
				dbg("Executed ioctl. Result = %d (data=%04x)",
				     le32_to_cpu(result),
				     le32_to_cpu(*((long *) buffer)));
				if (copy_to_user(rio_cmd.buffer, buffer,
						 rio_cmd.length)) {
					free_page((unsigned long) buffer);
					retval = -EFAULT;
					goto err_out;
				}
				retries = 0;
			}

			/* rio_cmd.buffer contains a raw stream of single byte
			   data which has been returned from rio.  Data is
			   interpreted at application level.  For data that
			   will be cast to data types longer than 1 byte, data
			   will be little_endian and will potentially need to
			   be swapped at the app level */

		}
		free_page((unsigned long) buffer);
		break;

	case RIO_SEND_COMMAND:
		data = (void *) arg;
		if (!data)
			break;
		if (copy_from_user(&rio_cmd, data, sizeof(struct RioCommand)))
			return -EFAULT;
		if (rio_cmd.length > PAGE_SIZE)
			return -EINVAL;
		buffer = (unsigned char *) __get_free_page(GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		if (copy_from_user(buffer, rio_cmd.buffer, rio_cmd.length)) {
			free_page((unsigned long)buffer);
			return -EFAULT;
		}

		requesttype = rio_cmd.requesttype | USB_DIR_OUT |
		    USB_TYPE_VENDOR | USB_RECIP_DEVICE;
		dbg("sending command: reqtype=%0x req=%0x value=%0x index=%0x len=%0x",
		     requesttype, rio_cmd.request, rio_cmd.value,
		     rio_cmd.index, rio_cmd.length);
		/* Send rio control message */
		retries = 3;
		while (retries) {
			result = usb_control_msg(rio->dev,
						 usb_sndctrlpipe(rio-> dev, 0),
						 rio_cmd.request,
						 requesttype,
						 rio_cmd.value,
						 rio_cmd.index, buffer,
						 rio_cmd.length,
						 rio_cmd.timeout);
			if (result == -ETIMEDOUT)
				retries--;
			else if (result < 0) {
				err("Error executing ioctrl. code = %d",
				     le32_to_cpu(result));
				retries = 0;
			} else {
				dbg("Executed ioctl. Result = %d",
				       le32_to_cpu(result));
				retries = 0;

			}

		}
		free_page((unsigned long) buffer);
		break;

	default:
		return -ENOIOCTLCMD;
		break;
	}

	return 0;

err_out:
	return retval;
}

static ssize_t
write_rio(struct file *file, const char *buffer,
	  size_t count, loff_t * ppos)
{
	unsigned long copy_size;
	unsigned long bytes_written = 0;
	unsigned int partial;

	int result = 0;
	int maxretry;
	int errn = 0;

        rio_usb_data *rio;

	rio = file->private_data;

        /* Sanity check to make sure rio is connected, powered, etc */
        if (!rio || !rio->dev)
          return -ENODEV;

	do {
		unsigned long thistime;
		char *obuf = rio->obuf;

		thistime = copy_size =
		    (count >= OBUF_SIZE) ? OBUF_SIZE : count;
		if (copy_from_user(rio->obuf, buffer, copy_size)) {
			errn = -EFAULT;
			goto error;
		}
		maxretry = 5;
		while (thistime) {
			if (!rio->dev) {
				errn = -ENODEV;
				goto error;
			}
			if (signal_pending(current)) {
				return bytes_written ? bytes_written : -EINTR;
			}

			result = usb_bulk_msg(rio->dev,
					      usb_sndbulkpipe(rio->dev, write_endpoint_rio (rio)),
					      obuf, thistime, &partial, 5 * HZ);

			dbg("write stats: result:%d thistime:%lu partial:%u\n",
			     result, thistime, partial);

			if (result == USB_ST_TIMEOUT) {	/* NAK - so hold for a while */
				if (!maxretry--) {
					errn = -ETIME;
					goto error;
				}
				interruptible_sleep_on_timeout(&rio-> wait_q, NAK_TIMEOUT);
				continue;
			} else if (!result & partial) {
				obuf += partial;
				thistime -= partial;
			} else
				break;
		};
		if (result) {
			err("Write Whoops - %x", result);
			errn = -EIO;
			goto error;
		}
		bytes_written += copy_size;
		count -= copy_size;
		buffer += copy_size;
	} while (count > 0);

	return bytes_written ? bytes_written : -EIO;

error:
	return errn;
}

static ssize_t
read_rio(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	ssize_t read_count;
	unsigned int partial;
	int this_read;
	int result;
	int maxretry = 10;
	rio_usb_data *rio;
	char *ibuf;

	rio = file->private_data;

        /* Sanity check to make sure rio is connected, powered, etc */
        if (!rio || !rio->dev)
          return -ENODEV;

	ibuf = rio->ibuf;

	read_count = 0;

	while (count > 0) {
		if (signal_pending(current)) {
			return read_count ? read_count : -EINTR;
		}
		if (!rio->dev) {
			return -ENODEV;
		}
		this_read = (count >= IBUF_SIZE) ? IBUF_SIZE : count;

		result = usb_bulk_msg(rio->dev,
				      usb_rcvbulkpipe(rio->dev, read_endpoint_rio (rio)),
				      ibuf, this_read, &partial,
				      (int) (HZ * 16));

		dbg("read stats: result:%d this_read:%u partial:%u\n",
		       result, this_read, partial);

		if (partial) {
			count = this_read = partial;
		} else if (result == USB_ST_TIMEOUT || result == 15) {	/* FIXME: 15 ??? */
			if (!maxretry--) {
				err("read_rio: maxretry timeout");
				return -ETIME;
			}
			interruptible_sleep_on_timeout(&rio->wait_q,
						       NAK_TIMEOUT);
			continue;
		} else if (result != USB_ST_DATAUNDERRUN) {
			err("Read Whoops - result:%u partial:%u this_read:%u",
			     result, partial, this_read);
			return -EIO;
		} else {
			unlock_kernel();
			return (0);
		}

		if (this_read) {
			if (copy_to_user(buffer, ibuf, this_read)) {
				return -EFAULT;
			}
			count -= this_read;
			read_count += this_read;
			buffer += this_read;
		}
	}
	return read_count;
}

static struct file_operations usb_rio_fops = {
        owner:          THIS_MODULE,
	read:		read_rio,
	write:		write_rio,
	ioctl:		ioctl_rio,
	open:		open_rio,
	release:	close_rio,
};

static void *probe_rio(struct usb_device *dev, unsigned int ifnum,
		       const struct usb_device_id *id)
{
	rio_usb_data *rio;
	char name[6];
	int minor;

	/* Bail if the Vendor is not recognized */
	switch(dev->descriptor.idVendor){
	case VENDOR_DIAMOND00:
	case VENDOR_DIAMOND01:
		break;
	default:
		err("probe_rio: Device not recognized by this driver");

		return NULL;
	}

	switch(dev->descriptor.idProduct){
	case PRODUCT_RIO500:
	        info("USB Rio 500 found at address %d", dev->devnum);

		break;
        case PRODUCT_RIO600:
		info("USB Rio 600 found at address %d", dev->devnum);

		break;
	case PRODUCT_RIO800:
		info("USB Rio 800 found at address %d", dev->devnum);

		break;
	case PRODUCT_RIOS10:
		info("USB Rio S10 found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIOS11:
		info("USB Rio S11 found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIOS50:
		info("USB Rio S50 found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIOS35:
		info("USB Rio S35S found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIOS30:
		info("USB Rio S30S found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIORIOT:
		info("USB Rio Riot found at address %d", dev->devnum);
		
		break;
	case PRODUCT_RIO900:
		info("USB Rio 900 found at address %d", dev->devnum);
		
		break;
	case PRODUCT_PSAPLAY:
		info("USB Rio psa[play found at address %d", dev->devnum);

		break;
	case PRODUCT_FUSE:
		info("USB Rio Fuse found at address %d", dev->devnum);

		break;
	case PRODUCT_CHIBA:
		info("USB Rio Chiba found at address %d", dev->devnum);

		break;
	case PRODUCT_CALI:
		info("USB Rio Cali found at address %d", dev->devnum);

		break;
	case PRODUCT_RIOS11:
		info("USB Rio S11/ESA found at address %d", dev->devnum);

		break;
	case PRODUCT_FUSE:
		info("USB Rio Fuse found at address %d", dev->devnum);
		
		break;
	case PRODUCT_CHIBA:
		info("USB Rio Chiba found at address %d", dev->devnum);
		
		break;
	case PRODUCT_CALI:
	case PRODUCT_CALI256:
		info("USB Rio Cali found at address %d", dev->devnum);
		
		break;
	default:
		err("probe_rio: Device not recognized by this driver");

		return NULL;
	}

	for (minor = 0 ; minor < RIO_MINORS && rio_table[minor]; minor++)
		if (rio_table[minor]->dev == NULL)
			break;

	if (rio_table[minor] && rio_table[minor]->dev){
		err("probe_rio: No more minors available for device");
		return NULL;
	}

	if (rio_table[minor]) {
		rio = rio_table[minor];
	} else {
		if (!(rio = (rio_usb_data *) kmalloc(sizeof(rio_usb_data),
						     GFP_KERNEL))) {
			err("out of memory");
			return NULL;
		}
	}

	memset(rio, 0, sizeof(rio_usb_data));

	rio->minor = minor;
	rio->dev = dev;
	rio->isopen = 0;

	if (!(rio->obuf = (char *) kmalloc(OBUF_SIZE, GFP_KERNEL))) {
		err("probe_rio: Not enough memory for the output buffer");
		return NULL;
	}
	dbg("probe_rio: obuf address:%p", rio->obuf);

	if (!(rio->ibuf = (char *) kmalloc(IBUF_SIZE, GFP_KERNEL))) {
		err("probe_rio: Not enough memory for the input buffer");
		kfree(rio->obuf);
		return NULL;
	}
	dbg("probe_rio: ibuf address:%p", rio->ibuf);

	info("Device assigned minor %d",minor);
	sprintf(name, "rio%d", minor);

	init_waitqueue_head(&rio->wait_q);

	rio->devfs = devfs_register (usb_devfs_handle, name, DEVFS_FL_DEFAULT,
			USB_MAJOR, RIO_MINOR_BASE + minor,
			S_IFCHR | S_IRUSR | S_IWUSR |
			S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
			&usb_rio_fops, NULL);

	/* no devfs != error */
	if (!rio->devfs)
	        dbg("probe_rio: device node registration failed");

	rio_table[minor] = rio;
	return rio;
}

static void disconnect_rio(struct usb_device *dev, void *ptr)
{
	struct rio_usb_data *rio = (struct rio_usb_data *) ptr;

	if (!rio || !rio->dev){
		err("disconnect on a nonexisting interface");		
	}

	/* decrement module use count */
	MOD_DEC_USE_COUNT;

	rio->dev = NULL;

	devfs_unregister(rio->devfs);

	rio_table[rio->minor] = NULL;

	kfree(rio->ibuf);
	kfree(rio->obuf);

	info("disconnect_rio: USB rio disconnected.");
}

static struct usb_device_id rio_id_table [] = {
	{ USB_DEVICE(VENDOR_DIAMOND00, PRODUCT_RIO500    ) }, 	   /* Rio 500      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIO600    ) },      /* Rio 600      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIO800    ) },	   /* Rio 800      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_PSAPLAY   ) },	   /* Nike psaplay */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS10    ) },      /* Rio S10      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS11    ) },      /* Rio S11      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS50    ) },      /* Rio S50      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS35    ) },      /* Rio S35S     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS30    ) },      /* Rio S30S     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIORIOT   ) },      /* Rio Riot     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIO900    ) },      /* Rio 900      */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOFUSE   ) },      /* Rio Fuse     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOCHIBA  ) },      /* Rio Chiba    */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOCALI   ) },      /* Rio Cali     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOCALI256) },      /* Rio Cali     */
	{ USB_DEVICE(VENDOR_DIAMOND01, PRODUCT_RIOS11    ) },      /* Rio S11/ESA  */
	{ }					                /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, rio_id_table);

static struct usb_driver rio_driver = {
	name:		"usbrio",
	probe:		probe_rio,
	disconnect:	disconnect_rio,
	fops:		&usb_rio_fops,
	minor:		RIO_MINOR_BASE,
	id_table:	rio_id_table,
};

static int __init usb_rio_init(void)
{
	if (usb_register(&rio_driver) < 0)
		return -1;

	info("USB rio support registered.");
	return 0;
}


static void __exit usb_rio_cleanup(void)
{
	int i;

	/* clean up all allocated space */
	for (i=0;i<RIO_MINORS;i++)
		if (rio_table[i]) {
			if (rio_table[i]->devfs)
				devfs_unregister(rio_table[i]->devfs);

			if (rio_table[i]->ibuf)
				kfree(rio_table[i]->ibuf);

			if (rio_table[i]->obuf)
				kfree(rio_table[i]->obuf);

			kfree(rio_table[i]);
		}

	usb_deregister(&rio_driver);
}

module_init(usb_rio_init);
module_exit(usb_rio_cleanup);

MODULE_AUTHOR("Nathan Hjelm <hjelmn@users.sourceforge.net>");
MODULE_DESCRIPTION("USB Rio driver");

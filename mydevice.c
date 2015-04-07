#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/miscdevice.h>
#include<linux/device.h>
#include<linux/fs.h>
#include<linux/slab.h>
#include <linux/uaccess.h>
#include <linux/parport.h>

static struct pardevice *pprt;
static DEFINE_SPINLOCK(pprt_lock);
static unsigned char data,control;

static const struct file_operations mydevice_fops;
static struct miscdevice mydevice[2] = { 
	{ 
		.minor = MISC_DYNAMIC_MINOR,
		.name = "mydev1",
		.fops = &mydevice_fops,
		.nodename = "mydev1",
		.mode = 0666,  // pin 1 = STROBE
	},
	{ 
		.minor = MISC_DYNAMIC_MINOR,
		.name = "mydev2",
		.fops = &mydevice_fops,
		.nodename = "mydev2",
		.mode = 0666, // pin 5 = D3
	}
};

static int mydevice_open(struct inode *inode, struct file *file)
{
	int i,minor = iminor(inode);
	int *deviceno = kmalloc(sizeof(int),GFP_KERNEL);
	for (i=0;i<2;i++) {
		dev_t dvt = (mydevice[i].this_device)->devt;
		if( minor == MINOR(dvt)) {
			*deviceno = i;
			break;
		}
	}
	file->private_data = deviceno;
	return 0;
}

static int mydevice_release(struct inode *inode, struct file *file)
{
	if(file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}
	return 0;
}


static ssize_t mydevice_write(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char temp;
	int retval = len;
	int devicepin;

	if(copy_from_user(&temp, buff, 1)) //only take the first byte
		return -EFAULT;
	if(temp<48 || temp>49)  //ascii code of 0 and 1
		return -EINVAL;
	temp -=48;
	devicepin = *(int *)(filp->private_data);
	pr_err("got %d at %d\n",temp,devicepin);
	spin_lock_irq(&pprt_lock);
	switch(devicepin) {
	case 0: //STROBE is the first bit
		control = parport_read_control(pprt->port);
		control &= ~(0x01); //reset the bit
		control |= !temp; // change the bit, ! because STROBE is activelow 
		parport_write_control(pprt->port, control);
		break;	

	case 1:
		data = parport_read_data(pprt->port);
		pr_err("data = %x\n",data);
		data &= ~(1<<3);
		data |= (temp<<3);		
		pr_err("data = %x\n",data);
		parport_write_data(pprt->port, data);
		break;

	default:
		retval = -EINVAL;
		break;
	}
	spin_unlock_irq(&pprt_lock);	
	return retval;
}

static void mydevice_attach(struct parport *port)
{
	int i,err;
	if (port->number != 0)  // we connect to lpt1
		return;

	if (pprt) {
		pr_err("port->number=%d already registered!\n",port->number);
		return;
	}

	pprt = parport_register_device(port, "mydevice", NULL, NULL, NULL, 0, (void *)&pprt);
	if (pprt == NULL) {
		pr_err("port->number=%d, parport_register_device() failed\n", port->number);
		return;
	}

	if (parport_claim(pprt)) {
		pr_err("could not claim access to parport. Aborting.\n");
		goto err_unreg_device;
	}

	for (i=0;i<2;i++) {
		err = misc_register(&mydevice[i]);
		if(err < 0)
			goto error;
	}

	return;

error:
	for(--i;i>=0;i--)
		misc_deregister(&mydevice[i]);

err_unreg_device:
	parport_unregister_device(pprt);
	pprt = NULL;
}

static void mydevice_detach(struct parport *port)
{
	int i;
	if (port->number != 0)
		return;

	if (!pprt) {
		pr_err("nothing to unregister.\n");
		return;
	}

	for(i=0;i<2;i++)
		misc_deregister(&mydevice[i]);

	parport_release(pprt);
	parport_unregister_device(pprt);
	pprt = NULL;
}



static const struct file_operations mydevice_fops = {
	.owner = THIS_MODULE,
	.write= mydevice_write,
	.open = mydevice_open,
	.release = mydevice_release,
};

static struct parport_driver mydevice_driver = {
	.name = "mydevice",
	.attach = mydevice_attach,
	.detach = mydevice_detach,
};

int __init init_m(void)
{
	return parport_register_driver(&mydevice_driver);
}

void __exit cleanup_m(void)
{	
	parport_unregister_driver(&mydevice_driver);
}
module_init(init_m);
module_exit(cleanup_m);
MODULE_LICENSE("GPL");

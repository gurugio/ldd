
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/highmem.h>
MODULE_LICENSE("GPL");

static int major;
static int minor;
static struct class *cl;
static struct cdev my_cdev;

struct page *mmap_page;


void simple_vma_open(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA open, virt 0x%lx, phys 0x%lx\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void simple_vma_close(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA close\n");
}

static struct vm_operations_struct my_mem_vm_ops = {
	.open = simple_vma_open,
	.close = simple_vma_close,
	.name = "MY_MEM",
};

static int open_mem(struct inode *inode, struct file *filp)
{
	/* if (dev->dev_info) */
	/* 	filp->f_mapping->backing_dev_info = dev->dev_info; */

	/* /\* Is /dev/mem or /dev/kmem ? *\/ */
	/* if (dev->dev_info == &directly_mappable_cdev_bdi) */
	/* 	filp->f_mode |= FMODE_UNSIGNED_OFFSET; */

	printk("%s %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static ssize_t read_mem(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	unsigned long offset = *ppos;

	printk("buf:%p count:%d ppos:%p p:%lx\n", buf, count, ppos, offset);
	*ppos += count;
	return 0;
}

static int mmap_mem(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	char *ptr_page;
	int i;

	vma->vm_page_prot = phys_mem_access_prot(file,
						 vma->vm_pgoff,
						 size,
						 vma->vm_page_prot);
	printk(KERN_NOTICE "vm_page_prot=%lx\n", (unsigned long)vma->vm_page_prot);
	
	vma->vm_ops = &my_mem_vm_ops;

	mmap_page = alloc_page(GFP_KERNEL);
	ptr_page = kmap_atomic(mmap_page);
	for (i = 0; i < 20; i++)
		ptr_page[i] = i;
	kunmap_atomic(ptr_page);

	/* Remap-pfn-range will mark the range VM_IO */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    page_to_pfn(mmap_page), //vma->vm_pgoff, // pfn
			    size,
			    vma->vm_page_prot)) {
		return -EAGAIN;
	}
	
	vma->vm_ops->open(vma);
	return 0;
}


static const struct file_operations my_mem_fops = {
	/* .llseek		= memory_lseek, */
	.read		= read_mem,
	/* .write		= write_mem, */
	.mmap		= mmap_mem,
	.open		= open_mem,
	/* .get_unmapped_area = get_unmapped_area_mem, */
};

static int __init my_mem_init(void)
{
	int ret;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, minor, 1, "my_mem");
	if (ret < 0) {
		printk(KERN_CRIT "fail to allocate chrdev region\n");
		return ret;
	}
	major = MAJOR(dev);

	printk("major=%d minor=%d\n", major, minor);

	cl = class_create(THIS_MODULE, "my_mem_class");
	if (IS_ERR(cl)) {
		printk(KERN_CRIT "fail to class_create\n");
		unregister_chrdev_region(dev, 1);
		return -1;
	}

	cdev_init(&my_cdev, &my_mem_fops);
	if (cdev_add(&my_cdev, dev, 1) == -1) {
		printk(KERN_CRIT "fail to cdev_add\n");
		class_destroy(cl);
		unregister_chrdev_region(dev, 1);
		return -1;
	}

	if (device_create(cl, NULL, MKDEV(major, minor), NULL, "my_mem0") == NULL) {
		printk(KERN_CRIT " fail to create device\n");
		class_destroy(cl);
		unregister_chrdev_region(MKDEV(major, minor), 1);
		cdev_del(&my_cdev);
		return -1;
	}
		
	return 0;
}

static void __exit my_mem_exit(void)
{
	dev_t dev = MKDEV(major, minor);

	device_destroy(cl, dev);
	cdev_del(&my_cdev);
	class_destroy(cl);
	unregister_chrdev_region(dev, 1);
}

module_init(my_mem_init);
module_exit(my_mem_exit);

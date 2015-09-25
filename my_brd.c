
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/sizes.h>
MODULE_LICENSE("GPL");

static int major;
static int		my_brd_number;
static struct request_queue	*my_brd_queue;
static struct gendisk		*my_brd_disk;

/*
 * Backing store of pages and lock to protect it. This is the contents
 * of the block device.
 */
static spinlock_t		my_brd_lock;
static struct radix_tree_root	my_brd_pages;


static int my_brd_rw_page(struct block_device *bdev, sector_t sector,
		       struct page *page, int rw)
{
	printk(KERN_EMERG "%s:%d\n", __FUNCTION__, __LINE__);
	printk(KERN_EMERG "============================================\n");
	dump_stack();
	printk(KERN_EMERG "============================================\n\n\n");
	return 0;
}

static int my_brd_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long arg)
{
	printk(KERN_EMERG "%s:%d\n", __FUNCTION__, __LINE__);
	printk(KERN_EMERG "============================================\n");
	dump_stack();
	printk(KERN_EMERG "============================================\n\n\n");
	return 0;
}

#ifdef CONFIG_BLK_DEV_RAM_DAX
static long my_brd_direct_access(struct block_device *bdev, sector_t sector,
			void **kaddr, unsigned long *pfn, long size)
{
	/* struct brd_device *brd = bdev->bd_disk->private_data; */
	/* struct page *page; */

	/* if (!brd) */
	/* 	return -ENODEV; */
	/* page = brd_insert_page(brd, sector); */
	/* if (!page) */
	/* 	return -ENOSPC; */
	/* *kaddr = page_address(page); */
	/* *pfn = page_to_pfn(page); */

	/* /\* */
	/*  * TODO: If size > PAGE_SIZE, we could look to see if the next page in */
	/*  * the file happens to be mapped to the next page of physical RAM. */
	/*  *\/ */
	return PAGE_SIZE;
}
#else
#define my_brd_direct_access NULL
#endif


static const struct block_device_operations my_brd_fops = {
	.owner =		THIS_MODULE,
	.rw_page =		my_brd_rw_page,
	.ioctl =		my_brd_ioctl,
	.direct_access =	my_brd_direct_access,
};


static void brd_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct brd_device *brd = bdev->bd_disk->private_data;
	int rw;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
	int err = -EIO;

	/* brd_make_request() is called 3-times before my_brd_init completes.
	 * Where? Why? Why 3-times?
	 */
	printk(KERN_EMERG "brd_make_request: do nothing but bio_end()\n");
	printk(KERN_EMERG "============================================\n");
	dump_stack();
	printk(KERN_EMERG "============================================\n\n\n");

	printk(KERN_EMERG "sector=%d size=%d capa=%d rw=%d\n",
	       (int)bio->bi_iter.bi_sector,
	       (int)bio->bi_iter.bi_size,
	       (int)get_capacity(my_brd_disk),
	       (int)bio->bi_rw & REQ_DISCARD);
	       
#if 0
	sector = bio->bi_iter.bi_sector;
	if (bio_end_sector(bio) > get_capacity(bdev->bd_disk))
		goto out;

	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		err = 0;
		discard_from_brd(brd, sector, bio->bi_iter.bi_size);
		goto out;
	}

	rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;
#endif
	bio_endio(bio, 0);

}

static struct kobject *brd_probe(dev_t dev, int *part, void *data)
{
	struct kobject *kobj;

	printk(KERN_EMERG "brd_probe start\n");
	kobj = get_disk(my_brd_disk);
	*part = 0;

	printk(KERN_EMERG "brd_probe ends\n");
	return kobj;
}

static int __init my_brd_init(void)
{
	printk(KERN_EMERG "start my_brd_init\n");
	major = register_blkdev(0, "my_brd_ramdisk");
	if (major < 0)
		goto out;

	printk(KERN_EMERG "major=%d\n", major);
	my_brd_number		= 0;
	spin_lock_init(&my_brd_lock);
	INIT_RADIX_TREE(&my_brd_pages, GFP_ATOMIC);

	my_brd_queue = blk_alloc_queue(GFP_KERNEL);
	if (!my_brd_queue)
		goto out_unregister;
	
	blk_queue_make_request(my_brd_queue, brd_make_request);
	blk_queue_max_hw_sectors(my_brd_queue, 1024);
	blk_queue_bounce_limit(my_brd_queue, BLK_BOUNCE_ANY);

	/* This is so fdisk will align partitions on 4k, because of
	 * direct_access API needing 4k alignment, returning a PFN
	 * (This is only a problem on very small devices <= 4M,
	 *  otherwise fdisk will align on 1M. Regardless this call
	 *  is harmless)
	 */
	blk_queue_physical_block_size(my_brd_queue, PAGE_SIZE);

	my_brd_queue->limits.discard_granularity = PAGE_SIZE;
	my_brd_queue->limits.max_discard_sectors = UINT_MAX;
	my_brd_queue->limits.discard_zeroes_data = 1;
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, my_brd_queue);

	/* 1: number of devices?? */
	my_brd_disk = alloc_disk(1);
	if (!my_brd_disk)
		goto out_free_queue;
	printk(KERN_EMERG "disk-alloc ok\n");
	my_brd_disk->major		= major;
	my_brd_disk->first_minor	= 0;
	my_brd_disk->fops		= &my_brd_fops;
	my_brd_disk->private_data	= NULL;
	my_brd_disk->queue		= my_brd_queue;
	my_brd_disk->flags		= GENHD_FL_EXT_DEVT;
	sprintf(my_brd_disk->disk_name, "my_brd%d", 0);
	set_capacity(my_brd_disk, SZ_16M / 512);

	/* disk is shown in /sys/block??? */
	/* add_disk() must be after all initializations.
	 * It calls many methods of the disk including brd_make_request() */
	add_disk(my_brd_disk);
	printk(KERN_EMERG "add_disk ok\n");
	
	blk_register_region(MKDEV(major, 0), 1UL,
			    THIS_MODULE, brd_probe, NULL, NULL);
	printk(KERN_EMERG "blk_register_region ok\n");

	printk(KERN_EMERG "complete my_brd_init\n");
	return 0;

out_free_queue:
	blk_cleanup_queue(my_brd_queue);
out_unregister:
	unregister_blkdev(major, "my_brd_ramdisk");
out:
	return 0;
}

/*
 * Free all backing store pages and radix tree. This must only be called when
 * there are no other users of the device.
 */
#define FREE_BATCH 16
static void my_brd_free_pages(struct radix_tree_root *pages_tree)
{
	unsigned long pos = 0;
	struct page *pages[FREE_BATCH];
	int nr_pages;

	do {
		int i;

		nr_pages = radix_tree_gang_lookup(pages_tree,
				(void **)pages, pos, FREE_BATCH);

		for (i = 0; i < nr_pages; i++) {
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(pages_tree, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}

		pos++;

		/*
		 * This assumes radix_tree_gang_lookup always returns as
		 * many pages as possible. If the radix-tree code changes,
		 * so will this have to.
		 */
	} while (nr_pages == FREE_BATCH);
}

static void __exit my_brd_exit(void)
{
	del_gendisk(my_brd_disk); /* <-> alloc_disk */

	put_disk(my_brd_disk);  /* <-> get_disk */
	blk_cleanup_queue(my_brd_queue);
	my_brd_free_pages(&my_brd_pages);

	blk_unregister_region(MKDEV(major, 0), 1UL);
	unregister_blkdev(RAMDISK_MAJOR, "my_brd_ramdisk");

	printk(KERN_EMERG "complete my_brd_exit\n");
}

module_init(my_brd_init);
module_exit(my_brd_exit);
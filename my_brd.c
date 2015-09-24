
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
	return 0;
}

static int my_brd_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long arg)
{
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
}

static int __init my_brd_init(void)
{
	major = register_blkdev(0, "my_brd_ramdisk");
	if (major < 0)
		goto out;

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
	my_brd_disk->major		= major;
	my_brd_disk->first_minor	= 0;
	my_brd_disk->fops		= &my_brd_fops;
	my_brd_disk->private_data	= NULL;
	my_brd_disk->queue		= my_brd_queue;
	my_brd_disk->flags		= GENHD_FL_EXT_DEVT;
	sprintf(my_brd_disk->disk_name, "my_brd%d", 0);
	set_capacity(my_brd_disk, SZ_16M / 512);

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
	put_disk(my_brd_disk);
	blk_cleanup_queue(my_brd_queue);
	my_brd_free_pages(&my_brd_pages);
	unregister_blkdev(major, "my_brd_ramdisk");
}

module_init(my_brd_init);
module_exit(my_brd_exit);

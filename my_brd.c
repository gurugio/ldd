
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

#define SECTOR_SHIFT		9

static int major;
static int		my_brd_number;
static struct request_queue	*my_brd_queue;
static struct gendisk		*my_brd_disk;

/*
 * Backing store of pages and lock to protect it. This is the contents
 * of the block device.
 */
static spinlock_t		my_brd_lock;

#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1 << PAGE_SECTORS_SHIFT)

#define DISK_PAGES (SZ_32M / 4096)
#define DISK_SECTORS (SZ_32M / 512)
static struct page *disk_image[DISK_PAGES];


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


static struct page *brd_sector_to_page(int sector_num)
{
	return disk_image[sector_num >> PAGE_SECTORS_SHIFT];
}

static void brd_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	/* struct brd_device *brd = bdev->bd_disk->private_data; */
	int rw;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
	int err = -EIO;

	/* brd_make_request() is called 3-times before my_brd_init completes.
	 * Where? Why? Why 3-times?
	 * ARM-board: add_disk() calls blkdev_get()->rescan_partition()->...
	 [  299.736026] CPU: 0 PID: 806 Comm: insmod Tainted: G        W  O  3.16.7+ #53
	 [  299.743084] [<80014ec4>] (unwind_backtrace) from [<80011eb4>] (show_stack+0x10/0x14)
	 [  299.750840] [<80011eb4>] (show_stack) from [<805cf094>] (dump_stack+0x80/0xc0)
	 [  299.758072] [<805cf094>] (dump_stack) from [<7f0080f0>] (brd_make_request+0x2c/0x80 [my_brd])
	 [  299.766606] [<7f0080f0>] (brd_make_request [my_brd]) from [<802a30f8>] (generic_make_request+0xa8/0xd4)
	 [  299.776014] [<802a30f8>] (generic_make_request) from [<802a31a8>] (submit_bio+0x84/0x168)
	 [  299.784186] [<802a31a8>] (submit_bio) from [<801443ac>] (_submit_bh+0x1a4/0x24c)
	 [  299.791623] [<801443ac>] (_submit_bh) from [<80145b38>] (block_read_full_page+0x274/0x328)
	 [  299.799894] [<80145b38>] (block_read_full_page) from [<800c5a34>] (do_read_cache_page+0x84/0x198)
	 [  299.808772] [<800c5a34>] (do_read_cache_page) from [<800c5b60>] (read_cache_page+0x18/0x20)
	 [  299.817129] [<800c5b60>] (read_cache_page) from [<802b3724>] (read_dev_sector+0x28/0x6c)
	 [  299.825215] [<802b3724>] (read_dev_sector) from [<802b5068>] (read_lba+0xbc/0x160)
	 [  299.832785] [<802b5068>] (read_lba) from [<802b542c>] (efi_partition+0xc8/0x8b8)
	 [  299.840186] [<802b542c>] (efi_partition) from [<802b45a8>] (check_partition+0x140/0x1fc)
	 [  299.848277] [<802b45a8>] (check_partition) from [<802b3c80>] (rescan_partitions+0x9c/0x2bc)
	 [  299.856632] [<802b3c80>] (rescan_partitions) from [<80147ec4>] (__blkdev_get+0x2bc/0x3dc)
	 [  299.864803] [<80147ec4>] (__blkdev_get) from [<80148188>] (blkdev_get+0x1a4/0x31c)
	 [  299.872373] [<80148188>] (blkdev_get) from [<802b1a58>] (add_disk+0x330/0x42c)
	 [  299.879602] [<802b1a58>] (add_disk) from [<7f00a14c>] (my_brd_init+0x14c/0x1c4 [my_brd])
	 [  299.887695] [<7f00a14c>] (my_brd_init [my_brd]) from [<80008b84>] (do_one_initcall+0x8c/0x1c4)
	 [  299.896311] [<80008b84>] (do_one_initcall) from [<800914cc>] (load_module+0x1b28/0x1f6c)
	 [  299.904394] [<800914cc>] (load_module) from [<800919ec>] (SyS_init_module+0xdc/0xf0)
	 [  299.912138] [<800919ec>] (SyS_init_module) from [<8000e560>] (ret_fast_syscall+0x0/0x30)
	 [  299.920244] ============================================
	 [  299.928516] sector=0 size=4096 capa=32768 rw=0
	 */
	/* printk(KERN_EMERG "brd_make_request: do nothing but bio_end()\n"); */
	/* printk(KERN_EMERG "============================================\n"); */
	/* dump_stack(); */
	/* printk(KERN_EMERG "============================================\n"); */

	printk(KERN_EMERG "bio-info:sector=%d size=%d capa=%d rw=%d\n",
	       (int)bio->bi_iter.bi_sector,
	       (int)bio->bi_iter.bi_size,
	       (int)get_capacity(my_brd_disk),
	       (int)(bio->bi_rw & REQ_DISCARD));
	       

	sector = bio->bi_iter.bi_sector;

	/* printk(KERN_EMERG "end_sector=%d capa=%d\n", */
	/*        (int)bio_end_sector(bio), */
	/*        (int)get_capacity(bdev->bd_disk)); */
	if (bio_end_sector(bio) > get_capacity(bdev->bd_disk)) {
		printk(KERN_EMERG "sector overflow\n");
		goto out;
	}

	/* rw == REQ_DISCARD == 0x80: discard sector */
	/* REQ_DISCARD command is called when format */
	/* BUGBUG: discarding size is multiple of page-size?? */
	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		int n = (int)bio->bi_iter.bi_size; /* length to clear */
		err = 0;

		while (n >= PAGE_SIZE) {
			/* clear page */
			/* page number == sector / 8 */
			clear_highpage(brd_sector_to_page(sector));
			/* sector=512-byte, page=4096-byte ===> sector += 8 */
			sector += PAGE_SIZE >> SECTOR_SHIFT;
			n -= PAGE_SIZE;
		}
		goto out;
	}
	
	rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;

	bio_for_each_segment(bvec, bio, iter) {
		void *mem;
		void *disk_ptr;
		unsigned int len = bvec.bv_len;
		unsigned int offset = sector & (PAGE_SECTORS - 1) << SECTOR_SHIFT;

		printk(KERN_EMERG "bvec:sector=%d page-%x disk-page-%x len=%d offset=%x rw=%s\n",
		       (int)sector,
		       (int)page_to_pfn(bvec.bv_page),
		       (int)page_to_pfn(brd_sector_to_page(sector)),
		       bvec.bv_len, bvec.bv_offset,
		       rw == READ ? "READ":"WRITE");


		/*
		 * BUGBUG: len can be bigger than PAGE_SIZE.
		 * It should check next page should be referenced.
		 *
		 * And is (disk_ptr + offset) correct?
		 */
		
		mem = kmap_atomic(bvec.bv_page);
		disk_ptr = kmap_atomic(brd_sector_to_page(sector));
		if (rw == READ) {
			memcpy(mem + bvec.bv_offset, disk_ptr + offset, len);
		} else {
			memcpy(disk_ptr + offset, mem + bvec.bv_offset, len);
		}
		kunmap_atomic(disk_ptr);
		kunmap_atomic(mem);
		sector += len >> SECTOR_SHIFT;
	}

	err = 0;
out:
	bio_endio(bio, err);
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
	int i;

	printk(KERN_EMERG "allocate %d-pages\n", DISK_PAGES);
	for (i = 0; i < DISK_PAGES; i++)
		disk_image[i] = alloc_page(GFP_KERNEL);
	

	printk(KERN_EMERG "start my_brd_init\n");
	major = register_blkdev(0, "my_brd_ramdisk");
	if (major < 0)
		goto out;

	printk(KERN_EMERG "major=%d\n", major);
	my_brd_number		= 0;
	spin_lock_init(&my_brd_lock);

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
	set_capacity(my_brd_disk, DISK_SECTORS);

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

static void __exit my_brd_exit(void)
{
	int i;
	del_gendisk(my_brd_disk); /* <-> alloc_disk */

	put_disk(my_brd_disk);  /* <-> get_disk */
	blk_cleanup_queue(my_brd_queue);

	blk_unregister_region(MKDEV(major, 0), 1UL);
	unregister_blkdev(major, "my_brd_ramdisk");

	for (i = 0; i < DISK_PAGES; i++)
		__free_page(disk_image[i]);

	printk(KERN_EMERG "complete my_brd_exit\n");
}

module_init(my_brd_init);
module_exit(my_brd_exit);

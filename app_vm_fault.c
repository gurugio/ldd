

/*
 *
 * test app for drv_remap_pfn_range.c drv_vm_insert_mixed.c drv_vma_fault.c
 *
 */



#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

char buf[1024];

int main(void)
{
	int fd;
	int ret;
	char *ptr[3];
	int i;
	char command[128];

	system("ps -eo vsz,rss,pid,comm | grep a.out");
	system("cat /proc/meminfo | grep MemFree");
	
	fd = open("/dev/my_mem0", O_RDWR | O_SYNC);
	if (fd < 0) {
		printf("fail to open my_mem0\n");
		return 0;
	}

	ptr[0] = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap failed");
	}
	printf("ptr-%p\n", ptr[0]);
	for (i = 0; i < 10; i++) {
		printf("%x ", ptr[0][i]);
	}
	printf("\n");

	ptr[1] = mmap(NULL, 4096*2, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 4096*1);
	if (ptr == MAP_FAILED) {
		perror("mmap failed");
	}
	printf("ptr-%p\n", ptr[1]);
	for (i = 0; i < 10; i++) {
		printf("%x ", ptr[1][i]);
	}
	printf("\n");

	ptr[2] = mmap(NULL, 4096*3, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 4096*3);
	if (ptr == MAP_FAILED) {
		perror("mmap failed");
	}
	printf("ptr-%p\n", ptr[2]);
	for (i = 0; i < 10; i++) {
		printf("%x ", ptr[2][i]);
	}
	printf("\n");
	printf("ptr-%p\n", ptr[2] + 4096);
	for (i = 4096; i < 4096 + 10; i++) {
		printf("%x ", ptr[2][i]);
	}
	printf("\n");


	system("ps -eo vsz,rss,pid,comm | grep a.out");
	system("cat /proc/meminfo | grep MemFree");
	sprintf(command, "cat /proc/%d/smaps", getpid());
	system(command);

	sleep(1);
	munmap(ptr[0], 4096);
	munmap(ptr[1], 4096*2);
	munmap(ptr[2], 4096*3);
	close(fd);

	return 0;
}

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"
#include "fo.h"

//该程序适用于根目录区数据范围是簇的整数倍

#define VOL_NAME 'h' /* 卷标，根据自己的分区进行调整 */

struct bios_pram_block bpb; /* 定义全局BPB */
u32 dir_offset[16];
u32 cur_index=0;
u32 dir_mul_offset[16];
u32 sum; /* 大于1的时候多簇 */
u32 init_dir_mul_offset[16];
u32 init_sum;
char init_path[8]="PS N:\\>";
char path[1000]="PS N:\\>";

void print_bpb(struct bios_pram_block *bpb);

int main(int argc, char *argv[])
{
	disk_open_vol(VOL_NAME);
	disk_read_bpb(&bpb);
	path[3]=VOL_NAME+'A'-'a';
	init_path[3]=VOL_NAME+'A'-'a';

	dir_offset[0]=bpb.root_start_offset;

	dir_mul_offset[0]=bpb.root_start_offset;
	init_dir_mul_offset[0]=bpb.root_start_offset;
	init_sum=bpb.root_ent_cnt*32/(bpb.clus_size);
	sum=init_sum;
	memset(dir_mul_offset,0,sizeof(dir_mul_offset));
	//printf("%d\n",sum);
	for(int i=1;i<init_sum;i++){
		init_dir_mul_offset[i]=init_dir_mul_offset[i-1]+2048;
	}
	memcpy(dir_mul_offset,init_dir_mul_offset,sizeof(init_dir_mul_offset));
	/*for(int i=0;i<=init_sum;i++){
		printf("%08x\n",init_dir_mul_offset[i]);
	}*/
	//printf("%03x",fo_offset_to_clus(0x8000));
	const int SIZE = 1000;
	int opt;
	char cmd[SIZE];
	char arg[SIZE];

	while (1)
	{	
		printf("%s",path);
		cmd[0] = arg[0] = '\0';

		scanf("%s", cmd);
		if (getchar() != '\n')
			gets(arg);

		if (!strcmp(cmd, "ls")) /* ls 显示文件列表 */
			ls(arg);
		else if (!strcmp(cmd, "cd")) /* cd 切换目录 */
			cd(arg);
		else if (!strcmp(cmd, "mkdir")) /* mkdir 创建目录 */
			mkdir(arg);
		else if (!strcmp(cmd, "touch")) /* touch 创建文件 */
			touch(arg);
		else if (!strcmp(cmd, "rm")) /* rm 删除文件/目录 */
			rm(arg);
		else if (!strcmp(cmd,"cat")) /* cat 查看文件内容 */
			cat(arg);
		else if (!strcmp(cmd,"hexdump")) /* hexdump 查看磁盘数据 */
			hexdump(arg);
		//调用库
		else if (!strcmp(cmd,"clear"))
			system("cls");
		else if (!strcmp(cmd,"exit"))
			exit(0);
		else if (!strcmp(cmd, "-help")||!strcmp(cmd,"help")) /* help 显示帮助信息 */
		{
			printf("可用命令如下: \n");
			printf("ls [目录]: 显示目录下的文件列表\n");
			printf("ls -f -0xXXX: 查看文件分配表,可选到达上限\n");
			printf("ls -f -X: 查询X簇号的下一个簇号\n");
			printf("ls -c: 打印文件控制块\n");
			printf("cd [目录]: 切换到指定的目录\n");
			printf("mkdir [目录]: 创建一个新的目录\n");
			printf("touch [文件]: 创建一个新的文件\n");
			printf("cat [文件]: 查看文件内容\n");
			printf("hexdump -p -0xd -n : 查看磁盘数据 0xd: 起始地址 n: 字节上限 p: C 16进制\n");
			printf("rm [文件]: 删除指定的文件\n");
			printf("rm [目录] -r:删除指定的目录\n");
			printf("clear: 清屏\n");
			printf("exit: 退出\n");
			printf("-help/help:显示帮助信息\n");
		}
		else
		{
			printf((strlen(arg) ? "未知命令\"%s %s\"" : "未知命令\"%s\""), cmd, arg);
			puts("输入-help/help查看帮助");
		}
	}
	disk_close();
	return 0;
}

/* 输出bios_pram_block中的内容 */
void print_bpb(struct bios_pram_block *bpb)
{
	printf("文件系统类型: %s\n", bpb->fs_type);
	printf("卷标ID: %u\n", bpb->vol_id);
	printf("卷标名: %s\n\n", bpb->vol_lab);
	printf("每个扇区的字节数: %u\n", bpb->byte_per_sec);
	printf("每个簇的扇区数: %u\n", bpb->sec_per_clus);
	printf("保留扇区总数: %u\n", bpb->rsvd_sec_cnt);
	printf("FAT数量: %u\n", bpb->num_fats);
	printf("根目录条目数量上限: %u\n", bpb->root_ent_cnt);
	printf("每个FAT占用扇区数: %u\n", bpb->sec_per_fat);
	printf("扇区总数: %u\n", bpb->tot_sec);
	printf("FAT表起始偏移: %u\n", bpb->fat_start_offset);
	printf("根目录起始偏移: %u\n", bpb->root_start_offset);
	printf("数据区起始偏移: %u\n", bpb->data_start_offset);
}
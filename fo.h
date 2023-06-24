#pragma once

#include <windows.h>
#include <stdio.h>
#include<winbase.h>
#include "types.h"
#include "disk.h"

#define FILE_SIZE 32

/*
 *   起始地址:起始扇区*每扇区字节
 *   0x1000 0x4000 0x8000
 *   FAT表起始扇区:保留扇区大小
 *   根目录起始扇区:保留扇区+2*FAT表扇区个数
 *   数据区起始扇区:保留扇区+2*FAT表扇区个数+根目录最大个数*32/每扇区字节
 *   簇号从2开始
 */

/*
 * FAT12/16 文件条目
 * 关闭内存对齐优化
 */
struct file_entry{
    u8 Dir_Name[11];    /*文件名和文件拓展名*/
    /* 0x10为目录 0x20为文件 */
    u8 Dir_Attr;        /*文件属性*/
    u8 Reserve[10];     /*保留位*/
    u16 Dir_WrtTime;    /*创建时间*/
    u16 Dir_WrtDate;    /*创建日期*/
    u16 Dir_First_Clus; /*首簇号*/
    u32 Dir_File_Size;  /*文件大小*/
}__attribute__((packed));

void fo_printf_all_fat(u32 size); /* 打印文件分配表 */
u16 fo_get_next_free_fat(); /* 得到第一个空闲簇号 */
void fo_printf_all_entry(u32 offset); /* 打印文件控制块 */

void fo_printf_one_clus(u32 offset); /* 打印一个簇的数据*/
void fo_printf_file_entry(u32 offset); /* 打印文件信息 */
void fo_printf_file_data(u16 Firstclus); /* 打印文件数据 */
u32 fo_clus_to_offset(u32 clus); /* 将簇号转成偏移 */
u16 fo_offset_to_clus(u32 offset); /* 将偏移转成簇号 */
void fo_printf_disk(u32 target,u32 start,u32 option); /* 打印磁盘数据 */
u16 fo_get_next_fat(u16 order); /* 返回下一个簇号 */
void fo_printf_entry_fat(u16 Firstorder); /* 打印一个条目的连续簇号 */
u32 fo_get_next_dir(u32 offset,char* target); /* 返回目录存储数据的位置 */
u16 fo_get_file_firstclus(u32 offset,char* target); /* 返回文件的首簇号 */
u32 fo_stricmp(char* src1,char* src2,u32 size); /* 不管大小写,字符数组比较 */
void fo_change_path(char* path,char* dir,int target_index,int flag); /* 改变路径 */
u32 fo_get_next_offset(u32 offset); /* 得到下一个可用的位置 */
u32 fo_is_short_file(char* filename,int* s_start,int* sum_pre); /* 判断是否是短文件条目文件 */
u32 fo_create_short_file(u32 start,char* filename); /* 创建短文件条目文件 */
u32 fo_remove_short_file(u32 start,char* filename); /* 删除短文件条目文件 */
u32 fo_create_short_dir(u32 start,char* dirname); /* 创建短文件条目目录 */
u32 fo_remove_short_dir(u32 start,char* dirname); /* 删除短文件条目目录 */
void fo_datetime(u16* date,u16* time,u32 flag); /* 时间转化 */
void fo_edit_fat(u16 target_order,u16 next_order); /* 修改某个fat表项的下一个表项,清空旧簇的数据 */
void fo_edit_mul_offset(u16 order); /* 修改多余簇的起始数据 */
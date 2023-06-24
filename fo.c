#include "fo.h"

extern char init_path[8];
extern u32 dir_mul_offset[16];
extern u32 sum; /* 大于1的时候多簇 */

//大部分数据都限制在一个簇里
//外层调用的时候可以写循环,多次传入起始偏移

/*
 * 修改多余一个簇的数据
 */
void fo_edit_mul_offset(u16 order){
    sum=0;
    u16 cur_order=order;
    while(cur_order<0xff8){
        dir_mul_offset[sum]=fo_clus_to_offset(cur_order);
        sum++;
        u16 next_order=fo_get_next_fat(cur_order);
        cur_order=next_order;
    }
}

/*
 * 打印文件控制块
 * 将一个簇的文件控制块全部打印
 */
void fo_printf_all_entry(u32 offset){
    //puts("控制块,还未实现");
    for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
        struct file_entry temp;
        disk_read(&temp,offset+i*FILE_SIZE,FILE_SIZE);
        if(strlen(temp.Dir_Name)==0) break;
        printf("第%d个:\t",i+1);
        if(temp.Dir_Name[0]!=0xe5){
            printf("%02hhx",0);
        }else{
            printf("%02hhx",temp.Dir_Name[0]);
        }
        printf("\t");
        for(int j=0;j<11;j++){
            printf("%c",temp.Dir_Name[j]);
        }
        printf("\t");
        printf("%02hhx %02hhx\t",temp.Dir_Attr,temp.Reserve[0]);
        fo_printf_entry_fat(temp.Dir_First_Clus);
        printf("\t");
        printf("%u\n",temp.Dir_File_Size);
    }
}

/*
 * 修改下一个fat表项,清空旧簇的数据
 * 奇数 保留高12位
 * 偶数 保留低12位
 * target_order: 需要修改的簇号
 * next_order: 下一个簇号
 */
void fo_edit_fat(u16 target_order,u16 next_order){
    if(target_order>0xff8||target_order<2) return ;
    if(next_order==1) return ;
    u32 offset=target_order*1.5,start=bpb.fat_start_offset;
    u16 fat,save;
    //读出
    disk_read(&fat,start+offset,sizeof(u16));
    //printf("%04x\n",fat);
    if(target_order%2){
        //奇数,需要保留低4位,修改高12位
        save=fat&0x000f;
        fat=next_order<<4;
        fat=fat|save;
    }else{
        //偶数,需要保留高4位,修改低12位
        save=fat&0xf000;
        fat=next_order;
        fat=fat|save;
    }
    //printf("%04x\n",fat);
    //写回,修改
    disk_write(&fat,start+offset,sizeof(u16));
    //如果新分配簇,清空旧簇的数据
    if(next_order>1){
        u32 old=fo_clus_to_offset(target_order);
        char empty[512];
        memset(empty,0,512);
        for(int i=0;i<bpb.sec_per_clus;i++){
            disk_write(empty,old+i*bpb.byte_per_sec,bpb.byte_per_sec);
        }
    }
}

/*
 * 遍历,得到第一个空闲簇号
 * 奇数 保留高12位
 * 偶数 保留低12位
 * u16: 0则没有
 */
u16 fo_get_next_free_fat(){
    u32 start=bpb.fat_start_offset;
    for(int i=2;i<0xff8;i++){
        u32 offset=i*1.5;
        u16 fat;
        disk_read(&fat,start+offset,sizeof(u16));
        if(i%2){
            fat>>=4;
        }else{
            fat&=0x0fff;
        }
        if(fat==0){
            return (u16)i;
        }
    }
    return 0;
}

/*
 * 打印文件分配表
 * 奇数 保留高12位
 * 偶数 保留低12位
 */
void fo_printf_all_fat(u32 size){
    if(size>4095){
        puts("ERROR: Cross the border!");
        return ;
    }
    u32 start=bpb.fat_start_offset;
    for(int i=0;i<=size;i++){
        u32 offset=i*1.5;
        u16 fat;
        disk_read(&fat,start+offset,sizeof(u16));
        printf("%04x ",fat);
        if(i%2){
            fat>>=4;
        }else{
            fat&=0x0fff;
        }
        printf("%03x ",fat);
        if(fat!=0x0){
            printf("簇号%04d已被占用  ",i);
        }else{
            printf("簇号%04d未被占用  ",i);
        }
        if((i-1)%4==0) printf("\n");
    }
    printf("\n");
}

/*
 * 删除短文件条目目录
 * u32: 1成功 0失败
 * 
 * 循环递归删除
 */
u32 fo_remove_short_dir(u32 start,char* dirname){
    if(dirname!=NULL){
        //判断是否是短文件条目
        if(strlen(dirname)>8){
            puts("ERROR: Is nor short directory!");
            return 0;
        }
        //sum: 0时全小写 0x08  =strlen(dirname)全大写 0x00
        int sum=0;
        for(int i=0;i<strlen(dirname);i++){
            sum+=isupper(dirname[i]);
        }
        if(sum>0&&sum<strlen(dirname)){
            puts("ERROR: Is nor short directory!");
            return 0;
        }
    }
    
    if(dirname!=NULL){
        //puts("个别");
        //寻找目录
        for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
            struct file_entry temp;
            disk_read(&temp,start+i*FILE_SIZE,FILE_SIZE);
            if(strlen(temp.Dir_Name)==0) return 0;
            if(temp.Dir_Name[0]!=0xe5&&temp.Dir_Attr==0x10&&fo_stricmp(dirname,temp.Dir_Name,8)){
                //printf("应该删除%s\n",temp.Dir_Name);
                temp.Dir_Name[0]=0xe5;
                disk_write(&temp,start+i*FILE_SIZE,FILE_SIZE);
                //清空子目录
                u16 cur_order=temp.Dir_First_Clus;
                u32 cur_offset=fo_clus_to_offset(cur_order);
                while(cur_order<0xff8){
                    u16 next_order=fo_get_next_fat(cur_order);
                    fo_edit_fat(cur_order,0);
                    fo_remove_short_dir(cur_offset,NULL);
                    cur_order=next_order;
                    cur_offset=fo_clus_to_offset(cur_order);
                }
                return 1;
            }
        }
        return 0;
    }else{
        //puts("全删");
        for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
            struct file_entry temp;
            disk_read(&temp,start+i*FILE_SIZE,FILE_SIZE);
            if(temp.Dir_Name[0]==0) return 0;
            //printf("%s\n",temp.Dir_Name);
            if(temp.Dir_Name[0]!=0xe5&&temp.Dir_Name[0]!=0x2e){
                if(temp.Dir_Attr==0x10){
                    //printf("应该删除%s\n",temp.Dir_Name);
                    temp.Dir_Name[0]=0xe5;
                    disk_write(&temp,start+i*FILE_SIZE,FILE_SIZE);
                    //多簇
                    u16 cur_order=temp.Dir_First_Clus;
                    u32 cur_offset=fo_clus_to_offset(cur_order);
                    while(cur_order<0xff8){
                        u16 next_order=fo_get_next_fat(cur_order);
                        fo_edit_fat(cur_order,0);
                        fo_remove_short_dir(cur_offset,NULL);
                        cur_order=next_order;
                        cur_offset=fo_clus_to_offset(cur_order);
                    }
                }else if(temp.Dir_Attr==0x20){
                    //printf("应该删除%s\n",temp.Dir_Name);
                    temp.Dir_Name[0]=0xe5;
                    disk_write(&temp,start+i*FILE_SIZE,FILE_SIZE);
                    if(temp.Dir_First_Clus){
                        //假如有多个簇号,下一个都需要修改为0(即未占用)
                        u16 cur_order=temp.Dir_First_Clus;
                        while(cur_order<0xff8){
                            u16 next_order=fo_get_next_fat(cur_order);
                            fo_edit_fat(cur_order,0);
                            cur_order=next_order;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/*
 * 删除短文件条目文件
 * u32: 1成功 0失败
 * 
 * 1.未有数据时,找到并把temp.Name[0]=0xe5
 * 2.有数据(有簇号)时,找到置为0xe5,簇号的下一个变为0(原来是0xfff)
 */
u32 fo_remove_short_file(u32 start,char* filename){
    //判断是否是短文件条目
    int s_start=0,sum_pre=0;
    if(!fo_is_short_file(filename,&s_start,&sum_pre)){
        puts("ERROR: Is not short file!");
        return 0;
    }

    //寻找文件
    for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
        struct file_entry temp;
        disk_read(&temp,start+i*FILE_SIZE,FILE_SIZE);
        if(strlen(temp.Dir_Name)==0) break;
        if(temp.Dir_Name[0]!=0xe5&&temp.Dir_Attr==0x20&&fo_stricmp(filename,temp.Dir_Name,11)){
            //puts("应该删除");
            temp.Dir_Name[0]=0xe5;
            disk_write(&temp,start+i*FILE_SIZE,FILE_SIZE);
            if(temp.Dir_First_Clus){
                //假如有多个簇号,下一个都需要修改为0(即未占用)
                u16 cur_order=temp.Dir_First_Clus;
                while(cur_order<0xff8){
                    u16 next_order=fo_get_next_fat(cur_order);
                    fo_edit_fat(cur_order,0);
                    cur_order=next_order;
                }
            }
            return 1;
        }
    }
    return 0;
}

/*
 * 创建短文件条目目录
 * 还要创建在子目录区创建. ..
 * test 0x08
 * TEXT 0x00
 * u32: 1成功 0失败
 */
u32 fo_create_short_dir(u32 start,char* dirname){
    //判断是否是短文件条目
    if(strlen(dirname)>8) puts("ERROR: Is nor short directory!");
    //sum: 0时全小写 0x08  =strlen(dirname)全大写 0x00
    int sum=0;
    for(int i=0;i<strlen(dirname);i++){
        sum+=isupper(dirname[i]);
    }
    if(sum>0&&sum<strlen(dirname)) puts("ERROR: Is nor short directory!");

    //填充分割线
    u8 seg[32];
    seg[0]=0xe5;seg[1]=0xb0;seg[2]=0x65;seg[3]=0xfa;seg[4]=0x5e;seg[5]=0x87;seg[6]=0x65;seg[7]=0xf6;
    seg[8]=0x4e;seg[9]=0x39;seg[10]=0x59;seg[11]=0x0f;seg[12]=0x00;seg[13]=0x75;seg[14]=0x00;seg[15]=0x00;
    for(int i=16;i<32;i++){
        if(i==26||i==27) seg[i]=0;
        else seg[i]=255;
    }

    //填充数据
    struct file_entry target,next,next_n;
    memset(&target,0,sizeof(target));
    memset(&next,0,sizeof(next));
    memset(&next_n,0,sizeof(next_n));
    
    //填充父目录结构体
    //先得到簇号
    target.Dir_First_Clus=fo_get_next_free_fat();
    //printf("%d\n",target.Dir_First_Clus);
    if(!target.Dir_First_Clus){
        puts("ERROR: None unused clus!");
        return 0;
    }
    //新建目录占一个簇,修改fat表,清空旧簇的数据
    fo_edit_fat(target.Dir_First_Clus,0xfff);
    //标识大小写位(win)
    int to_upper=0;
    if(!sum){
        target.Reserve[0]=0x08;
        to_upper='A'-'a';
    }else{
        target.Reserve[0]=0x00;
    }
    //转化目录名
    for(int i=0;i<strlen(dirname);i++){
        target.Dir_Name[i]=dirname[i]+to_upper;
    }
    for(int i=strlen(dirname);i<11;i++){
        target.Dir_Name[i]=' ';
    }
    target.Dir_Attr=0x10;
    //剩余的保留位
    for(int i=1;i<10;i++){
        target.Reserve[i]=0;
    }
    //时间转化
    fo_datetime(&target.Dir_WrtDate,&target.Dir_WrtTime,1);
    target.Dir_File_Size=0;

    //填充两个子目录
    //填充.子目录
    memcpy(&next,&target,sizeof(target));
    next.Dir_Name[0]='.';
    next.Reserve[0]=0;
    for(int i=1;i<8;i++) next.Dir_Name[i]=' ';
    //填充..子目录
    memcpy(&next_n,&target,sizeof(target));
    next_n.Dir_Name[0]='.';next_n.Dir_Name[1]='.';
    next_n.Reserve[0]=0;
    for(int i=2;i<8;i++) next_n.Dir_Name[i]=' ';
    next_n.Dir_First_Clus=0;

    //写入数据,子目录应该写在父目录下
    //写入分割线与父目录
    disk_write(seg,start,sizeof(seg));
    disk_write(&target,start+32,sizeof(target));
    //写入子目录
    u32 next_offset=fo_clus_to_offset(target.Dir_First_Clus);
    disk_write(&next,next_offset,sizeof(next));
    disk_write(&next_n,next_offset+32,sizeof(next_n));
}

/*
 * 创建短文件条目(8.3规范)
 * HA.txt 0x10
 * haha.txt 0x18
 * u32: 1成功 0失败
 */
u32 fo_create_short_file(u32 start,char* filename){
    //判断是否是短文件条目
    int s_start=0,sum_pre=0;
    if(!fo_is_short_file(filename,&s_start,&sum_pre)){
        puts("ERROR: Is not short file!");
        return 0;
    }

    //填充分割线
    u8 seg[32];
    seg[0]=0xe5;seg[1]=0xb0;seg[2]=0x65;seg[3]=0xfa;seg[4]=0x5e;seg[5]=0x87;seg[6]=0x65;seg[7]=0x2c;
    seg[8]=0x67;seg[9]=0x87;seg[10]=0x65;seg[11]=0x0f;seg[12]=0x00;seg[13]=0xd2;seg[14]=0x63;seg[15]=0x68;
    seg[16]='.';
    for(int i=17;i<32;i++){
        if(i>=28) seg[i]=255;
        else seg[i]=0;
    }
    for(int i=18,j=s_start;i<32&&j<strlen(filename);i+=2,j++){
        seg[i]=filename[j];
    }
    /*for(int i=0;i<16;i++){
        printf("%02hhx ",seg[i]);
    }
    printf("\n");*/

    //填充结构体
    struct file_entry target;
    memset(&target,0,sizeof(target));
    int to_upper=0;
    //标识大小写位(win)
    if(!sum_pre){
        target.Reserve[0]=0x18;
        to_upper='A'-'a';
    }else{
        target.Reserve[0]=0x10;
    }
    //转化文件名
    for(int i=0,j=0;i<8||j<s_start-1;i++,j++){
        if(j<s_start-1){
            target.Dir_Name[i]=filename[j]+to_upper;
        }else{
            target.Dir_Name[i]=' ';
        }
    }
    //转化扩展名
    for(int i=8,j=s_start;i<11||j<strlen(filename);i++,j++){
        if(j<strlen(filename)){
            target.Dir_Name[i]=filename[j]+to_upper;
        }else{
            target.Dir_Name[i]=' ';
        }
    }
    //属性位
    target.Dir_Attr=0x20;
    //剩余的保留位
    for(int i=1;i<10;i++){
        target.Reserve[i]=0;
    }
    //时间转化
    fo_datetime(&target.Dir_WrtDate,&target.Dir_WrtTime,1);
    //printf("%d %d\n",target.Dir_WrtDate,target.Dir_WrtTime);
    //fo_datetime(&target.Dir_WrtDate,&target.Dir_WrtTime,0);
    target.Dir_First_Clus=0;
    target.Dir_First_Clus=0;

    //写入数据
    disk_write(seg,start,32);
    disk_write(&target,start+32,sizeof(target));
    return 1;
}

/*
 * 判断是否是短文件条目
 * *filename: 目标文件名
 * *s_start: 扩展名的起始位置
 * *sum_pre: 判断文件名全小写或全大写
 * u32: 1是 0不是
 */
u32 fo_is_short_file(char* filename,int* s_start,int* sum_pre){
    int flag=0,prefix=0,suffix=-1;
    // 限制8.3规范,长度限制
    for(int i=0;i<strlen(filename);i++){
        if(filename[i]=='.'){
            flag++;
            *s_start=i+1;
            if(*s_start==1) return 0;
        }
        if(flag>1) return 0;
        if(!(*s_start)){
            prefix++;
        }else{
            suffix++;
        }
    }
    //printf("%d %d %d\n",prefix,suffix,s_start);
    if(!flag||prefix>8||suffix>3) return 0;

    //限制8.3规范,短文件条目(文件名全大写或全小写,规定扩展名全小写)
    //sum_pre: 0 全小写 0x18 s_start-1 全大写 0x10
    int sum_suf=0;
    for(int i=0;i<*s_start-1;i++){
        *sum_pre+=isupper(filename[i]);
    }
    for(int i=*s_start;i<strlen(filename);i++){
        sum_suf+=isupper(filename[i]);
    }
    //printf("%d %d\n",sum_pre,sum_suf);
    if(sum_suf||(*sum_pre&&(*sum_pre)<*s_start-1)) return 0;
    return 1;
}

/*
 * flag: 0时输出,非零时转化
 */
void fo_datetime(u16* date,u16* time,u32 flag){
    if(!flag){
        u16 year=*date&0xfe00;
        year=year>>9;
        u16 month=*date&0x1e0;
        month=month>>5;
        u16 day=*date&0x1f;
        printf("%04u/%02u/%02u  ",year+1980,month,day);
        u16 hour=*time&0xf7d8;
        hour=hour>>11;
        u16 minute=*time&0x7e0;
        minute=minute>>5;
        printf("%02u/%02u",hour,minute);
    }else{
        SYSTEMTIME _time;
        GetLocalTime(&_time);
        //printf("当前时间为：%2d:%2d:%2d %2d:%2d\n",_time.wYear,_time.wMonth,_time.wDay,_time.wHour,_time.wMinute);
        u16 year=(u16)(_time.wYear-1980);
        year=year<<9;
        *date=*date|year;
        u16 month=(u16)_time.wMonth;
        month=month<<5;
        *date=*date|month;
        u16 day=(u16)_time.wDay;
        *date=*date|day;
        u16 hour=(u16)_time.wHour;
        hour=hour<<11;
        *time=*time|hour;
        u16 minute=(u16)_time.wMinute;
        minute=minute<<5;
        *time=*time|minute;
    }
}

/*
 * 得到下一个可用的位置
 */
u32 fo_get_next_offset(u32 offset){
    u32 next_offset=0;
    for(int i=0;i<bpb.clus_size/16;i++){
        char temp[16];
        int sum=0;
        disk_read(temp,offset+i*16,16);
        for(int i=0;i<16;i++){
            if(temp[i]==0) sum++;
        }
        if(sum==16){
            next_offset=offset+i*16;
            break;
        }
    }
    return next_offset;
}

/* 
 * 切换上下文
 * 修改路径
 * flag: 1进入下级,0返回上级
 */
void fo_change_path(char* path,char* dir,int target_index,int flag){
    if(target_index==0){
        memcpy(path,init_path,8);
        return ;
    }
    int index=0;
    for(int i=0;i<1000;i++){
        if(path[i]=='\0') break;
        if(path[i]=='\\'){
            index++;
            if(index==target_index){
                i=i+1;
                for(int j=0;j<8;j++){
                    if(dir[j]=='\0') break;
                    path[i]=dir[j];
                    i++;
                }
                if(flag){
                    path[i]='\\';
                    path[++i]='>';
                    path[++i]='\0';
                }else{
                    path[i]='>';
                    path[++i]='\0';
                }
                break;
            }
        }
    }
}

/* 
 * 从当前偏移(目录)得到数据存储的位置
 * target: 目标目录
 */
u32 fo_get_next_dir(u32 offset,char* target){
    u32 dir_clus=0;
    for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
        struct file_entry temp;
        disk_read(&temp,offset+i*FILE_SIZE,FILE_SIZE);
        if(strlen(temp.Dir_Name)==0) break;
        if(temp.Dir_Name[0]!=0xe5&&temp.Dir_Attr==0x10&&fo_stricmp(target,temp.Dir_Name,8)){
            dir_clus=temp.Dir_First_Clus;
        }
    }
    return dir_clus>0?fo_clus_to_offset(dir_clus):dir_clus;
}

/*
 * 从当前目录得到目标文件的首簇号
 */
u16 fo_get_file_firstclus(u32 offset,char* target){
    u16 file_firstclus=0xffff;
    for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
        struct file_entry temp;
        disk_read(&temp,offset+i*FILE_SIZE,FILE_SIZE);
        if(strlen(temp.Dir_Name)==0) break;
        if(temp.Dir_Name[0]!=0xe5&&temp.Dir_Attr==0x20&&fo_stricmp(target,temp.Dir_Name,11)){
            file_firstclus=temp.Dir_First_Clus;
        }
    }
    return file_firstclus;
}

/* 
 * 字符数组比较
 * 不管大小写
 * size>8 src1: 目标 src2: 磁盘数据
 */
u32 fo_stricmp(char* src1,char* src2,u32 size){
    int sum=0;
    for(int i=0;i<8;i++){
        if(src1[i]!=' '&&src2[i]!=' '){
            if(!(src1[i]-src2[i]==0||src1[i]-src2[i]=='a'-'A'||src1[i]-src2[i]=='A'-'a')){
                return 0;
            }
            sum++;
        }
    }
    if(size>8){
        if(src1[sum]!='.') return 0;
        sum++;
        for(int i=8;i<size&&sum<strlen(src1);i++){
            if(!(src1[sum]-src2[i]==0||src1[sum]-src2[i]=='a'-'A'||src1[sum]-src2[i]=='A'-'a')){
                return 0;
            }
            sum++;
        }
    }
    return 1;
}

/*
 * 输出文件内容
 * Firstclus: 首簇号
 */
void fo_printf_file_data(u16 Firstclus){
    u16 cur_order=Firstclus;
    while(cur_order!=0xfff){
        fo_printf_one_clus(fo_clus_to_offset(cur_order));
        u16 next_order=fo_get_next_fat(cur_order);
        cur_order=next_order;
    }
    printf("\n");
}

/*
 * 打印一个条目的连续簇号
 * Firstorder: 首簇号
 */
void fo_printf_entry_fat(u16 Firstorder){
    if(Firstorder<2) return ;
    u16 cur_order=Firstorder;
    while(cur_order<0xff8){
        if(cur_order==Firstorder){
            printf("%d",cur_order);
        }else{
            printf("->%d",cur_order);
        }
        u16 next_order=fo_get_next_fat(cur_order);
        cur_order=next_order;
    }
    //printf("\n");
}

/* 
 * 根据簇号读取下一个簇号
 * 奇数 保留高12位
 * 偶数 保留低12位
 * order: 簇号
 */
u16 fo_get_next_fat(u16 order){
    u32 start=bpb.fat_start_offset;
    u32 offset=order*1.5;
    u16 fat;
    disk_read(&fat,start+offset,sizeof(u16));
    if(order%2){
        fat>>=4;
    }else{
        fat&=0x0fff;
    }
    return fat;
}

/*
 * 将偏移转成簇号
 */
u16 fo_offset_to_clus(u32 offset){
    return (offset-bpb.data_start_offset)/(bpb.sec_per_clus*bpb.byte_per_sec)+2;
}

/* 
 * 将簇号转成偏移
 */
u32 fo_clus_to_offset(u32 clus){
    return bpb.data_start_offset+(clus-2)*bpb.sec_per_clus*bpb.byte_per_sec;
}

/* 
 * 根据偏移输出数据
 * 一个簇的数据
 * offset: 起始偏移
 */
void fo_printf_one_clus(u32 offset){
    for(int i=0;i<bpb.sec_per_clus;i++){
        char buff[512];
        disk_read(buff,offset+i*bpb.byte_per_sec,bpb.byte_per_sec);
        for(int j=0;j<bpb.byte_per_sec;j++){
            if(buff[j]!=0) printf("%c",buff[j]);
        }
    }
}

/* 
 * 打印文件信息
 * 限制在一个簇里
 * offset: 起始偏移
 */
void fo_printf_file_entry(u32 offset){
    for(int i=0;i<bpb.clus_size/FILE_SIZE;i++){
        struct file_entry temp;
        disk_read(&temp,offset+i*FILE_SIZE,FILE_SIZE);
        if(strlen(temp.Dir_Name)==0) break;
        if(temp.Dir_Name[0]!=0xe5&&(temp.Dir_Attr==0x10||temp.Dir_Attr==0x20)){
            if(temp.Dir_Attr==0x10){
                printf("d-----\t\t");
            }else if(temp.Dir_Attr==0x20){
                printf("-a----\t\t");
            }
            fo_datetime(&temp.Dir_WrtDate,&temp.Dir_WrtTime,0);
            printf("\t\t");
            int sum=0,is_lower=0;
            if((temp.Reserve[0]&0x0f)==0x08) is_lower='a'-'A';
            for(int j=0;j<8;j++){
                if(temp.Dir_Name[j]!=' '){
                    printf("%c",temp.Dir_Name[j]+is_lower);
                }else{
                    sum++;
                }
            }
            if(temp.Dir_Attr==0x20){
                printf(".");
                for(int j=8;j<11;j++){
                    if(temp.Dir_Name[j]!=' ') printf("%c",temp.Dir_Name[j]+'a'-'A');
                    else sum++;
                }
            }else{
                printf("    ");
            }
            for(int j=0;j<sum;j++){
                printf(" ");
            }
            printf("\t");
            printf("%d\n",temp.Dir_File_Size);
        }
    }
}

/* 
 * 打印磁盘数据16进制
 * target: 磁盘容量
 * start: 起始偏移
 * option: 字节上限 =0 全部打印
 */
void fo_printf_disk(u32 target,u32 start,u32 option){
    u32 Max=target*1024*1024;
    if(start+option>Max){
        puts("ERROR: Memory overrun!");
        return ;
    }
    Max/=16;
    if(option!=0){
        Max=option/16;
    }
    u8 cur[16];
    memset(cur,0,sizeof(cur));
    int i=0;
    for(;i<Max;i++){
        disk_read(cur,start+i*16,sizeof(cur));
        printf("%08x   ",i*16+start);
        for(int j=0;j<16;j++){
            printf("%02hhx ",cur[j]);
            if(j==7) printf("  ");
        }
        printf("  |");
        for(int j=0;j<16;j++){
            if(cur[j]==0||cur[j]==0xff||cur[j]=='\n') printf(".");
            else printf("%c",cur[j]);
        }
        printf("|\n");
    }
    printf("%08x\n",i*16+start);
}
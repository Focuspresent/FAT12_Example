#include "fs.h"

#define DISK_CAPACITY 8 /* 磁盘容量/M */

extern u32 dir_offset[16];
extern u32 cur_index;
extern u32 dir_mul_offset[16];
extern u32 sum;
extern u32 init_dir_mul_offset[16];
extern u32 init_sum;
extern char path[1000];


/*
 * 无参: 规范打印文件信息
 * -f: 打印文件分配表
 * -c: 打印当前目录下的文件控制块
 */
void ls(const char *dir){
    const char s[2]="-";
    //一些选项参数
    int flag[4];
    /*
     * flag[0]: 0 时调用打印文件分配表 
     * flag[3]: 打印文件分配表的可选参数 -0xXXX 打印到哪个上限
     * flag[1]: 0 时调用打印文件控制块
     * flag[2]: 查询文件分分配表的可选参数 -X 选择哪个簇号
     */
    flag[0]=-1;flag[1]=-1;flag[2]=-1;flag[3]=0xfff;
    char* token;
    token=strtok((char*)dir,s);
    while(token!=NULL){
        if((strlen(token)==1||strlen(token)==2)&&token[0]=='f') flag[0]++;
        if(strlen(token)==1&&token[0]=='c') flag[1]++;
        if(atoi(token)) flag[2]=atoi(token);
        if(strlen(token)>2&&token[1]=='x') sscanf(token,"%x",&flag[3]);
        token=strtok(NULL,s);
    }
    if(!flag[0]&&!flag[1]){
        //两个功能混杂
        puts("ERROR: Too much parameter!");
    }else if(!flag[0]){
        //打印可选上限的分配表,不选则全打印
        if(flag[2]==-1) fo_printf_all_fat(flag[3]);
        else if(flag[2]>=2){
            //查询下一个簇号
            u16 fat=fo_get_next_fat(flag[2]);
            printf("簇号%d的下一个簇号是:%d\t",flag[2],fat);
            if(fat!=0x0){
                printf("已被占用\n");
            }else{
                printf("未被占用\n");
            }
        }
    }else if(!flag[1]){
        if(sum>1){
            for(int i=0;i<sum;i++){
                printf("第%d个位于%08x:\n",i+1,dir_mul_offset[i]);
                fo_printf_all_entry(dir_mul_offset[i]);
            }
        }else{
            fo_printf_all_entry(dir_offset[cur_index]);
        }
    }else if(strlen(dir)>0){
        //无效参数
        puts("ERROR: Parameter call error!");
    }else{
        //无参时查看当前文件下的文件与目录
        if(sum>1){
            for(int i=0;i<sum;i++){
                fo_printf_file_entry(dir_mul_offset[i]);
            }
        }else{
            fo_printf_file_entry(dir_offset[cur_index]);
        }
    }
}

void cd(const char *dir){
    if(!strcmp("..",dir)){
        if(cur_index>0){
            dir_offset[cur_index]=0;
            fo_change_path(path,(char*)"",cur_index,0);
            cur_index--;
            if(!cur_index){
                sum=init_sum;
                memcpy(dir_mul_offset,init_dir_mul_offset,sizeof(init_dir_mul_offset));
            }else{
                fo_edit_mul_offset(fo_offset_to_clus(dir_offset[cur_index]));
            }
        }
    }else if(!strcmp(".",dir)){
    }else if(!strcmp("\\",dir)){
        while(cur_index>0){
            dir_offset[cur_index]=0;
            cur_index--;
        }
        fo_change_path(path,(char*)dir,0,1);
        sum=init_sum;
        memcpy(dir_mul_offset,init_dir_mul_offset,sizeof(init_dir_mul_offset));
    }else{
        u32 next_offset=0;
        if(sum>1){
            for(int i=0;i<sum;i++){
                next_offset|=fo_get_next_dir(dir_mul_offset[i],(char*)dir);
            }
        }else{
            next_offset=fo_get_next_dir(dir_offset[cur_index],(char*)dir);
        }
        if(next_offset>0){
            cur_index++;
            dir_offset[cur_index]=next_offset;
            fo_edit_mul_offset(fo_offset_to_clus(next_offset));
            fo_change_path(path,(char*)dir,cur_index,1);
        }else{
            puts("ERROR: Directory does not exist!");
        }
    }
}

void cat(const char *dir){
    u16 target_firstclus=0;
    target_firstclus=fo_get_file_firstclus(dir_offset[cur_index],(char*)dir);
    if(target_firstclus==0){
        puts("File size is zero!");
    }else if(target_firstclus<0xffff){
        fo_printf_file_data(target_firstclus);
    }else{
        puts("ERROR: File does not exist!");
    }
}

void hexdump(const char *dir){
    const char s[2]="-";
    u32 start,option,flag=-1;
    char* token;
    token=strtok((char*)dir,s);
    while(token!=NULL){
        if(strlen(token)==2&&token[0]=='C') flag++;
        if(atoi(token)) option=atoi(token);
        if(strlen(token)>2&&token[1]=='x') sscanf(token,"%x",&start);
        token=strtok(NULL,s);
    }
    if(!flag){
        if(option){
            if(option%16==0) fo_printf_disk(DISK_CAPACITY,start,option);
            else puts("ERROR: -X need multiples of 16");
        }
        else puts("ERROR: Insufficient parameter!");
    }else{
        puts("ERROR: Insufficient parameter!");
    }
}

void mkdir(const char *dir){
    if(strlen(dir)==0){
        puts("ERROR: Parameter call error!");
        return ;
    }
    u32 next_offset=fs_get_mul_next_offset();
    if(!next_offset){
        return ;
    }
    u32 ans=0;
    if(sum>1){
        for(int i=0;i<sum;i++){
            ans|=fo_get_next_dir(dir_mul_offset[i],(char*)dir);
        }
    }else{
        ans=fo_get_next_dir(dir_offset[cur_index],(char*)dir);
    }
    if(ans!=0){
        puts("ERROR: Directroy does exist!");
        return ;
    }
    ans=0;
    ans=fo_create_short_dir(next_offset,(char*)dir);
    if(ans){
        puts("Create directory success!");
    }
}

void touch(const char *dir){
    if(strlen(dir)==0){
        puts("ERROR: Parameter call error!");
        return ;
    }
    u32 next_offset=fs_get_mul_next_offset();
    if(!next_offset){
        return ;
    }
    u32 ans=0;
    if(sum>1){
        for(int i=0;i<sum;i++){
            ans|=fo_get_file_firstclus(dir_mul_offset[i],(char*)dir);
        }
    }else{
        ans=fo_get_file_firstclus(dir_offset[cur_index],(char*)dir);
    }
    if(ans<0xffff){
        puts("ERROR: File does exist!");
        return ;
    }
    ans=0;
    ans=fo_create_short_file(next_offset,(char*)dir);
    if(ans){
        puts("Create file success!");
    }
}

/*
 * 参数:
 * 文件名: 删除文件
 * 目录名 -r: 递归删除目录
 * 
 */
void rm(const char *dir){
    const char s[2]="-";
    int flag=-1;
    char *token;
    char dirname[8];
    memset(dirname,' ',8);
    token=strtok((char*)dir,s);
    while(token!=NULL){
        if(strlen(token)==1&&token[0]=='r') flag++;
        else{ 
            for(int i=0;i<strlen(token)-1;i++){
                dirname[i]=token[i];
            }
            dirname[strlen(token)-1]='\0';
        }
        token=strtok(NULL,s);
    }
    if(flag>=0){
        u32 ans=0;
        if(sum>1){
            for(int i=0;i<sum;i++){
                ans|=fo_remove_short_dir(dir_mul_offset[i],dirname);
            }
        }else{
            ans=fo_remove_short_dir(dir_offset[cur_index],dirname);
        }
        if(!ans) puts("ERROR: Remove error!");
    }else if(strlen(dir)){
        u32 ans=0;
        if(sum>1){
            for(int i=0;i<sum;i++){
                ans|=fo_remove_short_file(dir_mul_offset[i],(char*)dir);
            }
        }else{
            ans=fo_remove_short_file(dir_offset[cur_index],(char*)dir);
        }
        if(!ans) puts("ERROR: Remove error!");
    }else {
        puts("ERROR: Insufficient parameter!");
    }
}

u32 fs_get_mul_next_offset(){
    u32 next_offset=0;
    if(!cur_index){
        //特判根目录
        int i;
        for(i=0;i<sum;i++){
            next_offset=fo_get_next_offset(dir_mul_offset[i]);
            if(next_offset) break;
        }
        //puts("在根");
        //假如到达根目录区上限
        if(i==sum&&(next_offset+64>dir_mul_offset[sum-1]+2048)){
            puts("ERROR: Meomry overturn!");
            next_offset=0;
        }
    }else if(sum>1){
        //数据区,还可以分配簇号,从最后开始找
        next_offset=fo_get_next_offset(dir_mul_offset[sum-1]);
        //假如当前簇不够，尝试分配簇
        if(next_offset+64>dir_mul_offset[sum-1]+2048){
            u16 cur_order=fo_offset_to_clus(dir_mul_offset[sum-1]);
            u16 free_order=fo_get_next_free_fat();
            if(free_order<0xff8){
                //先修改fat表
                fo_edit_fat(cur_order,free_order);
                fo_edit_fat(free_order,0xfff);
                //再修改多簇信息
                fo_edit_mul_offset(cur_order);
            }else{
                puts("ERROR: Meomry overturn!");
                next_offset=0;
            }
        }
    }else{
        next_offset=fo_get_next_offset(dir_offset[cur_index]);
        //假如当前簇不够，尝试分配簇(单簇才会进入,不需要特判根目录)
        if(next_offset+64>dir_offset[cur_index]+2048){
            u16 cur_order=fo_offset_to_clus(dir_offset[cur_index]);
            u16 free_order=fo_get_next_free_fat();
            if(free_order<0xff8){
                //先修改fat表
                fo_edit_fat(cur_order,free_order);
                fo_edit_fat(free_order,0xfff);
                //再修改多簇信息
                fo_edit_mul_offset(cur_order);
            }else{
                puts("ERROR: Meomry overturn!");
                next_offset=0;
            }
        }
    }
    return next_offset;
}
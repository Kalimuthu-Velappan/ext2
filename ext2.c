#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define block_size 1024                  //data block 1024 bytes
#define s_block_size 64                  //superblock 64 bytes
#define group_desc_size 32               //group description 32 bytes
#define inode_size 64                    //inode 64 bytes
#define inode_table_size (1024 * 8 * 64) //inode table

#define inode_counts (1024 * 8)              //inodes in total
#define block_counts (4 + 8 * 64 + 1024 * 8) //blocks in total
#define block_counts_per_block 256           //indirect data block
#define dir_counts_per_block 32              //dir counts

#define s_block_start 0                        //superblock address
#define group_desc_start 1024                  //group description address
#define block_bitmap_start 2048                //block bitmap address
#define inode_bitmap_start 3072                //inode bitmap address
#define inode_table_start 4096                 //inode table address
#define data_block_start (1024 * (8 * 64 + 4)) //data block address

#define volume_name "lips apple"
#define passwd "lips"

#define u_8 unsigned char
#define u_16 unsigned short
#define u_32 unsigned int

typedef struct super_block //64 bytes
{
    u_32 s_inodes_count;      //inodes in total
    u_32 s_blocks_count;      //blocks in total
    u_32 s_free_inodes_count; //free inodes in total
    u_32 s_free_blocks_count; //free blocks in total
    u_32 s_wtime;             //last write access to the file system
    char s_volume_name[16];   //volume name
    u_8 s_pad[28];
} Super_Block;

typedef struct group_desc //32 bytes
{
    u_32 bg_block_bitmap;      //block bitmap address
    u_32 bg_inode_bitmap;      //inode bitmap address
    u_32 bg_inode_table;       //inode table address
    u_32 bg_free_blocks_count; //free blocks in total
    u_32 bg_free_inodes_count; //free inodes in total
    u_32 bg_used_dirs_count;   //dirctories used in total
    u_8 bg_pad[8];
} Group_Desc;

typedef struct inode //64 bytes
{
    u_16 i_mode;   //indicate the access rights
    u_32 i_size;   //number of dir_entry * dir_entry size
    u_32 i_atime;  //access time
    u_32 i_ctime;  //create time
    u_32 i_mtime;  //modify time
    u_32 i_dtime;  //delete time
    u_16 i_blocks; //data blocks
    u_32 i_block[9];
    //1~8 contains direct block number
    //9 an indirect data block which contains direct block number
    char i_pad[4];
} Inode;

typedef struct dir_entry //32 bytes each block contains 32 dir_entry
{
    u_32 inode; //inode number
    u_16 rec_len;
    u_16 name_len; //file name lenth
    u_8 file_type; //1 : file  2 : directory
    char name[23]; //filename
} Dir_Entry;

u_32 fopen_table[16];          //contains inode number
char file_open_table[16][256]; //contasns file absolute path
char file_absolute_path[256];  //file absolute path
char current_path[256];        //current directory
char current_ttime[32];        //current time
u_32 last_alloc_inode = 1;     //last allocated inode number "/" took the first one
u_32 last_alloc_block = 0;     //last allocated block number
u_32 current_dir_inode;        //current directory inode number
u_8 current_dirlen;            //cureent directory path lenth

Super_Block s_block_buff[1];   //superblock buffer
Group_Desc gdt_buff[1];        //group description buffer
Inode inode_buff[1];           //inode buffer
Dir_Entry dir[32];             //directory enrty buffer
u_8 block_bitbuff[1024];       //block bitmap buffer
u_8 inode_bitbuff[1024];       //inode bitmap buffer
u_32 indirect_block_buff[256]; //indireact block buffer
char block_buff[1024];         //block buffer
char file_buff[264 * 1024];    //file buffer
FILE *fp;                      //a virtul disk pointer

void upload_super_block();         //upload super block
void load_super_block();           //load super block
void upload_group_desc();          //upload group description
void load_group_desc();            //load group description
void upload_block_bitmap();        //upload block bitmap
void load_block_bitmap();          //load block bitmap
void upload_inode_bitmap();        //upload inode bitmap
void load_inode_bitmap();          //load inode bitmap
void upload_dir(u_32 block_num);   //upload data block
void load_dir(u_32 block_num);     //load data block
void upload_block(u_32 block_num); //upload data block
void load_block(u_32 block_num);   //load data block
void upload_inode(u_32 inode_num); //upload inode
void load_inode(u_32 inode_num);   //load inode
u_32 alloc_block();                //allocate a block, return block number
u_32 alloc_inode();                //allocate an inode, return inode number
void remove_block(u_32 block_num); //remove a block
void remove_inode(u_32 inode_num); //remove an inode

u_32 current_time();    //set current time
char *time_now(u_32 t); //change time to string
void init_disk();       //initiate a disk

//arg=1:find an empty directory, arg=2:this block is empty?
int search_dir_entry(u_32 block_num, int arg);
//help fucntion search to finish seaching
int search_help(char filename[23], u_8 *type, u_32 tblock_num,
                u_32 *dir_num, u_32 *inode_num);
//search a file or directory,return file type,block number,directory enrty number,inode number
int search(char filename[23], u_8 *type, u_32 *block_num,
           u_32 *dir_num, u_32 *inode_num);
u_32 file_is_open(char filename[23]);       //find a file in file open table
void init_dir(u_32 dir_inode, u_8 dir_len); //initiate a directory

void help();                                      //show help
void ls_help(u_32 block_num);                     //help function ls
void ls();                                        //show name,user...in current directory
void mkdir(char filename[23]);                    //make a directory
void rmdir(char filename[23]);                    //delete a directory
void cd(char filename[23]);                       //change to a directory
void touch(char filename[23]);                    //make a file
void rm(char filename[23]);                       //delete a file
void cat(char filename[23]);                      //show file content
void open(char filename[23]);                     //open file, add to file open table
void close(char filename[23]);                    //close file, delete from file open table
void read(char filename[23]);                     //read file
void write(char filename[23]);                    //write file
void re_name(char oldname[23], char newname[23]); //rename a file
void chmod(char filename[23], u_16 mode);         //change a file's or a directory's mode
void login();                                     //login
void check();                                     //show disk infomation
void format();                                    //format the disk

void upload_super_block() //upload super block
{
    fseek(fp, s_block_start, SEEK_SET); //set file pointer to super block address
    fwrite(s_block_buff, s_block_size, 1, fp);
    fflush(fp);
}

void load_super_block() //load super block
{
    fseek(fp, s_block_start, SEEK_SET);
    fread(s_block_buff, s_block_size, 1, fp);
}

void upload_group_desc() //upload group description
{
    fseek(fp, group_desc_start, SEEK_SET);
    fwrite(gdt_buff, group_desc_size, 1, fp);
    fflush(fp);
}

void load_group_desc() //load group description
{
    fseek(fp, group_desc_start, SEEK_SET);
    fread(gdt_buff, group_desc_size, 1, fp);
}

void upload_block_bitmap() //upload block bitmap
{
    fseek(fp, block_bitmap_start, SEEK_SET);
    fwrite(block_bitbuff, block_size, 1, fp);
    fflush(fp);
}

void load_block_bitmap() //load block bitmap
{
    fseek(fp, block_bitmap_start, SEEK_SET);
    fread(block_bitbuff, block_size, 1, fp);
}

void upload_inode_bitmap() //upload inode bitmap
{
    fseek(fp, inode_bitmap_start, SEEK_SET);
    fwrite(inode_bitbuff, block_size, 1, fp);
    fflush(fp);
}

void load_inode_bitmap() //load inode bitmap
{
    fseek(fp, inode_bitmap_start, SEEK_SET);
    fread(inode_bitbuff, block_size, 1, fp);
}

void upload_dir(u_32 block_num) //upload data block
{
    fseek(fp, data_block_start + block_num * block_size, SEEK_SET);
    fwrite(dir, block_size, 1, fp);
    fflush(fp);
}

void load_dir(u_32 block_num) //load data block
{
    fseek(fp, data_block_start + block_num * block_size, SEEK_SET);
    fread(dir, block_size, 1, fp);
}

void upload_block(u_32 block_num) //upload data block
{
    fseek(fp, data_block_start + block_num * block_size, SEEK_SET);
    fwrite(block_buff, block_size, 1, fp);
    fflush(fp);
}

void load_block(u_32 block_num) //load data block
{
    fseek(fp, data_block_start + block_num * block_size, SEEK_SET);
    fread(block_buff, block_size, 1, fp);
}

void upload_inode(u_32 inode_num) //upload inode
{
    fseek(fp, inode_table_start + (inode_num - 1) * inode_size, SEEK_SET);
    fwrite(inode_buff, inode_size, 1, fp);
    fflush(fp);
}

void load_inode(u_32 inode_num) //load inode
{
    fseek(fp, inode_table_start + (inode_num - 1) * inode_size, SEEK_SET);
    fread(inode_buff, inode_size, 1, fp);
}

//byte              cur             cur+1
//bitbuff     1 1 1 1 1 1 1 1  1 1 0 0 0 0 0 0
//con                          0 0 1 0 0 0 0 0
//flag = 2
//data block number begin with 0
u_32 alloc_block() //allocate a block, return block number
{
    load_group_desc();
    if (gdt_buff->bg_free_blocks_count == 0)
    {
        printf("\033[1;31mThere is no block to be alloced!\n\033[m");
        return 0;
    }

    u_16 cur = last_alloc_block / 8;
    u_8 con = 128; // 1  0 0 0 0 0 0 0 0
    char flag = 0;
    load_block_bitmap();
    while (block_bitbuff[cur] == 255)
    {
        if (cur == 1023)
            cur = 0;
        else
            cur++;
    }
    while (block_bitbuff[cur] & con) //find an empty
    {
        con >>= 1;
        flag++;
    }
    block_bitbuff[cur] = block_bitbuff[cur] + con; //set 0 to 1
    last_alloc_block = cur * 8 + flag;
    upload_block_bitmap();
    load_super_block();
    s_block_buff->s_free_blocks_count--;
    upload_super_block();
    gdt_buff->bg_free_blocks_count--;
    upload_group_desc();
    return last_alloc_block;
}

//byte              cur             cur+1
//bitbuff     1 1 1 1 1 1 1 1  1 1 1 0 0 0 0 0
//con                          0 0 0 1 0 0 0 0
//flag = 3
//inode number begin with 1
u_32 alloc_inode() //allocate an inode, return inode number
{
    load_group_desc();
    if (gdt_buff->bg_free_inodes_count == 0)
    {
        printf("\033[1;31mThere is no inode to be alloced!\n\033[m");
        return 0;
    }

    u_16 cur = (last_alloc_inode - 1) / 8;
    u_8 con = 128;
    char flag = 0;
    load_inode_bitmap();
    while (inode_bitbuff[cur] == 255)
    {
        if (cur == 1023)
            cur = 0;
        else
            cur++;
    }
    while (inode_bitbuff[cur] & con)
    {
        con >>= 1;
        flag++;
    }
    inode_bitbuff[cur] = inode_bitbuff[cur] + con; //set 0 to 1
    last_alloc_inode = cur * 8 + flag + 1;
    upload_inode_bitmap();
    load_super_block();
    s_block_buff->s_free_inodes_count--;
    upload_super_block();
    gdt_buff->bg_free_inodes_count--;
    upload_group_desc();
    return last_alloc_inode;
}

//0 0 0 0  1 1 1 1 & 1 1 1 1  1 0 0 0 ~ = 1 1 1 1  0 1 1 1
void remove_block(u_32 block_num) //remove a block
{
    u_16 cur = block_num / 8;
    u_8 temp = block_num % 8;
    load_block_bitmap();
    block_bitbuff[cur] &= ~(255 >> temp) & (255 << (7 - temp));
    upload_block_bitmap();
    load_group_desc();
    gdt_buff->bg_free_blocks_count++;
    upload_group_desc();
    load_super_block();
    s_block_buff->s_free_blocks_count++;
    upload_super_block();
}

void remove_inode(u_32 inode_num) //remove an inode
{
    u_16 cur = (inode_num - 1) / 8;
    u_8 temp = (inode_num - 1) % 8;
    load_inode_bitmap();
    inode_bitbuff[cur] &= ~(255 >> temp) & (255 << (7 - temp));
    upload_inode_bitmap();
    load_group_desc();
    gdt_buff->bg_free_inodes_count++;
    upload_group_desc();
    load_super_block();
    s_block_buff->s_free_inodes_count++;
    upload_super_block();
}

u_32 current_time() //set current time
{
    time_t t = time(0);
    return (u_32)t;
}

char *time_now(u_32 t) //change time to string
{
    time_t ti = t;
    strftime(current_ttime, sizeof(current_ttime), "%Y-%m-%d %H:%M:%S", localtime(&ti));
    return current_ttime;
}

void init_disk() //initiate a disk
{
    printf("\033[1;37mWait...\033[m");
    if ((fp = fopen("./data.img", "w+b")) == NULL) //open disk
    {
        printf("\033[1;31mFail to open disk!\n\033[m");
        exit(0);
    }

    last_alloc_inode = 1; //reset last allocated inode number
    last_alloc_block = 0; //reset last allocated block bumber
    int i;
    for (i = 0; i < 16; i++) //reset file open table
    {
        fopen_table[i] = 0;
        memset(file_open_table[i], 0, 256);
    }
    memset(block_buff, 0, block_size); //clear block buffer
    fseek(fp, 0, SEEK_SET);            //use empty block buffer to format the disk
    for (i = 0; i < block_counts; i++)
        fwrite(block_buff, block_size, 1, fp);
    fflush(fp);

    //initiate super block
    load_super_block();
    s_block_buff->s_inodes_count = inode_counts;
    s_block_buff->s_blocks_count = block_counts;
    s_block_buff->s_free_inodes_count = inode_counts - 1;
    s_block_buff->s_free_blocks_count = block_counts;
    s_block_buff->s_wtime = current_time();
    strcpy(s_block_buff->s_volume_name, volume_name);
    upload_super_block();

    //initiate group descripion
    load_group_desc();
    gdt_buff->bg_block_bitmap = block_bitmap_start;
    gdt_buff->bg_inode_bitmap = inode_bitmap_start;
    gdt_buff->bg_inode_table = inode_table_start;
    gdt_buff->bg_free_inodes_count = inode_counts - 1;
    gdt_buff->bg_free_blocks_count = block_counts - 512 - 4;
    gdt_buff->bg_used_dirs_count = 0;
    upload_group_desc();

    load_block_bitmap();
    load_inode_bitmap();

    //initiate "/"
    load_inode(current_dir_inode);
    inode_buff->i_mode = 0x0007; //rwx
    inode_buff->i_size = 32 * 2;
    inode_buff->i_atime = current_time();
    inode_buff->i_ctime = current_time();
    inode_buff->i_mtime = current_time();
    inode_buff->i_dtime = 0;
    inode_buff->i_blocks = 1;
    inode_buff->i_block[0] = alloc_block();
    current_dir_inode = alloc_inode();
    current_dirlen = 1;
    upload_inode(current_dir_inode);

    //initiate "." ".."
    load_dir(0);
    dir[0].inode = dir[1].inode = current_dir_inode;
    dir[0].name_len = dir[1].name_len = 1;
    dir[0].file_type = dir[1].file_type = 2;
    strcpy(dir[0].name, ".");
    strcpy(dir[1].name, "..");
    for (i = 2; i < 32; i++)
        dir[i].inode = 0;
    upload_dir(inode_buff->i_block[0]);
    fclose(fp);
}

//arg=1:find an empty directory, arg=2:this block is empty?
int search_dir_entry(u_32 block_num, int arg)
{
    int i;
    load_dir(block_num);
    for (i = 0; i < 32; i++)
    {
        if (!dir[i].inode)
        {
            if (arg == 1)
                return i;
            else if (arg == 2)
                break;
        }
    }
    if (i == 32)
        return 1;
    return -1;
}

//help fucntion search to finish seaching
int search_help(char filename[23], u_8 *type, u_32 tblock_num, u_32 *dir_num, u_32 *inode_num)
{
    int i;
    load_dir(tblock_num);
    for (i = 0; i < dir_counts_per_block; i++)
    {
        if (dir[i].inode && !strcmp(dir[i].name, filename))
        {
            *type = dir[i].file_type;
            *inode_num = dir[i].inode;
            *dir_num = i;
            return 1;
        }
    }
    return 0;
}

//search a file or directory, return file type, block number, directory enrty number, inode number
int search(char filename[23], u_8 *type, u_32 *block_num, u_32 *dir_num, u_32 *inode_num)
{
    int i, j, flag;
    inode_buff->i_atime = current_time();
    for (i = 0; i < inode_buff->i_blocks; i++)
    {
        if (i < 8) //direct block
        {
            flag = search_help(filename, &(*type), inode_buff->i_block[i], dir_num, inode_num);
            if (flag)
            {
                *block_num = inode_buff->i_block[i];
                return 1;
            }
        }
        else //indirect block
        {
            int t = inode_buff->i_blocks - 8;
            load_block(inode_buff->i_block[8]);
            memcpy(indirect_block_buff, block_buff, block_size);
            for (j = 0; j < t; j++)
            {
                flag = search_help(filename, type, indirect_block_buff[j], dir_num, inode_num);
                if (flag)
                {
                    *block_num = indirect_block_buff[j];
                    return 1;
                }
            }
            break;
        }
    }
    return 0;
}

u_32 file_is_open(char filename[23]) //find a file in file open table
{
    int i;
    sprintf(file_absolute_path, "%s/%s", current_path, filename);
    for (i = 0; i < 16; i++)
        if (!strcmp(file_open_table[i], file_absolute_path))
            return fopen_table[i];
    return 0;
}

void init_dir(u_32 dir_inode, u_8 dir_len) //initiate a directory
{
    load_inode(dir_inode);
    inode_buff->i_mode = 0x0007; //rwx
    inode_buff->i_size = 32 * 2;
    inode_buff->i_atime = current_time();
    inode_buff->i_ctime = current_time();
    inode_buff->i_mtime = current_time();
    inode_buff->i_blocks = 1;
    inode_buff->i_block[0] = alloc_block();

    //initiate "." ".."
    load_dir(inode_buff->i_block[0]);
    dir[0].inode = dir_inode;
    dir[0].name_len = dir_len;
    dir[1].inode = current_dir_inode;
    dir[1].name_len = current_dirlen;
    dir[0].file_type = dir[1].file_type = 2;
    int i;
    for (i = 2; i < 32; i++) //clear
        dir[i].inode = 0;
    strcpy(dir[0].name, ".");
    strcpy(dir[1].name, "..");

    upload_dir(inode_buff->i_block[0]);
    upload_inode(dir_inode);
    load_group_desc();
    gdt_buff->bg_used_dirs_count++;
    upload_group_desc();
}

void help() //show help
{
    printf("\033[1;37m ----------------------------------------\n\033[m");
    printf("\033[1;37m |           Ext2 file system           |\n\033[m");
    printf("\033[1;37m |  0. help           : help            |\n\033[m");
    printf("\033[1;37m |  1. change dir     : cd dirname      |\n\033[m");
    printf("\033[1;37m |  2. list file      : ls              |\n\033[m");
    printf("\033[1;37m |  3. show file      : cat filename    |\n\033[m");
    printf("\033[1;37m |  4. create dir     : mkdir dirname   |\n\033[m");
    printf("\033[1;37m |  5. delete dir     : rmdir dirname   |\n\033[m");
    printf("\033[1;37m |  6. create file    : touch filename  |\n\033[m");
    printf("\033[1;37m |  7. delete file    : rm filename     |\n\033[m");
    printf("\033[1;37m |  8. rename file    : rename filename |\n\033[m");
    printf("\033[1;37m |  9. open file      : open filename   |\n\033[m");
    printf("\033[1;37m | 10. close file     : close filename  |\n\033[m");
    printf("\033[1;37m | 11. read file      : read filename   |\n\033[m");
    printf("\033[1;37m | 12. write file     : write filename  |\n\033[m");
    printf("\033[1;37m | 13. change mode    : chmod filename  |\n\033[m");
    printf("\033[1;37m | 14. log out        : quit            |\n\033[m");
    printf("\033[1;37m | 15. format disk    : format          |\n\033[m");
    printf("\033[1;37m | 16. check disk     : check           |\n\033[m");
    printf("\033[1;37m ----------------------------------------\n\033[m");
}

void ls_help(u_32 block_num) //help ls function
{
    int j, k, t;
    char full_mode[] = "rwx";
    load_dir(block_num);
    for (j = 0; j < 32; j++)
    {
        if (dir[j].inode)
        {
            if (dir[j].file_type == 1)
            {
                printf("\033[1;33m%-15s\033[m", dir[j].name);
                printf("\033[1;33m%-12s\033[m", " File ");
            }
            else if (dir[j].file_type == 2)
            {
                printf("\033[1;36m%-15s\033[m", dir[j].name);
                printf("\033[1;36m%-12s\033[m", " Dir ");
            }
            printf("\033[1;37m%-12s\033[m", "lips");
            load_inode(dir[j].inode);
            inode_buff->i_atime = current_time();
            upload_inode(dir[j].inode);
            printf("\033[1;37m%-25s\033[m", time_now(inode_buff->i_atime));
            printf("\033[1;37m%-25s\033[m", time_now(inode_buff->i_ctime));
            printf("\033[1;37m%-25s\033[m", time_now(inode_buff->i_mtime));
            for (k = 0, t = 4; k < 3; k++, t >>= 1)
                if (!(inode_buff->i_mode & t))
                    full_mode[k] = '-';
            printf("\033[1;37m%-14s\033[m", full_mode);
            printf("\033[1;37m%d\n\033[m", inode_buff->i_size);
        }
        strcpy(full_mode, "rwx");
    }
    load_inode(current_dir_inode); //re load current directory inode
}

void ls() //show name,user...in current directory
{
    printf("\033[1;37m%-15s %-10s %-15s %-24s %-24s %-19s %-13s %-10s\n\033[m", "name", "type",
           "user", "access time", "create time", "modify time", "mode", "size");
    load_inode(current_dir_inode);
    inode_buff->i_atime = current_time();

    int i, j;
    for (i = 0; i < inode_buff->i_blocks; i++)
    {
        if (i < 8)
            ls_help(inode_buff->i_block[i]);
        else
        {
            int t = inode_buff->i_blocks - 8;
            load_block(inode_buff->i_block[8]);
            memcpy(indirect_block_buff, block_buff, block_size);
            for (j = 0; j < t; j++)
                ls_help(indirect_block_buff[j]);
            break;
        }
    }
}

void mkdir(char filename[23]) //make a directory
{
    load_inode(current_dir_inode);
    if (!(inode_buff->i_mode & 0x0001)) //no execute righst
    {
        printf("\033[1;31mMkdir denied!\n\033[m");
        return;
    }
    if (inode_buff->i_size == block_size * 264) //directory is full
    {
        printf("\033[1;31mThere is no room to be alloced!\n\033[m");
        return;
    }

    u_32 block_num, dir_num, inode_num, dir_inode;
    u_8 type;
    int i, j, k, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (!flag)
    {
        inode_buff->i_atime = current_time();
        if (inode_buff->i_size != block_size * inode_buff->i_blocks) //free block
        {
            for (i = 0, flag = 0; !flag && i < inode_buff->i_blocks; i++)
            {
                if (i < 8)
                {
                    j = search_dir_entry(inode_buff->i_block[i], 1);
                    break;
                }
                else
                {
                    int t = inode_buff->i_blocks - 8;
                    load_block(inode_buff->i_block[8]);
                    memcpy(indirect_block_buff, block_buff, block_size);
                    for (k = 0; k < t; k++)
                    {
                        j = search_dir_entry(indirect_block_buff[t], 1);
                        flag = 1; //use flag to jump out the loop
                        break;
                    }
                }
            }
            dir_inode = alloc_inode();
            dir[j].inode = dir_inode;
            dir[j].name_len = strlen(filename);
            dir[j].file_type = 2;
            strcpy(dir[j].name, filename);
            if (i <= 8)
                upload_dir(inode_buff->i_block[i]);
            else
                upload_dir(indirect_block_buff[k]);
        }
        else //all blocks are full
        {
            if (inode_buff->i_blocks < 8)
            {
                inode_buff->i_block[inode_buff->i_blocks] = alloc_block();
                inode_buff->i_blocks++;
                load_dir(inode_buff->i_block[inode_buff->i_blocks - 1]);
                dir_inode = alloc_inode();
                dir[0].inode = dir_inode;
                dir[0].name_len = strlen(filename);
                dir[0].file_type = 2;
                strcpy(dir[0].name, filename);
                for (j = 1; j < 32; j++)
                    dir[j].inode = 0;
                upload_dir(inode_buff->i_block[inode_buff->i_blocks - 1]);
            }
            else
            {
                int t = inode_buff->i_blocks - 8;
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                indirect_block_buff[t] = alloc_block();
                memcpy(block_buff, indirect_block_buff, block_size);
                upload_block(inode_buff->i_block[8]);
                inode_buff->i_blocks++;

                load_dir(indirect_block_buff[t]);
                dir_inode = alloc_inode();
                dir[0].inode = dir_inode;
                dir[0].name_len = strlen(filename);
                dir[0].file_type = 2;
                strcpy(dir[0].name, filename);
                for (j = 1; j < 32; j++)
                    dir[j].inode = 0;
                upload_dir(indirect_block_buff[t]);
            }
        }
        inode_buff->i_mtime = current_time();
        inode_buff->i_size += 32;
        upload_inode(current_dir_inode);
        init_dir(dir_inode, strlen(filename));
    }
    else if (type == 1)
        printf("\033[1;31mU can not mkdir a dir same to a file!\n\033[m");
    else
        printf("\033[1;31mU can not mkdir two same dirs!\n\033[m");
}

void rmdir(char filename[23]) //delete a directory
{
    load_inode(current_dir_inode);
    if (!strcmp(filename, ".") || !strcmp(filename, ".."))
    {
        printf("\033[1;31mU can not delete this dir!\n\033[m");
        return;
    }

    u_32 block_num, dir_num, inode_num;
    u_8 type;
    int i, j, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag && type == 2)
    {
        load_inode(inode_num);
        if (!(inode_buff->i_mode & 0x0001)) //no execute right
        {
            printf("\033[1;31mRmdir denied!\n\033[m");
            return;
        }
        inode_buff->i_atime = current_time();
        if (inode_buff->i_size == 64)
        {
            inode_buff->i_mode = 0;
            inode_buff->i_size = 0;
            inode_buff->i_blocks = 0;

            //delete "." ".."
            load_dir(inode_buff->i_block[0]);
            dir[0].inode = 0;
            dir[1].inode = 0;
            upload_dir(inode_buff->i_block[0]);
            remove_block(inode_buff->i_block[0]);
            remove_inode(inode_num);

            load_inode(current_dir_inode);
            inode_buff->i_atime = current_time();
            load_dir(block_num);
            dir[dir_num].inode = 0;
            upload_dir(block_num);
            inode_buff->i_size -= 32;

            for (i = inode_buff->i_blocks; i > 1;) //delete block form the last one
            {
                if (i <= 8)
                {
                    if (!search_dir_entry(inode_buff->i_block[i - 1], 2))
                    {
                        remove_block(inode_buff->i_block[i - 1]);
                        inode_buff->i_blocks--;
                        i--;
                    }
                }
                else
                {
                    int t = inode_buff->i_blocks - 8;
                    load_block(inode_buff->i_block[8]);
                    memcpy(indirect_block_buff, block_buff, block_size);
                    for (j = 0; j < t; j++)
                    {
                        if (!search_dir_entry(indirect_block_buff[j], 2))
                        {
                            remove_block(indirect_block_buff[j]);
                            inode_buff->i_blocks--;
                            i--;
                        }
                    }
                }
            }
            inode_buff->i_mtime = current_time();
            upload_inode(current_dir_inode);
        }
        else
            printf("\033[1;31mDir is not empty!\n\033[m");
    }
    else if (type == 1)
        printf("\033[1;31mU can not rmdir a file! Use rm instead!\n\033[m");
    else
        printf("\033[1;31mCan not find the dir!\n\033[m");
}

void cd(char filename[23]) //change to a directory
{
    load_inode(current_dir_inode);
    if (!strcmp(filename, "."))
        return;
    else if (!strcmp(current_path, "/") && !strcmp(filename, ".."))
        return;

    u_32 block_num, dir_num, inode_num;
    u_8 type;
    int flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag && type == 2)
    {
        load_inode(inode_num);
        if (!(inode_buff->i_mode & 0x0004)) //no read right
        {
            printf("\033[1;31mCd denied!\n\033[m");
            return;
        }

        inode_buff->i_atime = current_time();
        if (!strcmp(filename, ".."))
        {
            current_dir_inode = inode_num;
            //for example /123/abc/
            //  /123/
            //set 'a' to '\0'
            current_path[strlen(current_path) - dir[dir_num - 1].name_len - 1] = '\0';
            current_dirlen = dir[dir_num].name_len;
        }
        else
        {
            current_dir_inode = inode_num;
            strcat(current_path, filename);
            strcat(current_path, "/");
            current_dirlen = dir[dir_num].name_len;
        }
    }
    else if (type == 1)
        printf("\033[1;31mU can not cd a file!\n\033[m");
    else
        printf("\033[1;31mCan not find the dir!\n\033[m");
}

void touch(char filename[23]) //make a file
{
    load_dir(current_dir_inode);
    if (!(inode_buff->i_mode & 0x0001)) //no execute right
    {
        printf("\033[1;31mTouch denied!\n\033[m");
        return;
    }
    if (inode_buff->i_size == block_size * 264) //current directory is full
    {
        printf("\033[1;31mThere is no room to be alloced!\n\033[m");
        return;
    }

    u_32 block_num, dir_num, inode_num, file_inode;
    u_8 type;
    int i, j, k, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (!flag)
    {
        inode_buff->i_atime = current_time();
        if (inode_buff->i_size != block_size * inode_buff->i_blocks)
        {
            for (i = 0, flag = 0; !flag && i < inode_buff->i_blocks; i++)
            {
                if (i < 8)
                {
                    j = search_dir_entry(inode_buff->i_block[i], 1);
                    break;
                }
                else
                {
                    int t = inode_buff->i_blocks - 8;
                    load_block(inode_buff->i_block[8]);
                    memcpy(indirect_block_buff, block_buff, block_size);
                    for (k = 0; k < t; k++)
                    {
                        j = search_dir_entry(indirect_block_buff[t], 1);
                        flag = 1;
                        break;
                    }
                }
            }
            file_inode = alloc_inode();
            dir[j].inode = file_inode;
            dir[j].name_len = strlen(filename);
            dir[j].file_type = 1;
            strcpy(dir[j].name, filename);
            if (i <= 8)
                upload_dir(inode_buff->i_block[i]);
            else
                upload_dir(indirect_block_buff[k]);
        }
        else
        {
            if (inode_buff->i_blocks < 8)
            {
                inode_buff->i_block[inode_buff->i_blocks] = alloc_block();
                inode_buff->i_blocks++;
                load_dir(inode_buff->i_block[inode_buff->i_blocks - 1]);
                file_inode = alloc_inode();
                dir[0].inode = file_inode;
                dir[0].name_len = strlen(filename);
                dir[0].file_type = 2;
                strcpy(dir[0].name, filename);
                for (j = 1; j < 32; j++)
                    dir[j].inode = 0;
                upload_dir(inode_buff->i_block[inode_buff->i_blocks - 1]);
            }
            else
            {
                int t = inode_buff->i_blocks - 8;
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                indirect_block_buff[t] = alloc_block();
                memcpy(block_buff, indirect_block_buff, block_size);
                upload_block(inode_buff->i_block[8]);
                inode_buff->i_blocks++;

                load_dir(indirect_block_buff[t]);
                file_inode = alloc_inode();
                dir[0].inode = file_inode;
                dir[0].name_len = strlen(filename);
                dir[0].file_type = 2;
                strcpy(dir[0].name, filename);
                for (j = 1; j < 32; j++)
                    dir[j].inode = 0;
                upload_dir(indirect_block_buff[t]);
            }
        }
        inode_buff->i_size += 32;
        upload_inode(current_dir_inode);
        load_inode(file_inode);
        inode_buff->i_mode = 0x0007; //rwx
        inode_buff->i_size = 0;
        inode_buff->i_atime = current_time();
        inode_buff->i_ctime = current_time();
        inode_buff->i_mtime = current_time();
        inode_buff->i_blocks = 0;
        upload_inode(file_inode);
    }
    else if (type == 2)
        printf("\033[1;31mU can not touch a file same to a dir!\n\033[m");
    else
        printf("\033[1;31mU can not touch two same files!\n\033[m");
}

void rm(char filename[23]) //delete a file
{
    load_dir(current_dir_inode);
    u_32 block_num, dir_num, inode_num;
    u_8 type;
    int i, j, k, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag && type == 1)
    {
        load_inode(inode_num);
        if (!(inode_buff->i_mode & 0x0001)) //no execute right
        {
            printf("\033[1;31mRm denied!\n\033[m");
            return;
        }

        if (file_is_open(filename))
            close(filename);
        
        load_dir(block_num);
        inode_buff->i_atime = current_time();
        for (i = 0; i < inode_buff->i_blocks; i++) //remove all blocks
        {
            if (i < 8)
                remove_block(inode_buff->i_block[i]);
            else
            {
                int t = inode_buff->i_blocks - 8;
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                for (j = 0; j < t; j++)
                    remove_block(indirect_block_buff[j]);
                break;
            }
        }
        inode_buff->i_mode = 0;
        inode_buff->i_size = 0;
        inode_buff->i_blocks = 0;
        remove_inode(inode_num);

        load_inode(current_dir_inode);
        inode_buff->i_atime = current_time();
        inode_buff->i_size -= 32;
        load_dir(block_num);
        dir[dir_num].inode = 0;
        upload_dir(block_num);

        for (i = inode_buff->i_blocks; i > 1;)
        {
            if (i <= 8)
            {
                if (!search_dir_entry(inode_buff->i_block[i - 1], 2))
                {
                    remove_block(inode_buff->i_block[i - 1]);
                    inode_buff->i_blocks--;
                    i--;
                }
            }
            else
            {
                int t = inode_buff->i_blocks - 8;
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                for (j = 0; j < t; j++)
                {
                    if (!search_dir_entry(indirect_block_buff[j], 2))
                    {
                        remove_block(indirect_block_buff[j]);
                        inode_buff->i_blocks--;
                        i--;
                    }
                }
            }
        }
        inode_buff->i_mtime = current_time();
        upload_inode(current_dir_inode);
        printf("\033[1;31mFile has been deleted!\n\033[m");
    }
    else if (type == 2)
        printf("\033[1;31mU can not rm a dir! Use rmdir instead!\n\033[m");
    else
        printf("\033[1;31mCan not find the file!\n\033[m");
}

void cat(char filename[23]) //show file content
{
    open(filename);
    read(filename);
}

void open(char filename[23]) //open file, add to file open table
{
    load_inode(current_dir_inode);
    u_32 inode_num = file_is_open(filename);
    if (inode_num)
    {
        printf("\033[1;31mFile has been opened!\n\033[m");
        return;
    }

    u_32 block_num, dir_num;
    u_8 type;
    int i, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag && type == 1)
    {
        load_inode(inode_num);
        if (!(inode_buff->i_mode & 0x0001)) //no execute right
        {
            printf("\033[1;31mOpen denied!\n\033[m");
            return;
        }

        i = 0;
        while (fopen_table[i])
            i++;
        sprintf(file_absolute_path, "%s/%s", current_path, filename);
        strcpy(file_open_table[i], file_absolute_path);
        fopen_table[i] = inode_num;
        printf("\033[1;31mFile opened!\n\033[m");
    }
    else if (type == 2)
        printf("\033[1;31mU can not open a dir! Use cd instead!\n\033[m");
    else
        printf("\033[1;31mCan not find the file!\n\033[m");
}

void close(char filename[23]) //close file, delete from file open table
{
    load_inode(current_dir_inode);
    u_32 inode_num = file_is_open(filename);
    if (!inode_num)
    {
        printf("\033[1;31mFile has been closed!\n\033[m");
        return;
    }

    u_32 block_num, dir_num;
    u_8 type;
    int i, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag && type == 1)
    {
        i = 0;
        while (fopen_table[i] != inode_num)
            i++;
        fopen_table[i] = 0;
        memset(file_open_table[i], 0, 256);
        printf("\033[1;31mFile closed!\n\033[m");
    }
    else if (type == 2)
        printf("\033[1;31mU can not close a dir!\n\033[m");
    else
        printf("\033[1;31mCan not find the file!\n\033[m");
}

void read(char filename[23]) //read file
{
    load_inode(current_dir_inode);
    u_32 inode_num = file_is_open(filename);
    int i, j, k;
    if (inode_num)
        load_inode(inode_num);
    else
    {
        u_32 block_num, dir_num;
        u_8 type;
        int flag;
        flag = search(filename, &type, &block_num, &dir_num, &inode_num);

        if (flag && type == 1)
            load_inode(inode_num);
        else if (type == 2)
        {
            printf("\033[1;31mU can not read a dir!\n\033[m");
            return;
        }
        else
        {
            printf("\033[1;31mU should touch it first!\n\033[m");
            return;
        }
    }

    if (!(inode_buff->i_mode & 0x0004)) //no read right
    {
        printf("\033[1;31mRead denied!\n\033[m");
        return;
    }
    inode_buff->i_atime = current_time();

    for (i = 0; i < inode_buff->i_blocks; i++)
    {
        if (i < 8)
        {
            load_block(inode_buff->i_block[i]);
            for (k = 0; k < inode_buff->i_size - i * block_size; k++)
                printf("%c", block_buff[k]);
        }
        else
        {
            int t = inode_buff->i_blocks - 8;
            load_block(inode_buff->i_block[8]);
            memcpy(indirect_block_buff, block_buff, block_size);
            for (j = 0; j < t; j++)
            {
                load_block(indirect_block_buff[j]);
                for (k = 0; k < inode_buff->i_size - i * block_size; k++)
                    printf("%c", block_buff[k]);
            }
            break;
        }
    }
    if (i == 0)
    {
        printf("\033[1;31mFile is empty!\n\033[m");
        return;
    }
    printf("\n");
}

void write(char filename[23]) //write file
{
    load_inode(current_dir_inode);
    u_32 inode_num = file_is_open(filename);
    int blocks_in_need, len, cur, size;
    int i, j, t;
    if (inode_num)
        load_inode(inode_num);
    else
    {
        u_32 block_num, dir_num;
        u_8 type;
        int flag;
        flag = search(filename, &type, &block_num, &dir_num, &inode_num);
        if (flag && type == 1)
            load_inode(inode_num);
        else if (type == 2)
        {
            printf("\033[1;31mU can not write a dir!\n\033[m");
            return;
        }
        else
        {
            printf("\033[1;31mCan not find the file!\n\033[m");
            return;
        }
    }

    if (!(inode_buff->i_mode & 0x0002)) //no write right
    {
        printf("\033[1;31mWrite denied!\n\033[m");
        return;
    }
    inode_buff->i_atime = current_time();
    fflush(stdin); //ready

    for (i = 0; i < inode_buff->i_blocks; i++)
    {
        if (i < 8)
        {
            load_block(inode_buff->i_block[i]);
            if (i != inode_buff->i_blocks - 1)
                memcpy(file_buff + i * block_size, block_buff, i * block_size);
            else
                memcpy(file_buff + i * block_size, block_buff, inode_buff->i_size - i * block_size);
            remove_block(inode_buff->i_block[i]);
        }
        else
        {
            int t = inode_buff->i_blocks - 8;
            load_block(inode_buff->i_block[8]);
            memcpy(indirect_block_buff, block_buff, block_size);
            for (j = 0; j < t; j++)
            {
                load_block(indirect_block_buff[j]);
                if (j + 8 != inode_buff->i_blocks - 1)
                    memcpy(file_buff + (j + 8) * block_size, block_buff, (j + 8) * block_size);
                else
                    memcpy(file_buff + (j + 8) * block_size, block_buff, inode_buff->i_size - (j + 8) * block_size);
                remove_block(indirect_block_buff[j]);
            }
        }
    }
    inode_buff->i_blocks = 0;
    for (i = 0; i < inode_buff->i_size; i++)
        printf("%c", file_buff[i]);

    printf("\033[1;32m\nWhere would u like to write(0~%d):\033[m", inode_buff->i_size);
    scanf("%d", &cur);
    size = cur;
    printf("\033[1;32mEnd with '#'\n\033[m");
    while (1)
    {
        file_buff[size] = getchar();
        if (size >= 264 * block_size - 1) //overrange
        {
            printf("\033[1;31mMax size is 264KB!\n\033[m");
            break;
        }
        else if (file_buff[size] == '\n')
            size--;
        else if (file_buff[size] == '#')
        {
            file_buff[size] = '\0';
            break;
        }
        size++;
    }
    len = strlen(file_buff);
    blocks_in_need = len / block_size;
    if (len % block_size)
        blocks_in_need++;
    if (blocks_in_need <= 264)
    {
        if (inode_buff->i_blocks < 8)
        {
            if (inode_buff->i_blocks <= blocks_in_need)
            {
                while (inode_buff->i_blocks < blocks_in_need)
                {
                    inode_buff->i_block[inode_buff->i_blocks] = alloc_block();
                    inode_buff->i_blocks++;
                }
            }
            else
            {
                while (inode_buff->i_blocks > blocks_in_need)
                {
                    remove_block(inode_buff->i_block[inode_buff->i_blocks - 1]);
                    inode_buff->i_blocks--;
                }
            }
        }
        else
        {
            if (inode_buff->i_blocks <= blocks_in_need)
            {
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                while (inode_buff->i_blocks < blocks_in_need)
                {
                    t = inode_buff->i_blocks - 8;
                    indirect_block_buff[t] = alloc_block();
                    inode_buff->i_blocks++;
                }
            }
            else
            {
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                while (inode_buff->i_blocks > blocks_in_need)
                {
                    t = inode_buff->i_blocks - 8;
                    remove_block(indirect_block_buff[t - 1]);
                    inode_buff->i_blocks--;
                }
            }
        }

        for (i = 0; i < blocks_in_need; i++)
        {
            if (i < 8)
            {
                load_block(inode_buff->i_block[i]);
                if (i != blocks_in_need - 1)
                    memcpy(block_buff, file_buff + i * block_size, block_size);
                else
                {
                    memcpy(block_buff, file_buff + i * block_size, len - i * block_size);
                    inode_buff->i_size = len;
                }
                upload_block(inode_buff->i_block[i]);
            }
            else
            {
                t = inode_buff->i_blocks - 8;
                load_block(inode_buff->i_block[8]);
                memcpy(indirect_block_buff, block_buff, block_size);
                for (j = 0; j < t; j++)
                {
                    load_block(indirect_block_buff[j]);
                    if ((j + 8) != blocks_in_need - 1)
                        memcpy(block_buff, file_buff + (j + 8) * block_size, block_size);
                    else
                    {
                        memcpy(block_buff, file_buff + (j + 8) * block_size, len - (j + 8) * block_size);
                        inode_buff->i_size = len;
                    }
                }
            }
        }
        inode_buff->i_mtime = current_time();
        upload_inode(inode_num);
    }
    else
        printf("\033[1;31mMax size is 8KB!\n\033[m");
}

void re_name(char oldname[23], char newname[23]) //rename a file
{
    load_inode(current_dir_inode);
    if (!(inode_buff->i_mode & 0x0001)) //no execute right
    {
        printf("\033[1;31mRename denied!\n\033[m");
        return;
    }

    u_32 block_num1, dir_num1, inode_num1;
    u_32 block_num2, dir_num2, inode_num2;
    u_8 type1, type2;
    int flag1, flag2;
    flag1 = search(oldname, &type1, &block_num1, &dir_num1, &inode_num1);

    if (flag1)
    {
        flag2 = search(newname, &type2, &block_num2, &dir_num2, &inode_num2);

        if (!flag2)
        {
            load_inode(current_dir_inode);
            inode_buff->i_atime = current_time();
            inode_buff->i_mtime = current_time();
            load_dir(block_num1);
            strcpy(dir[dir_num1].name, newname);
            dir[dir_num1].name_len = strlen(newname);
            upload_dir(block_num1);
        }
        else if (flag2 && type2 == 2)
            printf("\033[1;31mU can not create a file same to a dir!\n\033[m");
        else
            printf("\033[1;31mU can not create two same files!\n\033[m");
    }
    else
        printf("\033[1;31mU should touch it first!\n\033[m");
}

void chmod(char filename[23], u_16 mode) //change a file's or a directory's mode
{
    if (mode > 7 || mode < 0)
    {
        printf("\033[1;31mmode <0~7>!\n\033[m");
        return;
    }

    u_32 block_num, dir_num, inode_num;
    u_8 type;
    int i, flag;
    flag = search(filename, &type, &block_num, &dir_num, &inode_num);

    if (flag)
    {
        load_dir(block_num);
        load_inode(inode_num);
        inode_buff->i_atime = current_time();
        inode_buff->i_mode = mode;
        inode_buff->i_mtime = current_time();
        upload_inode(inode_num);
        printf("\033[1;31mMofify sucess!\n\033[m");
    }
    else
        printf("\033[1;31mU should touch it first!\n\033[m");
}

void login() //login
{
    char password[10];
    int i;
    for (i = 3; i > 0; i--)
    {
        printf("\033[1;34mU have to login! %d shots left.\n\033[m", i);
        printf("\033[1;34mPassword:\033[m");
        scanf("%s", password);
        if (!strcmp(password, passwd))
            break;
        else
            printf("\033[1;31mIncorrect! Try again!\n\033[m");
    }
    if (i == 0)
        exit(0);
}

void check() //show disk infomation
{
    load_super_block();
    printf("volume name         : %s\n", s_block_buff->s_volume_name);
    printf("inodes count        : %d\n", s_block_buff->s_inodes_count);
    printf("blocks count        : %d\n", s_block_buff->s_blocks_count);
    printf("free inodes         : %d\n", s_block_buff->s_free_inodes_count);
    printf("free blocks         : %d\n", s_block_buff->s_free_blocks_count);
    printf("inode size          : %d\n", inode_size);
    printf("block size          : %d\n", block_size);
    printf("create time         : %s\n\n", time_now(s_block_buff->s_wtime));
}

void format() //format the disk
{
    char ch;
    while (1)
    {
        fflush(stdin);
        printf("\033[1;31mAre u sure? y / n :\n\033[m");
        scanf(" %c", &ch);
        if (ch == 'y' || ch == 'Y')
        {
            printf("\033[1;34mGet everyting ready...\n\033[m");
            init_disk();
            fp = fopen("./data.img", "r+b");
            last_alloc_inode = 1;
            last_alloc_block = 0;
            load_super_block();
            load_group_desc();
            strcpy(current_path, "/");
            current_dir_inode = 1;
            current_dirlen = 1;
            printf("\033[1;31mFormat sucess!\n\033[m");
            return;
        }
        else if (ch == 'n' || ch == 'N')
            return;
        else
        {
            printf("\033[1;31mDenied!\n\033[m");
            break;
        }
    }
}

int main()
{
    printf("\033[1;34mWelcome to my file system!\n\033[m");
    fp = fopen("./data.img", "r+b");
    if (!fp)
    {
        printf("\033[1;31mFail to open disk!\n\033[m");
        printf("\033[1;34mWill create a new one!\n\033[m");
        init_disk();
    }
    fp = fopen("./data.img", "r+b");

    last_alloc_inode = 1;
    last_alloc_block = 0;
    load_super_block();
    load_group_desc();

    strcpy(current_path, "/");
    current_dir_inode = 1;
    current_dirlen = 1;

    char cmd[10], filename[23];
    login();
    while (1)
    {
        fflush(stdin);
        printf("\033[1;36mlips@apple:\033[m");
        printf("\033[1;34m%s\033[m", current_path);
        scanf("%s", cmd);
        if (!strcmp(cmd, "help"))
            help();
        else if (!strcmp(cmd, "ls"))
            ls();
        else if (!strcmp(cmd, "mkdir"))
        {
            scanf("%s", filename);
            mkdir(filename);
        }
        else if (!strcmp(cmd, "rmdir"))
        {
            scanf("%s", filename);
            rmdir(filename);
        }
        else if (!strcmp(cmd, "cd"))
        {
            scanf("%s", filename);
            cd(filename);
        }
        else if (!strcmp(cmd, "touch"))
        {
            scanf("%s", filename);
            touch(filename);
        }
        else if (!strcmp(cmd, "rm"))
        {
            scanf("%s", filename);
            rm(filename);
        }
        else if (!strcmp(cmd, "cat"))
        {
            scanf("%s", filename);
            cat(filename);
        }
        else if (!strcmp(cmd, "open"))
        {
            scanf("%s", filename);
            open(filename);
        }
        else if (!strcmp(cmd, "close"))
        {
            scanf("%s", filename);
            close(filename);
        }
        else if (!strcmp(cmd, "read"))
        {
            scanf("%s", filename);
            read(filename);
        }
        else if (!strcmp(cmd, "write"))
        {
            scanf("%s", filename);
            write(filename);
        }
        else if (!strcmp(cmd, "rename"))
        {
            char old[23], new[23];
            scanf("%s %s", old, new);
            re_name(old, new);
        }
        else if (!strcmp(cmd, "chmod"))
        {
            u_16 mode;
            scanf("%s %hd", filename, &mode);
            chmod(filename, mode);
        }
        else if (!strcmp(cmd, "quit"))
            exit(0);
        else if (!strcmp(cmd, "check"))
            check();
        else if (!strcmp(cmd, "format"))
            format();
        else
            printf("\033[1;31mCommand denied!\n\033[0m");
    }
    return 0;
}
#include<stdlib.h>
#include<time.h>
#include<stdio.h>
#include"Block.h"

#define MAX_INODE 1024

extern int free_inode,free_block,inode_counts,block_counts;
typedef struct INode
{
    int Type;
    int CreationTime;
    int ModificationTime;
    int Size;
    int LinksCount;
    int DirectBlock[8];
    int IndirectBlock;
    int Parent;
    int Available;
} INode;//64bytes

INode InodeTable[MAX_INODE];

int min(int a,int b)
{
    return a<b?a:b;
}

int max(int a,int b)
{
    return a>b?a:b;
}

int Inode_format()
{
    for(int i=0;i<MAX_INODE;i++)
    {
        memset(&InodeTable[i],0,sizeof(INode));
        InodeTable[i].Available=1;
    }
    INode root;
    Inode_init(&root,1,0,0,65535);
    Inode_write(0);
    InodeTable[0]=root;
    free_inode=MAX_INODE-1;
    inode_counts=1;
    InodeTable[0].Available=0;
    return 0;

}
//initialize the inode 
int Inode_init(INode *inode,int type,int size,int linkscount,int parent)
{
    inode->Type=type;
    inode->CreationTime=time(NULL);
    inode->ModificationTime=inode->CreationTime;
    inode->Size=size;
    inode->LinksCount=linkscount;
    inode->Parent=parent;
    memset(inode->DirectBlock,0,sizeof(inode->DirectBlock));
    inode->IndirectBlock=0;
    inode->Available=0;
    return 0;
}

//get the next available inode and return the inode number
int Inode_alloc()
{
    for(int i=0;i<MAX_INODE;i++)
    {
        if(InodeTable[i].Available==1)
        {
            InodeTable[i].Available=0;
            free_inode--;
            inode_counts++;
            if (inode_counts>MAX_INODE) return -1;
           // printf("alloc inode:%d\n",i);
            return i;
        }
    }
    return -1;

}

//free the inode
int Inode_free(int inode)
{
    if (inode < 0 || inode >= MAX_INODE)
    {
        return -1;
    }
    
    for (int i = 0; i < 8; i++)
    {
        if (InodeTable[inode].DirectBlock[i] != 0) Block_free(InodeTable[inode].DirectBlock[i],0);
    }
    if (InodeTable[inode].IndirectBlock!= 0)
    {
        char buf[BLOCK_SIZE];
        Block_read(InodeTable[inode].IndirectBlock, buf,0);
        short int p[128];
        memcpy(p, buf, BLOCK_SIZE);
        for (int j = 0; j < 128; j++)
            if (p[j] != 0) Block_free(p[j],0);
    }
    Inode_init(&InodeTable[inode],0,0,0,0);
    InodeTable[inode].Available=1;
    free_inode++;
    inode_counts--;
    Inode_write(inode);
    return 0;
}

//write the inode to the disk
int Inode_write(int inode)
{
    if (inode < 0 || inode >= MAX_INODE) return -1;
    int block=inode/INODE_PER_BLOCK+INODE_TABLE_START;
    int offset=inode%BLOCK_SIZE*sizeof(INode);
    char node[sizeof(INode)];
    memcpy(node,&InodeTable[inode],sizeof(INode));
    Block_write(block,offset,node,sizeof(INode));
    return 0; 
}

//add file or dir to dir
int Inode_add(int Dir, char* file, int type)
{   
    INode* dir=&InodeTable[Dir];
    file=strtok(file," \r\n");
    if (dir->Type != 1) return -1;
    int inode=Inode_alloc();
    if(inode==-1) return -1;
    char buf[BLOCK_SIZE];
    Directory dir_item[8];
    for(int i=0;i<8;i++)
    {
        if(dir->DirectBlock[i]!=0)
        {
            if (Block_read(dir->DirectBlock[i], buf,0) == -1) return -1;
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 1)
                {
                    strcpy(dir_item[j].Name, file);
                    dir_item[j].Inode = inode;
                    dir_item[j].Available = 0;
                    dir_item[j].Type = type;
                    Inode_init(&InodeTable[inode],type,0,0,Dir);
                    Inode_write(inode);
                   // printf("add file:%s\n",file);
                    memcpy(buf, dir_item, BLOCK_SIZE);
                    if (Block_write(dir->DirectBlock[i], 0,buf,BLOCK_SIZE) == -1) return -1;
                    dir->ModificationTime=time(NULL);
                    return 0;
                }
            }
        }
    }
    //realloc a block
    if (dir->LinksCount<8)
    {
        for (int direct = 0; direct < 8; direct++)
        {
            if (dir->DirectBlock[direct] == 0)
            {
                int block=Block_alloc();
                if(block==-1) return -1;
                dir->DirectBlock[direct]=block;
                dir->LinksCount++;
                for (int i = 0; i < 8; i++)
                {
                    memset(&dir_item[i], 0, sizeof(Directory));
                    dir_item[i].Available = 1;
                }
                strcpy(dir_item[0].Name, file);
                dir_item[0].Inode = inode;
                dir_item[0].Available = 0;
                dir_item[0].Type = type;
                Inode_init(&InodeTable[inode],type,0,0,Dir);
                Inode_write(inode);
              //  printf("add files:%s\n",file);
                memcpy(buf, dir_item, BLOCK_SIZE);
                if (Block_write(block, 0, buf,BLOCK_SIZE) == -1) return -1;
                //Block_print(block);
                dir->ModificationTime=time(NULL);
                return 0;
            }
        }
    }
    return -1;
}

//remove file from dir
int Inode_removeFile(int Dir, char* file)
{
    INode* dir=&InodeTable[Dir];
    if (dir->Type != 1) return -1;
    for (int i = 0; i < 8; i++)
    {
        char buf[BLOCK_SIZE];
        Directory dir_item[8];
        if (dir->DirectBlock[i] != 0)
        {
            if (Block_read(dir->DirectBlock[i], buf,0) == -1) return -1;
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 0 && strcmp(dir_item[j].Name, file) == 0 && dir_item[j].Type == 0)
                {
                   // printf("remove file:%s\n",file);
                    Inode_free(dir_item[j].Inode);
                    memset(&dir_item[j], 0, sizeof(Directory));
                    dir_item[j].Available = 1;
                    memcpy(buf, dir_item, BLOCK_SIZE);
                    if (Block_write(dir->DirectBlock[i], 0, buf,BLOCK_SIZE) == -1) return -1;
                    return 0;
                }
            }
        }
    }
    return -1;
}

int is_empty_dir(INode* dir)
{ 
    if (dir->Type != 1) return -1;
    for (int i = 0; i < 8; i++)
    {
        char buf[BLOCK_SIZE];
        Directory dir_item[8];
        if (dir->DirectBlock[i] != 0)
        {
            if (Block_read(dir->DirectBlock[i], buf,0) == -1) return -1;
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 0) 
                {
                    fprintf(stderr, "dir is not empty\n");
                    return -1;
                }   
            }
            Block_free(dir->DirectBlock[i],0);
            dir->DirectBlock[i] = 0;
            dir->LinksCount--;
        }
    }
    return 0;
}

//remove dir from dir
int Inode_removeDir(int Dir, char* subdir)
{
    INode* dir = &InodeTable[Dir];
    if (dir->Type != 1) return -1;
    for (int i = 0; i < 8; i++)
    {
        char buf[BLOCK_SIZE];
        Directory dir_item[8];
        if (dir->DirectBlock[i] != 0)
        {
            if (Block_read(dir->DirectBlock[i], buf,0) == -1) return -1;
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 0 && strcmp(dir_item[j].Name, subdir) == 0 && dir_item[j].Type == 1)
                {
                    INode* subdir_inode = &InodeTable[dir_item[j].Inode];
                    if (is_empty_dir(subdir_inode) == 0)
                    {
                        Inode_free(dir_item[j].Inode);
                        dir_item[j].Available = 1;
                        memcpy(buf, dir_item, BLOCK_SIZE);
                        if (Block_write(dir->DirectBlock[i], 0, buf,BLOCK_SIZE) == -1) return -1;
                        dir->ModificationTime=time(NULL);
                        return 0;
                    }
                    else return -1;
                }
            }
        }
    }
    return -1;
}


//list files in dir
int Inode_listFiles(int Dir, char name[64][MAX_NAME])
{
    INode* dir = &InodeTable[Dir];
    if (dir->Type != 1) return -1;
    int file_count=0,dir_count=0;
    for (int i = 0; i < 8; i++)
    {
        char buf[BLOCK_SIZE];
        Directory dir_item[8];
        if (dir->DirectBlock[i] != 0)
        {
            if (Block_read(dir->DirectBlock[i], buf, 0) == -1) return -1;
           // Block_print(dir->DirectBlock[i]);
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 0)
                {
                    strcpy(name[file_count++], dir_item[j].Name);
                    //strcpy(directory[dir_count++], dir_item[j].Name);
                }

            }
        }
    }
    qsort(name,file_count,sizeof(name[0]),strcmp);

    return file_count;
}

//search file in dir
int Inode_search(int Dir,char* file)
{
    INode* dir = &InodeTable[Dir];
    if (dir->Type != 1) return -1;
    for (int i = 0; i < 8; i++)
    {
        char buf[BLOCK_SIZE];
        Directory dir_item[8];
        if (dir->DirectBlock[i] != 0)
        {
            if (Block_read(dir->DirectBlock[i], buf, 0) == -1) return -1;
            memcpy(dir_item, buf, BLOCK_SIZE);
            for (int j = 0; j < 8; j++)
            {
                if (dir_item[j].Available == 0 && strcmp(dir_item[j].Name, file) == 0)
                {
                    return dir_item[j].Inode;
                }
            }
        }
    }
    return -1;
}

//write file
int Inode_write_file(int inode, int pos, char* buf, int len)
{
    INode* file = &InodeTable[inode];
    if (file->Type != 0) return -1;
    if (pos>=file->Size) pos=file->Size;
    int block = pos / BLOCK_SIZE;
    int offset = pos % BLOCK_SIZE;
    memcpy(buf,buf,len);
   // printf("write:%s\n",buf);
    file->Size=max(file->Size,pos+len);
    int consume = 0;
  //  printf("size:%d\n",file->Size);
    if (block<7)
    {
        if (file->DirectBlock[block] != 0)
        {
            Block_write(file->DirectBlock[block], offset, buf,len);
            len -= BLOCK_SIZE - offset;
            consume += BLOCK_SIZE - offset;
        }
        else
        {
            file->DirectBlock[block] = Block_alloc();
            file->LinksCount++;
            if (file->DirectBlock[block] == -1) return -1;
            Block_write(file->DirectBlock[block], 0, buf,len);
            len -= BLOCK_SIZE;
            consume += BLOCK_SIZE;
        }
        if (len > 0)
        {
            for (int i = block + 1; i < 8; i++)
            {
                if (file->DirectBlock[i] == 0)
                {
                    file->DirectBlock[i] = Block_alloc();
                    file->LinksCount++;
                    if (file->DirectBlock[i] == -1) return -1;
                    Block_write(file->DirectBlock[i], 0, buf + consume,len);
                    len -= BLOCK_SIZE;
                    consume += BLOCK_SIZE;
                }
                else
                {
                    Block_write(file->DirectBlock[i], 0, buf + consume,len);
                    len -= BLOCK_SIZE;
                    consume += BLOCK_SIZE;
                }
                if (len<=0) break;
            }
            if (len > 0)
            {
                if (file->IndirectBlock == 0)
                {
                    file->IndirectBlock = Block_alloc();
                    if (file->IndirectBlock == -1) return -1;
                    short int p[128];
                    memset(p, 0, sizeof(p));
                    Block_write(file->IndirectBlock, 0, p, sizeof(p));
                }
                char indirect[BLOCK_SIZE];
                Block_read(file->IndirectBlock, indirect, 0);
                short int p[128];
                memcpy(p, indirect, BLOCK_SIZE);
                for (int i = 0; i < 128; i++)
                {
                    if (p[i] == 0)
                    {
                        p[i] = Block_alloc();
                        if (p[i] == -1) return -1;
                        file->LinksCount++;
                        Block_write(p[i], 0, buf + consume,len);
                        len -= BLOCK_SIZE;
                        consume += BLOCK_SIZE;
                    }
                    else
                    {
                        Block_write(p[i], 0, buf + consume,len);
                        len -= BLOCK_SIZE;
                        consume += BLOCK_SIZE;
                    }
                    if (len<=0) break;
                }
                memcpy(indirect, p, BLOCK_SIZE);
                Block_write(file->IndirectBlock, 0, indirect, BLOCK_SIZE);
            }
        }
    }
    else
    {
        char indirect[BLOCK_SIZE];
        Block_read(file->IndirectBlock, indirect, 0);
        short int p[128];
        memcpy(p, indirect, BLOCK_SIZE);
        if (p[block - 7] != 0)
        {
            Block_write(p[block - 7], offset, buf,len);
            len -= BLOCK_SIZE - offset;
        }
        for (int i = block - 6; i < 128; i++)
        {
            if (p[i] == 0)
            {
                p[i] = Block_alloc();
                if (p[i] == -1) return -1;
                Block_write(p[i], 0, buf + consume,len);
                len -= BLOCK_SIZE;
                consume += BLOCK_SIZE;
            }
            else
            {
                Block_write(p[i], 0, buf + consume,len);
                len -= BLOCK_SIZE;
                consume += BLOCK_SIZE;
            }
            if (len<=0) break;
        }
    }     
    file->ModificationTime = time(NULL);
    return 0;
}

//read file
int Inode_read(int inode, int pos, char* buf)
{
    INode* file = &InodeTable[inode];
    if (file->Type != 0) return NULL;
    int len = file->Size - pos;
    int block = pos / BLOCK_SIZE;
    int offset = pos % BLOCK_SIZE;
  //  printf("pos:%d\n",pos);
    int consume = 0;
    if (block < 7)
    {
        if (file->DirectBlock[block] != 0)
        {
            Block_read(file->DirectBlock[block], buf + consume,offset);
           // printf("offset:%d\n",offset);
            consume += BLOCK_SIZE - offset;
        }
        if (consume < len)
        {
            for (int i = block + 1; i < 8; i++)
            {
                if (file->DirectBlock[i] != 0)
                {
                    Block_read(file->DirectBlock[i], buf + consume,0);
                    consume += BLOCK_SIZE;
                }
                if (consume >= len) break;
            }
            if (consume < len)
            {
                char indirect[BLOCK_SIZE];
                Block_read(file->IndirectBlock, indirect,0);
                short int p[128];
                memcpy(p, indirect, BLOCK_SIZE);
                for (int i = 0; i < 128; i++)
                {
                    if (p[i] != 0)
                    {
                        Block_read(p[i], buf + consume,0);
                        consume += BLOCK_SIZE;
                    }
                    if (consume >= len) break;
                }
            }
        }
    }
    else
    {
        char indirect[BLOCK_SIZE];
        Block_read(file->IndirectBlock, indirect,0);
        short int p[128];
        memcpy(p, indirect, BLOCK_SIZE);
        if (p[block - 7] != 0)
        {
            Block_read(p[block - 7], buf + consume,offset);
            consume += BLOCK_SIZE - offset;
        }
        for (int i = block - 6; i < 128; i++)
        {
            if (p[i] != 0)
            {
                Block_read(p[i], buf + consume,0);
                consume += BLOCK_SIZE;
            }
            if (consume >= len) break;
        }
    }
    //printf("read:%s\n",buf);
    return 0;
}

//delete file contents
int Inode_delete(int inode,int pos)
{
    INode *file=&InodeTable[inode];
    if (file->Type != 0) return -1;
    if (pos>=file->Size) return 0;
    int block = pos / BLOCK_SIZE;
    int offset = pos % BLOCK_SIZE;
    int len = file->Size - pos;
    int consume = 0;
    if (block<7)
    {
        if (file->DirectBlock[block] != 0)
        {
            Block_free(file->DirectBlock[block], offset);
            if (offset==0) file->LinksCount--;
            len -= BLOCK_SIZE - offset;
            consume += BLOCK_SIZE - offset;
        }
        if (consume<len)
        {
            for (int i = block + 1; i < 8; i++)
            {
                if (file->DirectBlock[i] != 0)
                {
                    Block_free(file->DirectBlock[i], 0);
                    file->LinksCount--;
                    len -= BLOCK_SIZE;
                    consume += BLOCK_SIZE;
                }
                if (consume >= len) break;
            }
        }
        if (consume<len)
        {
            char indirect[BLOCK_SIZE];
            Block_read(file->IndirectBlock, indirect,0);
            short int p[128];
            memcpy(p, indirect, BLOCK_SIZE);
            for (int i = 0; i < 128; i++)
            {
                if (p[i] != 0)
                {
                    Block_free(p[i], 0);
                    file->LinksCount--;
                    len -= BLOCK_SIZE;
                    consume += BLOCK_SIZE;
                }
                if (consume >= len) break;
            }
        }
    }
    else
    {
        if (file->IndirectBlock != 0)
        {
            char indirect[BLOCK_SIZE];
            Block_read(file->IndirectBlock, indirect,0);
            short int p[128];
            memcpy(p, indirect, BLOCK_SIZE);
            if (p[block - 7] != 0)
            {
                Block_free(p[block - 7], offset);
                if (offset==0) file->LinksCount--;
                len -= BLOCK_SIZE - offset;
                consume += BLOCK_SIZE - offset;
            }
            for (int i = block - 6; i < 128; i++)
            {
                if (p[i] != 0)
                {
                    Block_free(p[i], 0);
                    file->LinksCount--;
                    len -= BLOCK_SIZE;
                    consume += BLOCK_SIZE;
                }
                if (consume >= len) break;
            }
        }
    }
}

int Inode_print(int inode)
{
    INode* file = &InodeTable[inode];
    printf("Inode:%d\n",inode);
    printf("Type:%d\n",file->Type);
    /*printf("CreationTime:%d\n",file->CreationTime);
   // printf("ModificationTime:%d\n",file->ModificationTime);
  //  printf("Size:%d\n",file->Size);
   // printf("LinksCount:%d\n",file->LinksCount);
   // printf("Parent:%d\n",file->Parent);
  //  printf("DirectBlock:");
   // for(int i=0;i<8;i++)
    {
        printf("%d ",file->DirectBlock[i]);
    }
  //  printf("\n");
 //   printf("IndirectBlock:%d\n",file->IndirectBlock);
    printf("Available:%d\n",file->Available);*/
    return 0;
}
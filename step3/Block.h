#include<stdlib.h>


#include "tcp_utils.h"

#define BLOCK_SIZE 256
#define MAX_BLOCK 1024
#define MAX_NAME 26
#define FILE_BLOCK_START 259
#define INODE_TABLE_START 3
#define INODE_PER_BLOCK 4
#define INODE_TABLE_BLOCKS 256

typedef struct Block
{
    char Data[BLOCK_SIZE];
} Block;//256bytes
extern tcp_client client;
extern int ncyl, nsec;
typedef struct Directory_item
{
    char Name[MAX_NAME];
    int Inode;
    uint8_t Available;
    uint8_t Type;
    
} Directory;//32bytes



Block BlockTable[MAX_BLOCK];

int Block_format()
{
    client_send(client, "F", 2);
    return 0;
}

int Block_read(int block, char *buf,int pos)
{
    if (block < 0 || block >= 1024) return -1;
  //  printf("Block_read: %d %d\n", block, pos);
    int size=BLOCK_SIZE-pos;
    
    char read_cmd[20];
    memset(read_cmd,0,20);
    read_cmd[0]='R';
    read_cmd[1]=' ';
    sprintf(&read_cmd[2],"%d ",block/nsec);
    sprintf(&read_cmd[strlen(read_cmd)],"%d ",block%nsec);
   // printf("cmdread: %s\n", read_cmd);
    client_send(client, &read_cmd, sizeof(read_cmd));
    char recieve_buf[4096];
    int n=client_recv(client, recieve_buf, BLOCK_SIZE+5); 
    if (n<0) return -1;
    if (recieve_buf[0]=='N') return -1;
 //   printf("recieve: %s\n", recieve_buf);
    memcpy(&BlockTable[block].Data, &recieve_buf[4], BLOCK_SIZE);
    memcpy(buf, &BlockTable[block].Data[pos], size);
    return 0;
}

int Block_write(int block, int pos, char *buf, int len)
{
    if (block < 0 || block >= 1024) return -1;
    if (pos < 0 || pos >= BLOCK_SIZE) return -1;
   // printf("Block_write: %d %d\n", block, pos);
    len=(len>BLOCK_SIZE-pos)?BLOCK_SIZE-pos:len;    
    memcpy(&BlockTable[block].Data[pos], buf, len);
   // for (int i=0;i<BLOCK_SIZE;i++)
   // {
   //     printf("%x", BlockTable[block].Data[i]);
  //  }
    char write_cmd[350];
    write_cmd[0]='W';
    write_cmd[1]=' ';
    sprintf(&write_cmd[2],"%d ",block/nsec);
    sprintf(&write_cmd[strlen(write_cmd)],"%d ",block%nsec);
    sprintf(&write_cmd[strlen(write_cmd)],"%d ",BLOCK_SIZE);
    
    int p=strlen(write_cmd);
    memcpy(write_cmd+p,&BlockTable[block].Data,BLOCK_SIZE);
   // printf("cmdwrite: %s\n", write_cmd);
    client_send(client, &write_cmd, sizeof(write_cmd));
    char recieve_buf[4096];
    int n=client_recv(client, recieve_buf, BLOCK_SIZE+5);
    if (n<0) return -1;
    if (recieve_buf[0]=='N') return -1;
    return 0;
}

int Block_alloc()
{
    for (int i = FILE_BLOCK_START; i < MAX_BLOCK; i++)
    {
        if (BlockTable[i].Data[0] == '\0')
        {
            Block_free(i,0);
            return i;
        }
    }
    return -1;
}

int Block_free(int block,int pos)
{
    memset(&BlockTable[block].Data[pos], 0, BLOCK_SIZE-pos);
    if (pos!=0)
    {
        char write_cmd[350];
        write_cmd[0]='W';
        write_cmd[1]=' ';
        sprintf(&write_cmd[2],"%d ",block/nsec);
        sprintf(&write_cmd[strlen(write_cmd)],"%d ",block%nsec);
        sprintf(&write_cmd[strlen(write_cmd)],"%d ",BLOCK_SIZE);
        printf("cmdwrite: %s\n", write_cmd);
        int p=strlen(write_cmd);
        memcpy(write_cmd+p,&BlockTable[block].Data,BLOCK_SIZE);
        client_send(client, &write_cmd, sizeof(write_cmd));
        char recieve_buf[4096];
        int n=client_recv(client, recieve_buf, BLOCK_SIZE+5);
        if (n<0) return -1;
        if (recieve_buf[0]=='N') return -1;
    }
    else
    {
        char free_cmd[20];
        free_cmd[0]='F';
        free_cmd[1]='B';
        free_cmd[2]=' ';
        sprintf(&free_cmd[3],"%d ",block/nsec);
        sprintf(&free_cmd[strlen(free_cmd)],"%d ",block%nsec);
        //printf("cmdfree: %s\n", free_cmd);
        client_send(client, &free_cmd, sizeof(free_cmd));


    }
    return 0;
}

int Block_print(int block)
{
    printf("Block %d:\n", block);
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        printf("%c", BlockTable[block].Data[i]);
    }
    return 0;
}


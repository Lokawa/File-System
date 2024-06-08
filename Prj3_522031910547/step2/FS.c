#include<string.h>
#include<sys/mman.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>

#include "tcp_utils.h"
#include "Inode.h"

tcp_client client;
tcp_server server;
int inode_counts = 0;
int block_counts = 0;
int free_inode = MAX_INODE;
int free_block = MAX_BLOCK;
int current_inode = 0;
int nsec = 0, ncyl = 0;

int cmd_f(tcp_buffer *write_buf, char *args, int len)
{
    Block_format();
    Inode_format();
    current_inode = 0;
    send_to_buffer(write_buf, "Format Success", 14);
    return 0;
}

int cmd_mk(tcp_buffer *write_buf, char *args, int len)
{
   // char *file= strtok(args, " \r\n");
    if (Inode_add(current_inode, args, 0) < 0) 
    {
        static char err[] = "Failed to create file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    send_to_buffer(write_buf, "Created file Success", 20);
    return 0;
}

int cmd_mkdir(tcp_buffer *write_buf, char *args, int len)
{
   // char *file= strtok(args, " \r\n");
    if (Inode_add(current_inode, args, 1) < 0) 
    {
        static char err[] = "Failed to create directory";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    send_to_buffer(write_buf, "Created directory Success", 26);
    return 0;
}

int cmd_rm(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    if (Inode_removeFile(current_inode, file) < 0) 
    {
        static char err[] = "Failed to remove file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    send_to_buffer(write_buf, "Removed file Success", 20);
    return 0;
}

int cmd_rmdir(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    if (Inode_removeDir(current_inode, file) < 0) 
    {
        static char err[] = "Failed to remove directory";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    send_to_buffer(write_buf, "Removed directory Success", 25);
    return 0;
}

int cmd_ls(tcp_buffer *write_buf, char *args, int len)
{
    char file_list[64][MAX_NAME];
    int count=Inode_listFiles(current_inode,file_list);
    if (count< 0) 
    {
        static char err[] = "Failed to list directory";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    char buf[64*MAX_NAME+64];
    memset(buf,0,64*MAX_NAME+64);
    int pos=0;
    for(int i=0;i<count;i++)
    {
        memcpy(buf+pos,file_list[i],strlen(file_list[i]));
        pos+=strlen(file_list[i]);
        buf[pos++]=' ';
    }
    send_to_buffer(write_buf, buf+1, strlen(buf));
    return 0;
}

int cmd_cat(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    int inode=Inode_search(current_inode, file);
    if (inode < 0) 
    {
        static char err[] = "Failed to open file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    int size=InodeTable[inode].Size;
   // printf("size:%d\n",size);
    size=(size%BLOCK_SIZE==0)?size/BLOCK_SIZE:size/BLOCK_SIZE+1;
    char buf[size*BLOCK_SIZE];
    Inode_read(inode, 0, buf);

    send_to_buffer(write_buf, buf, size*BLOCK_SIZE);
   // send_to_buffer(write_buf, "Read file Success", 18);
    return 0;
}

int cmd_i(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    int l,pos;
    sscanf(strtok(NULL, " \r\n"), "%d", &pos);
    sscanf(strtok(NULL, " \r\n"), "%d", &l);
    char *data= strtok(NULL, " \r\n");
    int inode= Inode_search(current_inode, file);
    if (inode < 0) 
    {
        static char err[] = "Failed to open file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    pos=(pos>InodeTable[inode].Size)?InodeTable[inode].Size:pos;
    int size=InodeTable[inode].Size-pos-l;
    size=(size%BLOCK_SIZE==0)?size/BLOCK_SIZE:size/BLOCK_SIZE+1;
    char after_pos[size*BLOCK_SIZE];
    Inode_read(inode, pos, after_pos);
    Inode_write_file(inode, pos, data, l);
    Inode_write_file(inode, pos+l, after_pos, strlen(after_pos));
    send_to_buffer(write_buf, "Insert file Success", 20);
    return 0;
}

int cmd_w(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    int l;
    sscanf(strtok(NULL, " \r\n"), "%d", &l);
    char *buf= strtok(NULL, " \r\n");
    int inode= Inode_search(current_inode, file);
    if (inode < 0) 
    {
        static char err[] = "Failed to open file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
   // printf("buf:%s\n",buf);
    Inode_write_file(inode, 0, buf,l);
    send_to_buffer(write_buf, "Write file Success", 18);
    return 0;
}

int cmd_d(tcp_buffer *write_buf, char *args, int len)
{
    char *file= strtok(args, " \r\n");
    int pos,l;
    sscanf(strtok(NULL, " \r\n"), "%d", &pos);
    sscanf(strtok(NULL, " \r\n"), "%d", &l);
    int inode= Inode_search(current_inode, file);
    if (inode < 0) 
    {
        static char err[] = "Failed to open file";
        send_to_buffer(write_buf, err, sizeof(err));
        return 0;
    }
    int size=InodeTable[inode].Size-pos-l;
    size=(size%BLOCK_SIZE==0)?size/BLOCK_SIZE:size/BLOCK_SIZE+1;
    char* after_pos[size*BLOCK_SIZE];
    Inode_read(inode, pos+l, after_pos);
    Inode_delete(inode, pos);
    Inode_write_file(inode, pos, after_pos, strlen(after_pos));
    send_to_buffer(write_buf, "Delete file Success", 20);
    return 0;
}

int cmd_e(tcp_buffer *write_buf, char *args, int len)
{
    send_to_buffer(write_buf, "Bye!", 5);
    return -1;
}

int cmd_cd(tcp_buffer *write_buf, char *args, int len)
{
    char *path= strtok(args, "\r\n");
    path= strtok(path, "/\r\n");
    //printf("path:%s\n",path);
   // printf("current_inode:%d\n",current_inode);
    while (path!=NULL) 
    {
        if (strcmp(path, "home") == 0) 
        {
            current_inode=0;
            path= strtok(NULL, "/\r\n");
            continue;
        }
        if (strcmp(path, ".") == 0) 
        {
            path= strtok(NULL, "/\r\n");
            continue;
        }
        if (strcmp(path, "..") == 0) 
        {
            current_inode= InodeTable[current_inode].Parent;
            if (current_inode==65535)
            {
                static char err[] = "Already in root";
                send_to_buffer(write_buf, err, sizeof(err));
                return 0;
            }
            path= strtok(NULL, "/\r\n");
            continue;
        }
        int inode= Inode_search(current_inode, path);
        if (inode < 0) 
        {
            static char err[] = "No such directory";
            send_to_buffer(write_buf, err, sizeof(err));
            return 0;
        }
        if (InodeTable[inode].Type != 1) 
        {
            static char err[] = "Not a directory";
            send_to_buffer(write_buf, err, sizeof(err));
            return 0;
        }
        current_inode= inode;
        path= strtok(NULL, "/\r\n");
     //   printf("path:%s\n",path);
     //   printf("current_inode:%d\n",current_inode);
    }
   send_to_buffer(write_buf, "CD Success", 10);
   return 0;
}
static struct 
{
    const char* name;
    int (*handler)(tcp_buffer *write_buf, char *args, int len);
}cmd_table[]= {
    {"f", cmd_f},
    {"mk", cmd_mk},
    {"mkdir", cmd_mkdir},
    {"rm",cmd_rm},
    {"rmdir",cmd_rmdir},
    {"ls",cmd_ls},
    {"cat",cmd_cat},
    {"i",cmd_i},
    {"w",cmd_w},
    {"d",cmd_d},
    {"e",cmd_e},
    {"cd",cmd_cd}
};

#define NCMD (sizeof(cmd_table)/sizeof(cmd_table[0]))

void add_client(int id)
{
    printf("Client connected\n");
}

int handle_client(int id, tcp_buffer *write_buf,char *msg, int len)
{
    char *p = strtok(msg, " \r\n");
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (strcmp(p, cmd_table[i].name) == 0) {
            ret = cmd_table[i].handler(write_buf, p + strlen(p) + 1, len - strlen(p) - 1);
            printf("Command %s\n", p);
            break;
        }
    if (ret == 1) {
        static char unk[] = "Unknown command";
        send_to_buffer(write_buf, unk, sizeof(unk));
    }
    if (ret < 0) {
        return -1;
    }
}

void clear_client(int id)
{
    printf("Client disconnected\n");
}

int main(int argc,char* argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <BDSPort> <FSPort>", argv[0]);
        exit(EXIT_FAILURE);
    }
    int bdsp = atoi(argv[2]);
    int fsp = atoi(argv[3]);
    client = client_init("localhost", bdsp);
    client_send(client, "I", 2);
    char buf[4096];
    int n=client_recv(client,buf,sizeof(buf));
    if (n<0)
    {
        printf("Failed to connect to BDS\n");
        exit(EXIT_FAILURE);
    }
    sscanf(buf,"%d %d",&ncyl,&nsec);
    printf("ncyl:%d nsec:%d\n",ncyl,nsec);
    current_inode=0;
    server = server_init(fsp,1,add_client,handle_client,clear_client);
    server_loop(server);
   // static char buf[4096];

}
#include<stdlib.h>
#include<stdlib.h>
#include<time.h>
#include<stdio.h>

#include"Block.h"
#include"tcp_utils.h"


#define MAX_USER 16
#define USER_PER_BLOCK 8

typedef struct User
{
    char username[8];
    char password[16];
    uint16_t uid;
    uint16_t inode;
    uint16_t current_inode;
    uint16_t last_login;
} Users;//32bytes


Users UserTable[MAX_USER];

int User_login(char *username, char *password)
{

    for (int i = 0; i < MAX_USER; i++)
    {
        if (UserTable[i].uid == 65535) continue;
        if (strcmp(UserTable[i].username, username) == 0 && strcmp(UserTable[i].password, password) == 0)
        {
            UserTable[i].last_login = time(NULL);
            User_write(i);
            return i;
        }
    }
    return -1;
}

int User_create(char *username, char *password)
{
    for (int i = 0; i < MAX_USER; i++)
    {
        if (strcmp(UserTable[i].username, username) == 0)
        {
            client_send(client, "User already exists", 20);
            return -1;
        }
    }
    for (int i = 0; i < MAX_USER; i++)
    {
        if (UserTable[i].uid == 65535)
        {
            strcpy(UserTable[i].username, username);
            strcpy(UserTable[i].password, password);
            UserTable[i].uid = i;
            UserTable[i].inode = Inode_add(0, username, 1);
          //  printf("UserTable[%d].inode: %d\n", i, UserTable[i].inode);
            UserTable[i].current_inode = UserTable[i].inode;
            UserTable[i].last_login = time(NULL);
            User_write(i);
            return i;
        }
    }
    return -1;
}

int User_format()
{
    for (int i = 0; i < MAX_USER; i++)
    {
        memset(&UserTable[i], 0, sizeof(Users));
        UserTable[i].uid = 65535;
        int block=i/USER_PER_BLOCK;
        int offset=i%USER_PER_BLOCK;
        User_write(i);
    }
    Users root;
    strcpy(root.username, "root");
    strcpy(root.password, "root");
    root.uid = 0;
    root.inode = 0;
    root.current_inode = 0;
    root.last_login = time(NULL);
    UserTable[0] = root;
    User_write(0);
    return 0;
}

int User_write(int uid)
{
    int block=uid/USER_PER_BLOCK;
    int offset=uid%USER_PER_BLOCK;
    char buf[sizeof(Users)];
    memset(buf,0,sizeof(Users));
    memcpy(buf, &UserTable[uid], sizeof(Users));
    Block_write(block, offset*sizeof(Users), buf, sizeof(Users));
    return 0;
}
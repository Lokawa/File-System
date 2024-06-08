#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "tcp_utils.h"

// Block size in bytes
#define BLOCKSIZE 256
int ncyl, nsec, ttd;
char *diskfile;
int cyl;


// return a negative value to exit
int cmd_i(tcp_buffer *write_buf, char *args, int len) {
    static char buf[64];
    sprintf(buf, "%d %d", ncyl, nsec);

    // send to buffer, including the null terminator
    send_to_buffer(write_buf, buf, strlen(buf) + 1);
    return 0;
}

int cmd_r(tcp_buffer *write_buf, char *args, int len) {
    int c,s;
    sscanf(args,"%d %d",&c,&s);
    static char buf[275];
    memcpy(buf,"YES ",4);
    int result=memcpy(buf+4,&diskfile[BLOCKSIZE*(c*nsec+s)],BLOCKSIZE);
    if (result == -1) send_to_buffer(write_buf, "No", 3); else
    {
  //      static char yes[]="Yes ";
    //    send_to_buffer(write_buf, yes, sizeof(yes));
        
        usleep(ttd*abs(cyl-c));
        cyl=c;
        send_to_buffer(write_buf, buf, BLOCKSIZE+4);
    } 
  //  send_to_buffer(write_buf, args, len);
    
    return 0;
}

int cmd_w(tcp_buffer *write_buf, char *args, int len) {
    int c,s,l,offset;
    char data[BLOCKSIZE];
    sscanf(args,"%d %d %d %n",&c,&s,&l,&offset);
    memcpy(data,args+offset,BLOCKSIZE);
    usleep(ttd*abs(cyl-c));
    cyl=c;
    int result=memcpy(&diskfile[BLOCKSIZE*(c*nsec+s)],data,l);
    if (result == -1) send_to_buffer(write_buf, "No", 3); else send_to_buffer(write_buf, "Yes", 4); 
    return 0;
}

int cmd_e(tcp_buffer *write_buf, char *args, int len) {
    send_to_buffer(write_buf, "Bye!", 5);
    return -1;
}

int cmd_f(tcp_buffer *write_buf, char *args, int len) {
    memset(diskfile, 0, ncyl * nsec * BLOCKSIZE);
    return 0;
}

int cmd_fb(tcp_buffer *write_buf, char *args, int len) {
    int c,s;
    sscanf(args,"%d %d",&c,&s);
    memset(&diskfile[BLOCKSIZE*(c*nsec+s)], 0, BLOCKSIZE);
    return 0;
}

static struct {
    const char *name;
    int (*handler)(tcp_buffer *write_buf, char *, int);
} cmd_table[] = {
    {"I", cmd_i},
    {"R", cmd_r},
    {"W", cmd_w},
    {"E", cmd_e},
    {"F", cmd_f},
    {"FB", cmd_fb},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void add_client(int id) {
    // some code that are executed when a new client is connected
    // you don't need this in step1
}

int handle_client(int id, tcp_buffer *write_buf, char *msg, int len) {
  //  printf("Command %s\n", msg);
    char *p = strtok(msg, " \r\n");
    
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (strcmp(p, cmd_table[i].name) == 0) {
            ret = cmd_table[i].handler(write_buf, p + strlen(p) + 1, len - strlen(p) - 1);
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

void clear_client(int id) {
    // some code that are executed when a client is disconnected
    // you don't need this in step2
    
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay> <port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    // args
    char *diskfname = argv[1];
    ncyl = atoi(argv[2]);
    nsec = atoi(argv[3]);
    ttd = atoi(argv[4]);  // ms
    int port = atoi(argv[5]);

    // open file
    int fd = open(diskfname, O_RDWR | O_CREAT, 0);
    if (fd < 0) {
        fprintf(stderr,"Error: Cannot open file '%s'.\n", diskfname);
        exit(EXIT_FAILURE);
    }
    // stretch the file
    long FILESIZE= (long)ncyl * nsec * BLOCKSIZE;
    int result=lseek(fd, FILESIZE-1, SEEK_SET);
    if (result == -1) {
        close(fd);
        fprintf(stderr,"Error: Cannot stretch file '%s'.\n", diskfname);
        exit(EXIT_FAILURE);
    }
    result= write(fd, "", 1);
    if (result != 1) {
        close(fd);
        fprintf(stderr,"Error: Cannot write to file '%s'.\n", diskfname);
        exit(EXIT_FAILURE);
    }
    // mmap
    diskfile = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (diskfile == MAP_FAILED) {
        close(fd);
        fprintf(stderr,"Error: Cannot mmap file '%s'.\n", diskfname);
        exit(EXIT_FAILURE);
    }
    cyl=0;
    // command
    tcp_server server = server_init(port, 1, add_client, handle_client, clear_client);
    server_loop(server);
}

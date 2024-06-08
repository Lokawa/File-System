#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#include "tcp_utils.h"

char pwd[27648];

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <Port>", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[2]);
    tcp_client client = client_init("localhost", port);
    printf("Welcome to the File System!\n");

    static char buf[4096];
    char *cyan="\033[0;36m";
    char *reset="\033[0m";
    int logged_in = 0;
    while (1) {
        while (logged_in==0)
        {
            char username[8], password[16];
            printf("Login or Signup\n");
            static mode[8];
            fgets(mode, sizeof(mode), stdin);
            strtok(mode, "\n\r");
            if (strcmp(mode, "Login") == 0) 
            {
                printf("Please enter your username: ");
                scanf("%s", username);
                printf("Please enter your password: ");
                scanf("%s", password);
                char Login_cmd[32];
                memset(Login_cmd, 0, 32);
                memcpy(Login_cmd, "l ", 2);
                memcpy(Login_cmd + 2, username, strlen(username));
                memcpy(Login_cmd + 2+strlen(username), " ", 1);
                memcpy(Login_cmd + 3+strlen(username), password, strlen(password));
                char response[40];
               // printf("Login_cmd: %s\n", Login_cmd);
                client_send(client, Login_cmd, sizeof(Login_cmd));
                
                int n = client_recv(client, response, sizeof(response));
                if (n < 0) 
                {
                    printf("Log In Error\n");
                    continue;
                }
                response[n] = 0;
                //printf("%s\n", response);
                if (response[0]!='L') 
                {
                    logged_in=1;
                    printf("Welcome %s\n", username);
                    strcpy(pwd,"/");
                    strcat(pwd, username);
                }
                else printf("%s\n", response);  
            } 
            else if (strcmp(mode, "Signup") == 0) 
            {
                printf("Please enter your username: ");
                scanf("%s", username);
                printf("Please enter your password: ");
                scanf("%s", password);
                char Signup_cmd[40];
                memset(Signup_cmd, 0, 40);
                memcpy(Signup_cmd, "s ", 2);
                memcpy(Signup_cmd + 2, username, strlen(username));
                memcpy(Signup_cmd + 2+strlen(username), " ", 1);
                memcpy(Signup_cmd + 3+strlen(username), password, strlen(password));
              //  printf("Signup_cmd: %s\n", Signup_cmd);
                client_send(client, Signup_cmd, sizeof(Signup_cmd));
                char response[32];
                int n = client_recv(client, response, sizeof(response));
                if (n < 0) 
                {
                    printf("Sign Up Error\n");
                    continue;
                }
                response[n] = 0;
               // printf("%s\n", response);
                if (response[0]!='L') 
                { 
                    logged_in=1;
                    printf("Welcome %s\n", username);
                    strcpy(pwd,"/");
                    strcat(pwd, username);
                } 
                else printf("%s\n", response);
            } 
            else if (strcmp(mode, "Exit") == 0) 
            {
                client_send(client, "e", 2);
                break;
            } 
            else
            {
                printf("Invalid mode\n");
            }
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
        printf("%s%s> %s", cyan, pwd, reset);
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        client_send(client, buf, strlen(buf) + 1);
        int n = client_recv(client, buf, sizeof(buf));
        buf[n] = 0;
        if (buf[0]!='/') printf("%s\n", buf); else strcpy(pwd, buf);
        if (strcmp(buf, "Bye!") == 0) break;
        if (strcmp(buf, "Logout Success") == 0) logged_in=0;
    }
    client_destroy(client);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pthread.h>

#define NICKNAME_SIZE 50
#define BUF_SIZE 500
#define INPUT_SIZE 250

char send_buf[BUF_SIZE];
char recv_buf[BUF_SIZE];

char input[INPUT_SIZE];
char nickname[NICKNAME_SIZE];
int nickname_length = 0;

void error_handing(char *message);
//read & write
void read_routine(int sock);
void write_routine(int sock);

//socket
int sock;
int connect_server(char *ip, char *port);
void send_nickname(int sock);
void send_answer(int sock);
//output screen
void output_welcome(int sock);
void clear();
void GetWinSize(int *w, int *h);
int w, h;

int main(int argc, char *argv[]){
    pid_t pid;
    argc = 3;
    argv[1] = "127.0.0.1";
    argv[2] = "6666";
    if(argc != 3){
        printf("Usage : %s <IP> <port> \n", argv[0]);
        exit(1);
    }
    int sock = connect_server(argv[1], argv[2]);
    GetWinSize(&w,&h);
    output_welcome(sock);
    clear();
    printf("Waiting For Server......\n");
    pid = fork();
    if (pid == 0)
        read_routine(sock);
    else
        write_routine(sock);
    close(sock);
    return 0;
}

void output_welcome(int sock){
    clear();
    int i = 0, j = 0;
    while(1){
        for(i = 0; i < h; i ++)
            printf("-");
        printf("\n");
        for(i = 0; i < w - 3; i ++){
            printf("|");
            for(j = 1; j < h - 1; j ++)
                printf(" ");
            printf("|\n");
        }
        for(i = 0; i < h; i ++)
            printf("-");
        for(i = 0; i < h / 4; i ++)
            printf(" ");
        printf("\033[%dA", (w / 2));
        printf("Your Nickname : ");

        char temp;
        while ((temp = getchar()) != EOF && temp != '\n'){
            nickname[nickname_length++] = temp;
        }
        if(nickname_length > 20){
            printf("Nickname too long!\n");
            nickname_length = 0;
        }else{
            clear();
            send_nickname(sock);
            clear();
            break;
        }
    }
}

void clear(){
//    printf("\033[2J");
    printf("\033c");
}

void read_routine(int sock){
    while(1){
        int t = read(sock, recv_buf, BUF_SIZE);
        if(t == 0){
            printf("The Server shutdown!\n");
            return;
        }else if(strcmp(recv_buf, "Press any key to restart\n") == 0){
            printf("Press any key to Exit");
            getchar();
            return;
        }else{
            printf("%s", recv_buf);
        }
    }
}

void write_routine(int sock){
    while(1){
        int i = 0;
        int c = 0;
        char t = ' ';
        while ((t = getchar()) != '\n' && t != EOF){
            input[c ++] = t;
        }
        if(c > 250){
            printf("Input too long!\n");
            return;
        }
        for(i = 0; i < 303; i ++)
            send_buf[i] = 0;
        send_buf[0] = '2';
        for(i = 0; i < c; i ++){
            send_buf[i + 1] = input[i];
        }
        write(sock, send_buf, BUF_SIZE);
    }
}

void error_handing(char *message){
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int connect_server(char *ip, char *port){
    int sock;
    struct sockaddr_in serv_adr;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(ip);
    serv_adr.sin_port = htons(atoi(port));

    if(connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1){
        error_handing("connect() error!");
    }
    return sock;
}

void GetWinSize(int *w, int *h)
{
    struct winsize size;
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&size);
    *w = size.ws_row, *h = size.ws_col;
}

void send_nickname(int sock){
    if(nickname_length != 0){
        send_buf[0] = '1';
        int i = 0;
        for(i = 0; i < nickname_length; i ++){
            send_buf[i + 1] = nickname[i];
        }
        write(sock, send_buf, BUF_SIZE);
    }
}

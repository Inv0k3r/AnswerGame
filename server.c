#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <termios.h>


#define BUF_SIZE 500
#define EPOLL_SIZE 100
#define MAX_USER 100
#define MAX_QUESTION 50
#define MAX_LENGTH 500
#define MAX_ANS 8

//server socket
int serv_sock;
int clnt_sock;
struct sockaddr_in clnt_adr;
socklen_t adr_sz;
struct epoll_event *ep_events;
struct epoll_event event;//event
int epfd, event_cnt;


void error_handing(char *message);
//void send_bulletin();
//void listen_connect(char *port);

void listen_connect(int time);
void send_to_everyone(char *buf);
//question struct
struct Question{
    char q[MAX_LENGTH];
    char a[MAX_ANS][MAX_LENGTH];
    int ans_num;
    int correct;
    int score;
};
struct Question question[MAX_QUESTION];
int qu_num = 0;
void readFile();
char recv_buf[BUF_SIZE];
char send_buf[BUF_SIZE];// 300

//user queue
struct user{
    int socket;
    char username[100];
    int sum_score;
    int flag;
    int is_answer;
};
struct user user_list[MAX_USER];
int count = 0;
int is_empty();
int is_full();
void push(int user);
struct user pop(int user);
void set_username(int socket, char *username);
void add_score(int socket, int question_index);
void send_result();
void print_result();
void print_question_answer();
void sort();

int getch();
void init(char *port);
void clean(){
    printf("\033c");
}

int main(int argc, char *argv[]){
    argc = 2;
    argv[1] = "6666";
    if (argc != 2){
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    printf("Reading Question File ......\n");
    readFile();
    printf("Done!\n");
    int i = 0;
    int time = 5; // defalut time = 30s
    while(1){
        clean();
        printf("1. Start \n2. Show Question&Answer\n3. Set Time Out(default: 30s)\n4. Exit\n");
        char temp = getchar();
        if(temp - '0' == 1){
            clean();
            printf("Initing .....\n");
            init(argv[1]);
            printf("Done!\n");
            printf("Waiting for connect ... \n");
            listen_connect(time);
        }else if(temp - '0' == 2){
            //Show all question&answer
            print_question_answer();
            getchar();
        }else if(temp - '0' == 3){
            clean();
            printf("Enter the time:\n");
            scanf("%d", &time);
            if(time > 0){
                printf("Success for set time : %d\n", time);
            }else{
                printf("Please input true time!\n");
            }
            getchar();
            getchar();
        }else if(temp - '0' == 4){
            break;
        }
    }
    return 0;
}

void readFile(){
    char line[500];
    FILE *f;
    if((f = fopen("/home/a/answer_server/question_answer","r")) == NULL){
        error_handing("Can not open file!");
    }
    fgets(line, 500, f);
    qu_num = atoi(line);
    int i, j;
    for(i = 0; i < qu_num; i ++){
        fgets(question[i].q, 500, f);
        fgets(line, 500, f);
        question[i].score = atoi(line);
        fgets(line, 500, f);
        question[i].ans_num = atoi(line);
        for(j = 0; j < question[i].ans_num; j ++){
            fgets(question[i].a[j], 500, f);
        }
        fgets(line, 500, f);
        question[i].correct = atoi(line);
    }
    fclose(f);
}

void error_handing(char *message){
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void init(char *port){
    struct sockaddr_in serv_adr;
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(port));
    if(bind(serv_sock, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) == -1){
        error_handing("bind() error!");
    }
    if(listen(serv_sock, 5) == -1){
        error_handing("listen() error!");
    }
    epfd=epoll_create(EPOLL_SIZE);
    ep_events=(struct epoll_event*)malloc(sizeof(struct epoll_event)*EPOLL_SIZE);
    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd,EPOLL_CTL_ADD,serv_sock,&event);
    event.events = EPOLLIN;
    event.data.fd = 0;
    epoll_ctl(epfd,EPOLL_CTL_ADD,0,&event);
    int i = 0;
    for(i = 0; i < MAX_USER; i ++)
        user_list[i].flag = 0;
}

void listen_connect(int time){
    int str_len, i, j;
    int time_temp = -1;
    int flag = 0;
    //i : question
    //j : ep_events[j].data.fd
    //k : normal
    for(i = -1; i < qu_num; i ++){
        //send question
        start:
        if(i >= qu_num)
            break;
        if(flag == 1){
            printf("--------- %d -----------\n", i + 1);
            printf("Question %d : %s\n", i + 1, question[i].q);
            send_to_everyone(question[i].q);
            printf("True Answer : %s\n", question[i].a[question[i].correct]);
            for (j = 0; j < question[i].ans_num; j ++){
                printf("%s", question[i].a[j]);
                send_to_everyone(question[i].a[j]);
            }
            printf("---------sent success--------\n");
            char temp[500];
            snprintf(temp, 500, "You have %d second to answer\n", time);
            send_to_everyone(temp);

            //set every user that this user not answer this question
            //one user only can answer on quetion once
            int l = 0;
            for(l = 0; l < MAX_USER; l++){
                if(user_list[l].flag == 1)
                    user_list[l].is_answer = 0;
            }
        }
        while(1){
            if(flag == 0){
                printf("Press any key to Start!\n");
            }
            event_cnt=epoll_wait(epfd, ep_events, EPOLL_SIZE, (long)time_temp);
            if(event_cnt == -1){
                puts("epoll_wait() error");
                break;
            }
            if(event_cnt == 0){
                printf("Time Out!\n");
                printf("Nobody got the answer!\n");
                send_to_everyone("Time Out!\n");
                send_to_everyone("Nobody got the answer!\n");
                break;
            }
            for(j = 0; j < event_cnt; j++){
                if(ep_events[j].data.fd == 0){//std input
                    read(0,recv_buf, BUF_SIZE);
                    //remove std input
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[j].data.fd, NULL);
                    time_temp = time * 1000;
                    printf("Start send the question......\n");
                    flag = 1;
                    i = 0;
                    goto start;
                }
                else if(ep_events[j].data.fd == serv_sock){
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                    event.events = EPOLLIN;
                    event.data.fd = clnt_sock;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                    flag = 1;
                    if (!is_full())
                        push(clnt_sock);
                    else {
                        printf("User List Is Full!\n");
                    }
                }else {
                    str_len = read(ep_events[j].data.fd, recv_buf, BUF_SIZE);
                    if(str_len == 0){
                        pop(ep_events[j].data.fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[j].data.fd, NULL);
                        close(ep_events[j].data.fd);
                        //printf("closed client: %d \n", ep_events[j].data.fd);
                    }else{
                        if(recv_buf[0] == '1'){
                            char temp[BUF_SIZE];
                            int k = 0;
                            for(k = 0; k < strlen(recv_buf); k++){
                                temp[k] = recv_buf[k + 1];
                            }
                            //printf("%s : %s \n", recv_buf, temp);
                            set_username(ep_events[j].data.fd, temp);
                            printf("Press any key to Start!\n");
                        }else if (recv_buf[0] == '2'){
                            int l = 0;
                            for(l = 0; l < MAX_USER; l++){
                                if(user_list[l].flag == 1){
                                    char mess[500];
                                    if(user_list[l].socket == ep_events[j].data.fd){
                                        if(user_list[l].is_answer == 0){
                                            user_list[l].is_answer = 1;
                                            if (recv_buf[1] == 'A' + question[i].correct){
                                                add_score(ep_events[j].data.fd, i);
                                                strcpy(mess, "Congratulation! You got the answer!\n");
                                                write(ep_events[j].data.fd, mess, 500);
                                                i ++;
                                                goto start;
                                            }else{
                                                strcpy(mess, "Worng Answer!\n");
                                                write(ep_events[j].data.fd, mess, 500);
                                            }
                                        }else{
                                            strcpy(mess, "You have already answered this question.\n"
                                                         "Please wait for next\n");
                                            write(ep_events[j].data.fd, mess, 500);
                                        }
                                    }
                                }
                            }

                        }
                    }
                }
            }
        }
    }
    send_to_everyone("Challenge is end, Please wait for the result!\n");
    printf("Press any Key to Show Score List\n");
    getchar();
    getchar();
    sort();
    //print_result();
    close(serv_sock);
    close(epfd);
}

void send_to_everyone(char *buf){
    if(strlen(buf) != 0){
        int i = 0;
        for (i = 0; i < MAX_USER; i ++){
            if (user_list[i].flag == 1)
                write(user_list[i].socket, buf, BUF_SIZE);
        }
    }
}

int is_empty() {
    return count == 0 ? 1 : 0;
}

int is_full() {
    return (count == MAX_USER) ? 1 : 0;
}

void push(int user){
    int i = 0;
    for (i = 0; i < MAX_USER; i ++){
        if (user_list[i].flag == 0){
            strcpy(user_list[i].username, "null");
            user_list[i].socket = user;
            user_list[i].flag = 1;
            count += 1;
            return;
        }
    }
}

struct user pop(int user){
    struct user temp;
    temp.socket = -1;
    strcpy(temp.username, "null");
    if (is_empty())
     {
        return temp;
     }
     else
     {
        int i = 0;
        for (i = 0; i < MAX_USER; i ++){
            if (user_list[i].socket == user){
                strcpy(temp.username, user_list[i].username);
                temp.socket = user_list[i].socket;
                user_list[i].flag = 0;
                count -= 1;
                return temp;
            }
        }
     }
}

void set_username(int socket, char *username){
    if(strlen(username) != 0){
        int i = 0;
        for (i = 0; i < MAX_USER; i ++){
            if(user_list[i].flag == 1){
                if(user_list[i].socket == socket){
                    strcpy(user_list[i].username, username);
                    user_list[i].sum_score = 0;
                    printf("%s connected\n", user_list[i].username);
                    return;
                }
            }
        }
    }
}

void add_score(int socket, int question_index){
    int i = 0;
    for(i = 0; i < MAX_USER ; i++){
        if(user_list[i].flag == 1){
            if(user_list[i].socket == socket){
                user_list[i].sum_score += question[question_index].score;
                printf("%s got the answer to the %d\n",
                       user_list[i].username,
                        question_index + 1);
                return;
            }
        }
    }
}

void send_result(){
    struct user current[MAX_USER];
    int i = 0;
    char temp[300];
    int c = 0;
    for (i = 0; i < MAX_USER; i ++){
        if (user_list[i].flag == 1){
            strcpy(temp, user_list[i].username);
            strcat(temp, " : ");
            //strcat(temp, itoa(user_list[i].sum_score, sc, 5));
            strcat(temp, "\n");
            send_to_everyone(temp);
            strcpy(current[c].username, user_list[i].username);
            current[c].socket = user_list[i].socket;
            current[c].sum_score = user_list[i].sum_score;
            current[c].flag = user_list[i].flag;
        }
    }
}

void print_result(){
    send_to_everyone("Challenge End\n");
    send_to_everyone("---Score List---\n");
    printf("---Score List---\n");
    int i = 0;
    char temp[500];
    for(i = 0; i < MAX_USER ; i ++){
        if (user_list[i].flag == 1){
            snprintf(temp, 500, "%s : %d\n", user_list[i].username, user_list[i].sum_score);
            printf("%s", temp);
            send_to_everyone(temp);
        }
    }
    printf("Press any key to restart\n");
    send_to_everyone("Press any key to restart\n");
    getchar();
}

void print_question_answer(){
    int i = 0, j = 0;
    for (i = 0; i < qu_num; i ++){
        printf("Q[%d] : %s ==> %d Answer ==> Score : %d ==> Correct : %d\n",
               i, question[i].q, question[i].ans_num, question[i].score, question[i].correct);
        for (j = 0; j < question[i].ans_num; j ++){
            printf("%s", question[i].a[j]);
        }
    }
    printf("Press any key return to main menu\n");
    getchar();
}

void sort(){
    struct user u[MAX_USER];
    int i = 0, j = 0, c = 0;
    for(i = 0; i < MAX_USER; i ++){
        if(user_list[i].flag == 1){
            strcpy(u[c].username, user_list[i].username);
            u[c].sum_score = user_list[i].sum_score;
            c++;
        }
    }
    for(i = 0; i < c - 1; i ++){
        for(j = i + 1; j < c ; j ++){
            if(u[j].sum_score > u[i].sum_score){
                struct user temp;
                strcpy(temp.username, u[i].username);
                temp.sum_score = u[i].sum_score;
                strcpy(u[i].username, u[j].username);
                u[i].sum_score = u[j].sum_score;
                strcpy(u[j].username, temp.username);
                u[j].sum_score = temp.sum_score;
            }
        }
    }
    send_to_everyone("Challenge End\n");
    send_to_everyone("---Score List---\n");
    printf("---Score List---\n");
    for(i = 0; i < c; i ++){
        char temp[500];
        snprintf(temp, 500, "No.%d %s : %d\n", i + 1, u[i].username, u[i].sum_score);
        printf("%s", temp);
        send_to_everyone(temp);
    }
    printf("Press any key to restart\n");
    send_to_everyone("Press any key to restart\n");
    getchar();
}


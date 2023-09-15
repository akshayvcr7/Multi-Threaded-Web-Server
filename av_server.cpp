#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <thread>
#include <signal.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sstream>
#include <queue>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <error.h>


using namespace std;

#define MAX_SOC_Q 4096

pthread_mutex_t q_lock, print_lock;
pthread_cond_t q_cond;

struct query {
    string script_path;
    string query_str;
};

typedef query query_arg;

class server {
public:
    int socket_server, optn, port, buff, sock_accept, thread;
    struct sockaddr_in bind_addr;
    size_t addr_len;
    char ip_addr[INET_ADDRSTRLEN];
    queue<int> soc_req;
    server();
    void socket_connection_init(void);
    void socket_connection_accept(void);
    static void *serve_request(void *);

};

void get_image_path( string buff, char *img_path) {
    stringstream ss(buff);
    string word;
    char * buff_c = &buff[0];
    if (!getcwd(img_path, 100)) {
      perror("Couldn't read curdir");
    }
    //cout << "cur dir "<< img_path<< endl;

    while (ss >> word) {
        if (word[0] == '/')
            break;
    }
    cout << "word is " << word << endl;
    strcat(img_path, word.c_str());

}

char *read_file(FILE *fpipe) {
  int len = 100;
  char *file_buff = new char[len];
  int ind = 0;
  int c;
  while ((c = fgetc(fpipe)) != EOF) {
    file_buff[ind++] = c;
    if (ind == len) {
      len = len * 10;
      char *mod_buff = new char[len];
      memcpy(mod_buff, file_buff, len);
      free(file_buff);
      file_buff = mod_buff;
    }
  }
  file_buff[ind] = '\0';
  return file_buff;
}

query_arg query_string(string buff) {
    char cgi_path[1024];

    query_arg q1;
    string word;
    get_image_path(buff, cgi_path);
    cout << "cgi_path is "<< cgi_path << endl;
    stringstream ss(cgi_path);
    while (!ss.eof()) {
        getline(ss, word, '?');
        if (word[0] == '/') {
            q1.script_path = word;
            cout <<"script is "<< word << endl;
        }
        else {
            q1.query_str = word;
            cout <<"query is "<< word << endl;
        }
    }

    return q1;
}

void execute_cgi_script(void *S, string buff) {
    server *C = (server *)S;
    query_arg Q;
    char http[2048], com[1024];
    Q = query_string(buff);
    char *exec_cmd = new char[strlen(Q.script_path.c_str()) + 100];
    sprintf(exec_cmd, "QUERY_STRING='%s' ", Q.query_str.c_str());
    strcat(exec_cmd, Q.script_path.c_str());
    cout << "py cmd is " << exec_cmd << endl;

    FILE *file_stream = popen(exec_cmd, "r");

    if (!file_stream) {
        perror("file popen error");
    }
    else {
        char* result = read_file(file_stream);
        char http_header[25] = "HTTP/1.1 200 OK\r\n";
        strcpy(http, http_header);
        strcat(http, "Server: bot-av (Linux)\r\n");
        strcat(http, "Content-Type: text/html\r\n");
        sprintf(com , "Content-Length: %ld\r\n\r\n", strlen(result));
        strcat(http, com);
        strcat(http, result);
        write(C->soc_req.front(), http, strlen(http));
  }
  free(exec_cmd);
  pclose(file_stream);


}

void send_static_file(void *S, string buff){
    server *C = (server *)S;
    struct stat file_stat;
    char img_path[1024], http[2048], com[1024];

    int send_size;
    char http_header[25] = "HTTP/1.1 200 OK\r\n";
    get_image_path(buff, img_path);
    cout << "img_path is " << img_path << endl;
    strcpy(http, http_header);
    strcat(http, "Server: bot-av (Linux)\r\n");
    strcat(http, "Content-Type: text/html\r\n");

    int fd_img = open(img_path, O_RDONLY);

    if(fd_img < 0){
        printf("Cannot Open file path : %s with error %d\n", img_path, fd_img);
        exit(1);
    }
    fstat(fd_img, &file_stat);
    int img_size = file_stat.st_size;
    int blk_size = file_stat.st_blksize;
    sprintf(com , "Content-Length: %d\r\n\r\n", img_size);
    strcat(http, com);
    write(C->soc_req.front(), http, strlen(http));
    while(img_size > 0)
    {
        int send_size = (img_size > blk_size)?blk_size:img_size;
        if ((sendfile(C->soc_req.front(), fd_img, NULL, send_size)) < 0) {
            perror("send image fail");
            exit(1);
        }
        img_size = img_size - send_size;

    }
    close(fd_img);
    cout << "send_image successful" << endl;

}

server::server() {
    optn = 1;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(8080);
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    addr_len = sizeof(bind_addr);
}

void server::socket_connection_init(void)
{
    if ((socket_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation fail");
        exit(1);
    }

    if (setsockopt(socket_server, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &optn,
                   sizeof(optn)))
    {
        perror("setsockopt fail");
        exit(1);
    }
    // Forcefully binding the socket to the addr
    if (bind(socket_server, (struct sockaddr *)&bind_addr,
             sizeof(bind_addr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }

    if (listen(socket_server, MAX_SOC_Q) < 0)
    {
        perror("listen failed");
        exit(1);
    }
}

void server::socket_connection_accept(void)
{
    while (true)
    {
        cout << "accept started" << endl;
        if ((sock_accept = accept(socket_server, (struct sockaddr *)&bind_addr, (socklen_t *)&addr_len)) < 0)
        {
            perror("accept fail\n");
            exit(EXIT_FAILURE);
        }
        cout << endl
             << "-------Server Connection Started------" << endl;
        inet_ntop(AF_INET, (void *)&bind_addr.sin_addr.s_addr, ip_addr, INET_ADDRSTRLEN);

        cout << "ip addr is " << ip_addr << endl;
        pthread_mutex_lock(&q_lock);
        // enQueue(q_soc, new_socket);
        cout << "pushed the incoming requests" <<endl;
        soc_req.push(sock_accept);
        pthread_cond_signal(&q_cond);
        pthread_mutex_unlock(&q_lock);
        cout << "accept ended" << endl;
    }
}

void arg_info(void)
{
    cout << "options usage <p, t, b, s>" << endl;
    cout << "-p : --port_no - specify port value for comm (ex: 80)" << endl;
    cout << "-t : --threads - specify no of threads to spawn ( 1 - 20)" << endl;
    cout << "-b : --buff - specify buff to log incoming requests (1 - 20)" << endl;
    cout << "-s : --shed_algo : implies sheduling algorithm to be used (FIFO, RR) " << endl;
}

void *server::serve_request(void *S)
{
    // pthread_detach(pthread_self());
    server *server_inst = (server *)S;
    while(1) {
    int valread;
    const char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 22\n\nHello from bot server!";
    char buffer[1024];
    pthread_mutex_lock(&q_lock);

    // Queue empty
    if (server_inst->soc_req.empty())
    {
        printf("thread_cond_wait %ld\n", pthread_self());
        pthread_cond_wait(&q_cond, &q_lock);
    }

    valread = read(server_inst->soc_req.front(), buffer, 1024);
    printf("%s\n", buffer);

    string s(buffer);
    if (s.find(".py") == string::npos)
    {
        cout << "request type - static" <<endl;
        cout << "server recieve buffer is " << buffer << endl;
        send_static_file(S, s);
    }
    else {
        cout << "request type - dynamic" <<endl;
        execute_cgi_script(S, s);
    }

    //send(server_inst->soc_req.front(), hello, strlen(hello), 0);
    printf("client info sent from %ld\n", pthread_self());
    server_inst->soc_req.pop();

    pthread_mutex_unlock(&q_lock);

    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int opt;
    server *socket = new server();

    // put ':' in the starting of the string so that program can
    // distinguish between '?' and ':'
    while ((opt = getopt(argc, argv, ":p:t:b:s:h")) != -1)
    {
        switch (opt)
        {
        case 'h':
            arg_info();
            exit(1);
        case 'p':
        case 't':
        case 'b':
        case 's':
            if (opt == 'p') {
                printf("port no is : %s\n", optarg);
                sscanf(optarg, "%d", &socket->port);
            }
            if (opt == 't') {
                printf("threads spwan : %s\n", optarg);
                sscanf(optarg, "%d", &socket->thread);
            }
            if (opt == 'b') {
                printf("buff size : %s\n", optarg);
                sscanf(optarg, "%d", &socket->buff);
            }
            if (opt == 's')
                printf("shed algo is : %s\n", optarg);
            break;
        case ':':
            printf("option %c needs a value \n", optopt);
            break;
        case '?':
            printf("unknown option: %c\n", optopt);
            break;
        }
    }

    pthread_t th[socket->thread];

    socket->socket_connection_init();

    pthread_mutex_init(&q_lock, NULL);
    pthread_cond_init(&q_cond, NULL);

    for (int i = 0; i < socket->thread; i++)
    {
        pthread_create(&th[i], NULL, server::serve_request, (void *)socket);
    }

    socket->socket_connection_accept();


exit:
    return 0;
}
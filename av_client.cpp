#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <thread>
#include <signal.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <queue>

using namespace std;

#define HTTP_REQUEST 1024


class client {
public:
    int sock, valread, client_fd, port, shed_algo, thread;
    char *host;
    pthread_barrier_t barrier;
    pthread_barrierattr_t attr;
    struct sockaddr_in serv_addr;
    const char* hello;
    char buffer[2048];
    queue<int> client_q;
    client();
    static void *socket_connect(void *);
    ~client();
};

//Constructor
client::client() {
    hello = "Hello from client";
    host = new char[HTTP_REQUEST];
    memset(buffer, 0, sizeof(buffer));
    serv_addr.sin_family = AF_INET;
    sock = 0;
}

//Destructor
client::~client() {
    delete[] host;
}

void *client::socket_connect(void *C)
{
    client * Cli = (client *)C;
    int sock, client_fd, ret;
    ret = pthread_barrier_wait(&Cli->barrier);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("\n Socket creation error \n");
        exit(1);
    }
    //serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if ((inet_pton(AF_INET, "127.0.0.1", &Cli->serv_addr.sin_addr)) <= 0)
    {
        perror("\nInvalid address/ Address not supported \n");
        exit(1);
    }

    if ((client_fd = connect(sock, (struct sockaddr*)&Cli->serv_addr, sizeof(serv_addr)))< 0)
    {
        perror("\nConnection Failed \n");
        exit(1);
    }

    char *host_form = new char[64 + strlen(Cli->host)];
    sprintf(host_form, "GET %s HTTP/1.1", Cli->host);

    send(sock, host_form, strlen(host_form), 0);
    printf("http 'GET' sent to server\n");
    read(sock, Cli->buffer, 2048);

    printf("%s %ld\n", Cli->buffer, pthread_self());
    delete[] host_form;

    return NULL;
}


void arg_info(void)
{
    cout << "options usage <p, t, i, s>" << endl;
    cout << "-p : --port_no - specify port value for comm (ex: 80)" << endl;
    cout << "-t : --threads - specify no of threads to spawn ( 1 - 20)" << endl;
    cout << "-s : --shed_algo : implies sheduling algorithm to be used (FIFO, Concurrent) " << endl;
}

int main(int argc, char *argv[])
{
    int opt;
    client *socket = new client();
    char *a;

    // put ':' in the starting of the string so that program can
    // distinguish between '?' and ':'
    while ((opt = getopt(argc, argv, ":p:t:s:h:i")) != -1) {
        switch (opt) {
        case 'i':
            arg_info();
            exit(1);
        case 'h':
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
            if (opt == 's') {
                printf("shed algo is : %s\n", optarg);
                sscanf(optarg, "%d", &socket->shed_algo);
            }
            if (opt == 'h') {
                printf("host is : %s and len is %ld\n", optarg, strlen(optarg));
                memcpy(socket->host, optarg, strlen(optarg));
            }
            break;
        case ':':
            printf("option %c needs a value \n", optopt);
            break;
        case '?':
            printf("unknown option: %c\n", optopt);
            break;
        }
    }
    socket->serv_addr.sin_port = htons(8080);

    pthread_t cli_th[socket->thread];

    unsigned count =  socket->thread;
    int ret;
    ret = pthread_barrier_init(&socket->barrier, &socket->attr, count);
    if (ret < 0)
    {
        perror("barrier init fail");
    }

    //while(1) {

    for (int i = 0; i < socket->thread; i++) {
        pthread_create(&cli_th[i], NULL, client::socket_connect, (void *)socket);
    }

    for (int i = 0; i < socket->thread; i++) {
        pthread_join(cli_th[i], NULL);
    }

    for (int i=0; i<socket->thread; i++)
	{
		//pthread_create(&thread_scheduler[i],NULL, client, NULL);
        printf("thread_id %ld\n",cli_th[i]);
	}
    //}

    return 0;
}

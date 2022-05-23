#include "csapp.h"



typedef struct{
    char method[MAXLINE];
    char uri[MAXLINE];
    char suffix[MAXLINE];
    char version[MAXLINE];
    char domain[MAXLINE];
    char port[MAXLINE];
    void* next;
}request_line;

typedef struct{
    char header[MAXLINE];
    char data[MAXLINE];
}header_line;

typedef struct cache_node
{
  char hostname[MAXLINE];
  char path[MAXLINE];
  char* data;
  size_t size;
  struct cache_node* next;
  clock_t last_access_time;
} cache_node;


header_line* header_line_list;




static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void make_request(char* request_buf, request_line *line, char* buf);

void do_proxy(int fd);

void parse_header(char* buf);

int main(int argc, char **argv)
{
    int connfd, listenfd;
    struct sockaddr_storage clientaddr;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t addrlen;
    pthread_t tid;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    addrlen = sizeof(clientaddr);

    //Open port by number argv[1]
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        //connfd is socket connected to client
        connfd = Accept(listenfd, (SA *)&clientaddr, &addrlen);

        Getnameinfo(&clientaddr, addrlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        Pthread_create(&tid, NULL, do_proxy, connfd);

    }
}

void do_proxy(int connfd)
{
    Pthread_detach(Pthread_self());
    struct stat sbuf;
    char buf[MAXLINE];
    char request_buf[MAXLINE], response_msg_buf[MAXLINE];
    request_line* line = Malloc(sizeof(request_line));;
    rio_t rio_client_read, rio_server_read;
    
    int to_serverfd;
    struct hostent *serverp;
    struct sockaddr_in serveraddr;

    Rio_readinitb(&rio_client_read, connfd);
    Rio_readlineb(&rio_client_read, buf, MAXLINE);

    printf("connect request: %s\n",buf);
    
    parse_request(line, &request_buf, &buf);

    to_serverfd = Open_clientfd(line->domain, line->port);
    memset(buf, 0, strlen(buf));


    //for extra headers
    Rio_readlineb(&rio_client_read, buf, MAXLINE);
    
    while(strcmp(buf, "\r\n"))
    {
        parse_header(buf);
        memset(&buf[0], 0, sizeof(buf)); //flushing buffer
        Rio_readlineb(&rio_client_read, buf, MAXLINE);
    }

    Rio_writen(to_serverfd, request_buf, strlen(request_buf));
    Rio_readinitb(&rio_server_read, to_serverfd);

    int length;

    while((length = Rio_readnb(&rio_server_read, response_msg_buf, MAXLINE))>0){
        //printf("response: %s\n", response_msg_buf);
        Rio_writen(connfd, response_msg_buf, length);
    }
    Close(connfd);
    Close(to_serverfd);
    return;
}

void parse_request(request_line* line, char* request_buf, char* buf){
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char suffix[MAXLINE], domain[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
    char port[MAXLINE];

    sscanf(buf, "%s %s %s", method, uri, version);

    /*if(strcmp("GET", method)!=0){
        fprintf(stderr, "method only GET\n");
        return;
    }*/

    //make socket to communicate with server


    parse_request_line(uri, suffix, domain, port, filename, cgiargs); 


    strcpy(line->uri, uri);
    strcpy(line->domain, domain);
    strcpy(line->method, "GET");
    strcpy(line->suffix, suffix);
    strcpy(line->version, "HTTP/1.1");
    strcpy(line->port, port);

    make_request(request_buf, line, buf);
}

void parse_header(char* buf){

}


void make_request(char* request_buf, request_line *line, char* buf){

    //make request line
    


    strcpy(request_buf,"");
    strcat(request_buf, line->method);
    strcat(request_buf, " ");
    strcat(request_buf, line->suffix);
    strcat(request_buf, " ");
    strcat(request_buf, line->version);
    strcat(request_buf, "\r\n");

    //host line
    strcat(request_buf, "Host: ");
    strcat(request_buf, line->domain);
    strcat(request_buf, "\r\n");


    //accept line
    
    strcat(request_buf, "Accept: */*");
    strcat(request_buf, "\r\n");

    //
    strcat(request_buf, user_agent_hdr);

    strcat(request_buf, "Connection: close");
    strcat(request_buf, "\r\n");

    strcat(request_buf, "Proxy-Connection: close");
    strcat(request_buf, "\r\n");
    

    strcat(request_buf, "\r\n");

    return;
}

void parse_request_line(char* uri, char* suffix, char* domain, char *port, char* filename, char* cgiargs){

    int is_static=1;
    char *hostp, *portp, *suffixp;
    char *port_tempp, *argp;

    strcpy(port, "80");
    if(!strstr(uri, "http://")){
        fprintf(stderr, "uri must starts with http://");
        exit(1);
    }
    if(strstr(uri, "cgi-bin")){
        is_static = 0;
    }
    hostp = uri + 7;
    portp = strchr(hostp, ':');
    suffixp = strchr(hostp, '/');

    //if there is port num
    if(portp) {
        strncpy(domain, hostp, portp - hostp);
        strncpy(port, portp + 1, suffixp-portp-1);
    }
    //without port num and with suffix
    else if(suffixp) strncpy(domain, hostp, suffixp - hostp);
    else strcpy(domain, hostp);

    //if static
    if(is_static){
        strcpy(cgiargs, "");
        strcpy(filename, "");    
        strcat(filename, suffixp);
        if (!suffixp||*(suffixp+1)=='\0')   
            strcat(filename, ""); 
    }
    //else dynamic
    else{
        argp = index(uri, '?');
        if (argp)
        {
            strcpy(cgiargs, argp + 1);
            *argp = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename,"");
        strcat(filename, suffixp);
    }
    strcpy(suffix, filename);
    strcat(suffix, cgiargs);
    return;
}
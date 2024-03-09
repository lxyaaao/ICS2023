/*
 * proxy.c
 *
 * 刘昕垚 2200012866 
 * 
 * proxy main code
 * In this lab, I achieve a simple HTTP proxy that caches web objects
 * 
 */

#include <stdio.h>
#include "csapp.h"
#include "cache.h"
 
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
 
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
 
void doit(int fd);
void parse_uri(char *uri, char *filename, char *host, char *port);
void build_header(char *header, char *hostname, char *path, 
    char *port, rio_t *client_rio);
void clienterror(int fd, char *cause, char *errnum, 
    char *shortmsg, char *longmsg);
 
/* 
 * thread - The thread routine function to detach the thread.
 *          Almost the same as code on book.
 */
void* thread(void* vargp) {
    int connfd = *(int*)vargp;
    Pthread_detach(Pthread_self());
    free(vargp);
    doit(connfd);
    close(connfd);
    return NULL;
}
 
void sigchld_handler(int sig) { // reap all children
    int bkp_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = bkp_errno;
}
 
int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);
 
    int listenfd;
    int *connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
 
    /* Check command line args */
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
 
    for(int i = 0; i < CACHE_SIZE; ++ i)
        cache_init(i);
 
    listenfd = open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = malloc(sizeof(int));
        /* line:netp:tiny:accept */
        *connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
            getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                        port, MAXLINE, 0);
            printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }

    close(listenfd);
}
 
/*
 * doit - handle one HTTP request/response transaction
 *        handle request to server and fetch response from server
 */
void doit(int fd)  {
    rio_t rio;
 
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], host[MAXLINE], port[MAXLINE];
    char req_header_buf[MAXLINE];
 
    /* Read request line and headers */
    rio_readinitb(&rio, fd);

    /* line:netp:doit:readrequest */
    if(!rio_readlineb(&rio, buf, MAXLINE)) 
        return;
    printf("%s", buf);
    
    /* line:netp:doit:parserequest */
    if(sscanf(buf, "%s %s %s", method, uri, version) != 3) 
        return ;
        
    if(strcasecmp(method, "GET")) {    //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                  //line:netp:doit:endrequesterr
 
    int index = cache_find(fd, uri);
    /* Cache hit */
    if(index != -1) {
        cache_LRU(index);
        return ;
    }
 
    /* Back-up the URI */
    char old_uri[MAXLINE];
    strcpy(old_uri, uri);
 
    /* Parse URI from GET request */
    parse_uri(uri, filename, host, port);
 
    /* Connect with server */
    int clientfd = open_clientfd(host, port);
 
    /* Read http request form server */
    strcpy(req_header_buf, uri);
    build_header(req_header_buf, host, filename, port, &rio);
 
    /* Send response back to client */
    rio_readinitb(&rio, clientfd);
    rio_writen(clientfd, req_header_buf, strlen(req_header_buf));
 
    char cachebuf[MAX_OBJECT_SIZE] = { 0 };
    int buf_size = 0;
    size_t n;
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        /* Make sure the object is not too large */
        if(buf_size + n < MAX_OBJECT_SIZE) {
            memcpy(cachebuf + buf_size, buf, n);
            buf_size += n;
        }
            
        rio_writen(fd, buf, n);
    }
 
    /* Update cache */
    if(buf_size < MAX_OBJECT_SIZE)
        cache_update(old_uri, cachebuf);
 
    close(clientfd);
}
 
/*
 * parse_uri - parse URI 
 */
void parse_uri(char *uri, char *filename, char *host, char *port) {
    /* Default port as 80 */
    strcpy(port, "80");
    char *ptr1 = strstr(uri, "//");
 
    if(!ptr1) 
        ptr1 = uri;
    else 
        ptr1 += 2;
 
    char *ptr2 = strstr(ptr1, ":");
    if(ptr2) {
        /* Separate the string */
        *ptr2 = '\0';
 
        int tmp;
        sscanf(ptr2 + 1, "%d%s", &tmp, filename);
        sprintf(port, "%d", tmp);
 
        strcpy(host, ptr1);
    }
    else {
        ptr2 = strstr(ptr1, "/");
        strcpy(filename, ptr2);
 
        if(ptr2) {
            *ptr2 = '\0';
            strcpy(host, ptr1);
        }
    }
 
    return ;
}
 
/* 
 * build_header - Read request header from client.
 */
void build_header(char *header, char *hostname, char *filename, 
    char *port, rio_t *rio) {
        
    char buf[MAXLINE], result[MAXLINE], tmp[MAXLINE];
 
    sprintf(result, "GET %s HTTP/1.0\r\n", filename);
    sprintf(tmp, "Host: %s\r\n", hostname);
    strcat(result, tmp);
 
    while(rio_readlineb(rio, buf, MAXLINE) > 0 && strcmp(buf, "\r\n")) {
 
        /*ignore repeated header*/
        if (strstr(buf, "Host:") != NULL)
            continue;
        if (strstr(buf, "User-Agent:") != NULL)
            continue;
        if (strstr(buf, "Connection:") != NULL)
            continue;
        if (strstr(buf, "Proxy-Connection:") != NULL)
            continue;
 
        strcat(result, buf);
    }
 
    sprintf(tmp, "User-Agent: %s", user_agent_hdr);
    strcat(result, tmp);
    strcat(result, "Connection: false\r\n");
    strcat(result, "Proxy-Connection: false\r\n");
    strcat(result, "\r\n");
    strcpy(header, result);
 
    return;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
#pragma GCC diagnostic pop

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}
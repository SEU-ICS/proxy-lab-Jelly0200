#include <stdio.h>
#include <stdlib.h>
#include <csapp.h>
#include <string.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_LINE_SIZE 15

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

struct rwlock {
    sem_t lock;
    sem_t writelock;
    int readers;
};

struct cache {
    int available;
    char url[MAX_LINE_SIZE];
    char value[MAX_OBJECT_SIZE];
};

struct urldata {
    char host[MAX_LINE_SIZE];
    char port[MAX_LINE_SIZE];
    char path[MAX_LINE_SIZE];

};

struct cache Cache[MAX_CACHE_SIZE];
struct rwlock* Rw;
int pointer;

void rwinit() {
    Rw->readers = 0;
    sem_init(&Rw->lock, 0, 1);
    sem_init(&Rw->writelock, 0, 1);
}

void thread(void* v) {
    int fd = *(int*)v;
    Pthread_detach(pthread_self());
    go(fd);
    close(fd);

}

void go(int fd) {
    char buf[MAX_LINE_SIZE], method[MAX_LINE_SIZE].version[MAX_LINE_SIZE];
    char new_httpdata[MAX_LINE_SIZE], urltemp[MAX_LINE_SIZE];
    struct UrlData u;
    rio_t rio, server_rio;
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAX_LINE_SIZE);

    sscanf(buf, "%s %s %s", method, url, version);
    strcpy(urltemp, url);

    if (strcmp(method, "GET") != 0) {
        printf("The proxy can not handle thid method: %s\n", method);
        return;
    }

    if (readcache(fd, urltemp) != 0) {
        return;

    }
    parse_url(url, &u);
    change_httpdata(&rio, &u, new_httpdata);

    int server_fd = Open_clientfd(u.host, u.port);
    size_t n;

    Rio_readinitb(&server_rio, server_fd);
    Rio_writen(server_fd, new_httpdata, strlen(new_httpdata));

    char cache[MAX_OBJECT_SIZE];
    int sum = 0;
    while ((n = Rio_readlineb(&server_rio, buf, MAX_LINE_SIZE)) != 0) {
        Rio_writen(fd, buf, n);
        sum += n;
        strcat(cache, buf);

    }
    printf("proxy send %ld bytes to client\n", sum);
    if (sum < MAX_OBJECT_SIZE)writecache(cache, urltemp);
    close(server_fd);
    return;
} 

void writecache(char* buf, char* key) {
    sem_wait(&rw->writelock);
    int index;
    while (Cache[pointer].available != 0) {
        Cache[pointer].available = 0;
        pointer = (pointer + 1) % MAX_CACHE_SIZE;
    }
    index = pointer;
    Cache[index].available = 1;
    strcpy(Cache[index].key, key);
    strcpy(Cache[index].value, buf);
    sem_post(&Rw->writelock);
    return;

}

int readcache(int fd, char* url) {
    sem_wait(&Rw->lock);
    if (Rw->readers == 0) sem_wait(&Rw->writelock);
    Rw->readers++;
    sem_post(&Rw->lock);
    int i, flag = 0;
    for (i = 0; i < MAX_CACHE_SIZE; i++) {
        if (strcmp(url, Cache[i].key) == 0) {
            Rio_writen(fd, Cache[i].value, strlen(Cache[i].value));
            printf("proxy send %d bytes to client\n", strlen(Cache[i].value));
            Cache[i].available = 1;
            flag = 1;
            break;
        }
    }
    sem_wwait(&Rw->lock);
    Rw->readers--;
    if (Rw->readers == 0)sem_post(&Rw->writelock);
    sem_post(&Rw->lock);
    return flag;

}

void parse_url(char* url, struct UrlData* u) {
    char* hostpose = strstr(url, "//");
    if (hostpose == NULL) {
        char* pathpose = strstr(url, "/");
        if (pathpose != NULL) strcpy(u->path, pathpose);
        strcpy(u->port, "80");
        return;
    }
    else {
        char* portpose = strstr(hostpose + 2, "/");
        if (pathpose != NULL) {
            strcpy(u->path, pathpose);
            strcpy(u->port, "80");
            *pathpose = "\0";

        }
        strcpy(u->host, hostpose + 2);
    }
    return; 
}

void change_httpdata(rio_t* rio, struct UrlData* url, char* new_httpdata) {
    static const char* Con_hdr = "Connection:close\r\n";
    static const char* Pcon_hdr = "Proxy-Connection:close\r\n";
    char buf[MAX_LINE_SIZE];
    char Reqline[MAX_LINE_SIZE], Host_hdr[MAX_LINE_SIZE], Cdata[MAXLINE_SIZE];
    sprintf(Reqline, "Get %s HTTP/1.0\r\n", url->path);
    while (Rio_readlineb(rio, buf, MAX_LINE_SIZE) > 0) {
        if (strcmp(buf, "\r\n") == 0) {
            strcat(Cdata, "\r\n");
            break;
        }
        else if (strncasecmp(buf, "Host:", 5) == 0) {
            strcpy(Host_hdr, buf);
        }
        else if (!strncasecmp(buf, "Connection:", 11) && !strncasecmp(buf, "Proxy_Connection:", 17) && !strncasecmp(buf, "User_agent:", 11)) {
            strcat(Cdata, buf);

        }
        if (!strlen(Host_hdr)) {
            sprintf(Host_hdr, "Host:%s\r\n", url->host);

        }
        sprintf(new_httpdata, "%s%s%s%s%s%s", Reqline, Host_hdr, Con_hdr, Pcon_hdr, user_agent_hdr, Cdata);
        return;
    }
}

int main(int argc, char** argv) {
    Rw = Malloc(sizeof(struct rwlock));
    printf("%s", user_agent_hdr);
    pthread_t tid;
    int listenfd, connfd;
    char hostname[MAX_LINE_SIZE], port[MAX_LINE_SIZE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    rwinit();
    if (argc != 2) {
        fprintf(stderr, "usage:%s<port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAX_LINE_SIZE, port, MAX_LINE_SIZE, 0);
        printf("Accept connection from (%s%s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, (void*)&connfd);
    }
    return 0;


}
{
    printf("%s", user_agent_hdr);
    return 0;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "http_server.h"
#include "config.h"
#include "log.h"
#include "tty.h"
#include "version.h"

#define PORT     9001
#define BUF_SIZE 4096

int is_server_running = 0;

void send_404(int client_fd) {
    const char *msg =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "404 Not Found";
    write(client_fd, msg, strlen(msg));
}

void send_400(int client_fd) {
    const char *msg =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Bad Request";
    write(client_fd, msg, strlen(msg));
}

const char *get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "text/plain; charset=utf-8";

    if (strcmp(ext, ".html") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0)
        return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml; charset=utf-8";

    return "text/plain; charset=utf-8";
}

int get_ip_addr(char *ip, size_t ip_len) {
    int sockfd;
    struct sockaddr_in serv;
    struct sockaddr_in local;
    socklen_t addr_len = sizeof(local);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        goto fallback;

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(1);
    inet_pton(AF_INET, "10.255.255.255", &serv.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
        goto fallback;

    if (getsockname(sockfd, (struct sockaddr *)&local, &addr_len) < 0)
        goto fallback;

    if (!inet_ntop(AF_INET, &local.sin_addr, ip, ip_len))
        goto fallback;

    close(sockfd);
    return 0;

fallback:
    if (sockfd >= 0)
        close(sockfd);
    strncpy(ip, "127.0.0.1", ip_len);
    return -1;
}

void show_http_server_info(int port) {
    // Kperf v0.0.1

    // ➜  Local:   http://127.0.0.1:36002/kperf-result/index.html
    // ➜  Remote:  http://192.168.1.125:36002/kperf-result/index.html
    // ➜  press q / CTRL+C to quit
    printf("\n");
    printf("    %sKperf v%s%s\n", GREEN, get_version_str(), RESET);
    printf("\n");
    printf("    %s%s%s Local:   %shttp://127.0.0.1:%d/%s/index.html%s\n",
           GREEN,
           ARROW_CHAR,
           RESET,
           BLUE,
           port,
           KPERF_RESULTS_PATH,
           RESET);

    char ip[INET_ADDRSTRLEN];
    get_ip_addr(ip, sizeof(ip));
    printf("    %s%s%s Remote:  %shttp://%s:%d/%s/index.html%s\n",
           GREEN,
           ARROW_CHAR,
           RESET,
           BLUE,
           ip,
           port,
           KPERF_RESULTS_PATH,
           RESET);
    printf("    %s%s%s press    %sCTRL+C%s to quit\n", GREEN, ARROW_CHAR, RESET, BOLD, RESET);
    printf("\n");
}

int start_http_server(int port) {
    if (port == 0) {
        port = PORT;
    }
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buf[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        ERR("socket error\n");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        ERR("bind error\n");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        ERR("listen error\n");
        exit(1);
    }

    show_http_server_info(port);
    is_server_running = 1;

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            ERR("accept error\n");
            continue;
        }

        int n = read(client_fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            close(client_fd);
            continue;
        }
        buf[n] = '\0';

        /* parse http request */
        char method[8], path[256];
        if (sscanf(buf, "%7s %255s", method, path) != 2) {
            send_400(client_fd);
            close(client_fd);
            ERR("fail to parse http request\n");
            continue;
        }
        DEBUG("Request: %s %s\n", method, path);

        if (strcmp(method, "GET") != 0) {
            send_400(client_fd);
            WARNING("receive %s\n", buf);
            close(client_fd);
            continue;
        }

        // 防止目录穿越
        if (strstr(path, "..")) {
            send_400(client_fd);
            close(client_fd);
            continue;
        }

        // 默认文件
        if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
        }
        // if end with /, add index.html
        if (path[strlen(path) - 1] == '/') {
            strcat(path, "index.html");
        }

        char file_path[512];
        snprintf(file_path, sizeof(file_path), ".%s", path);

        int fd = open(file_path, O_RDONLY);
        if (fd < 0) {
            send_404(client_fd);
            close(client_fd);
            continue;
        }

        struct stat st;
        fstat(fd, &st);

        // 发送 HTTP 头
        char header[256];
        snprintf(header,
                 sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %ld\r\n"
                 "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                 "Pragma: no-cache\r\n"
                 "Expires: 0\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 get_content_type(file_path),
                 st.st_size);

        write(client_fd, header, strlen(header));

        // 发送文件内容
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf))) > 0) {
            write(client_fd, buf, r);
        }

        DEBUG("write file %s to client\n", file_path);

        close(fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

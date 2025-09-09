/*
 *  check-debian-sources.c
 *  gcc -O2 -o check-debian-sources check-debian-sources.c -lcurl
 *  sudo ./check-debian-sources
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ---------- 颜色 ---------- */
#define GREEN  "\033[32m[✓]\033[0m"
#define RED    "\033[31m[✗]\033[0m"
#define YELLOW "\033[33m[*]\033[0m"
#define CYAN   "\033[36m"

/* ---------- 内存缓冲 ---------- */
struct mem {
    char  *data;
    size_t size;
};
static size_t mem_write(void *c, size_t sz, size_t n, struct mem *u)
{
    size_t r = sz * n;
    u->data = realloc(u->data, u->size + r + 1);
    if (!u->data) return 0;
    memcpy(u->data + u->size, c, r);
    u->size += r;
    u->data[u->size] = 0;
    return r;
}

/* ---------- 通用 HTTP 下载 ---------- */
int fetch_xml(const char *url, struct mem *buf)
{
    CURL *c = curl_easy_init();
    if (!c) return 0;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, mem_write);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, buf);
    CURLcode res = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);
    return res == CURLE_OK && code == 200 && buf->size > 0;
}

/* ---------- TCP 探活 ---------- */
int tcp_ok(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) { close(fd); return 0; }
    int ok = connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0;
    close(fd);
    return ok;
}

/* ---------- 测试 URL 是否 200 ---------- */
int url_ok(const char *url)
{
    CURL *c = curl_easy_init();
    if (!c) return 0;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    CURLcode r = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);
    return r == CURLE_OK && code == 200;
}

/* ---------- 先拉 /man.xml，再决定后续 ---------- */
int probe_mirror(const char *name, const char *mirror)
{
    char man_url[256];
    snprintf(man_url, sizeof(man_url), "%s/man.xml", mirror);
    printf(YELLOW "   先测 %s /man.xml …\n", name);
    struct mem buf = {0};
    if (fetch_xml(man_url, &buf)) {
        printf(CYAN "--- 站点维护公告 man.xml ---\n");
        fwrite(buf.data, 1, buf.size > 4096 ? 4096 : buf.size, stdout);
        printf("\n--- 公告结束 ---\n");
        free(buf.data);
        printf(RED "   %s 处于维护模式（跳过 Release 测试）\n", name);
        return 0;   /* 维护中=不可用 */
    }
    free(buf.data);

    /* 无公告 → 继续原流程 */
    char rel_url[256];
    snprintf(rel_url, sizeof(rel_url), "%sdists/stable/Release", mirror);
    printf(YELLOW "   再测 %s Release …\n", name);
    if (url_ok(rel_url)) {
        printf(GREEN "   %s 可用\n", name);
        return 1;
    }
    /* Release 也失败 → 二次兜底 man.xml 已测过，无需重复 */
    printf(RED "   %s 不可用\n", name);
    return 0;
}

/* ---------- 步骤 1：apt ---------- */
int test_current_source(void)
{
    printf(YELLOW " 测试当前软件源 (apt update) …\n");
    int rc = system("apt update >/dev/null 2>&1");
    if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
        printf(GREEN " 当前软件源正常\n");
        return 1;
    }
    printf(RED " 当前软件源失败\n");
    return 0;
}

/* ---------- 步骤 2：镜像站 ---------- */
int test_mirrors(void)
{
    struct { const char *name, *url; } list[] = {
        {"Tuna", "https://mirrors.tuna.tsinghua.edu.cn/debian/"},
        {"USTC", "https://mirrors.ustc.edu.cn/debian/"},
        {"Official", "https://deb.debian.org/debian/"},
        {NULL, NULL}
    };
    printf(YELLOW " 测试备用镜像站 …\n");
    int found = 0;
    for (size_t i = 0; list[i].name; ++i)
        if (probe_mirror(list[i].name, list[i].url)) found = 1;
    return found;
}

/* ---------- 步骤 3：DNS/HTTP/HTTPS 分层 ---------- */
void test_network(void)
{
    printf(YELLOW " 网络分层检测 …\n");
    const char *dns[]   = {"8.8.8.8", "1.1.1.1", "223.5.5.5"};
    const char *http[]  = {"http://detectportal.firefox.com/success.txt", "http://www.baidu.com"};
    const char *https[] = {"https://www.cloudflare.com/", "https://www.baidu.com"};

    int dns_ok = 0, http_ok = 0, https_ok = 0;

    for (size_t i = 0; i < sizeof(dns)/sizeof(*dns); ++i)
        if (tcp_ok(dns[i], 53)) { dns_ok = 1; break; }
    printf(dns_ok ? GREEN " DNS 53：可达\n" : RED " DNS 53：不通\n");

    for (size_t i = 0; i < sizeof(http)/sizeof(*http); ++i)
        if (url_ok(http[i])) { http_ok = 1; break; }
    printf(http_ok ? GREEN " HTTP 80：可达\n" : RED " HTTP 80：不通\n");

    for (size_t i = 0; i < sizeof(https)/sizeof(*https); ++i)
        if (url_ok(https[i])) { https_ok = 1; break; }
    printf(https_ok ? GREEN " HTTPS 443：可达\n" : RED " HTTPS 443：不通\n");

    if (dns_ok && http_ok && https_ok)
        printf(GREEN "网络层全部正常，镜像站集体维护或被外部屏蔽。\n");
    else if (dns_ok)
        printf(RED "网络部分受限（DNS 正常，HTTP/HTTPS 异常），请检查防火墙/代理。\n");
    else
        printf(RED "网络完全不可达，请检查本地网络、DNS、网线/WiFi。\n");
}

/* ---------- main ---------- */
int main(int argc, char *argv[])
{
    if (geteuid() != 0) {
        fprintf(stderr, RED " 请以 root 运行：sudo %s\n", argv[0]);
        return 2;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    printf(YELLOW "=== Debian 软件源验证 ===\n");

    if (test_current_source()) {
        printf(GREEN "结论：当前软件源正常，无需操作。\n");
        curl_global_cleanup();
        return 0;
    }

    if (test_mirrors()) {
        printf(GREEN "结论：当前源故障，但存在可用镜像站。"
                      "请手动替换 /etc/apt/sources.list 为可用镜像。\n");
        curl_global_cleanup();
        return 1;
    }

    test_network();
    curl_global_cleanup();
    return 2;
}


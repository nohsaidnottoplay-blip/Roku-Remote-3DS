#include "ecp.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef __3DS__
#include <3ds.h>
#else
#include <time.h>
#endif

#define RESPONSE_LIMIT 32768

static uint64_t now_ms(void) {
#ifdef __3DS__
    return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000);
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
#endif
}

bool ecp_valid_ip(const char *ip) {
    if (!ip || !*ip || strlen(ip) > 15) return false;
    unsigned parts[4];
    const char *p = ip;
    for (unsigned i = 0; i < 4; ++i) {
        if (*p < '0' || *p > '9') return false;
        unsigned n = 0, digits = 0;
        const char *start = p;
        while (*p >= '0' && *p <= '9') {
            n = n * 10 + (unsigned)(*p++ - '0');
            if (++digits > 3 || n > 255) return false;
        }
        if (digits > 1 && *start == '0') return false;
        parts[i] = n;
        if (i < 3 && *p++ != '.') return false;
    }
    return !*p && parts[0] > 0 && parts[0] < 224;
}

static bool valid_key(const char *key) {
    const char *keys[] = {"Home", "Back", "Up", "Down", "Left", "Right",
        "Select", "Play", "Rev", "Fwd", "Info", "InstantReplay",
        "VolumeUp", "VolumeDown", "VolumeMute"};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        if (!strcmp(key, keys[i])) return true;
    return false;
}

/* One deadline covers connect, send, and receive. */
static int wait_socket(int fd, bool writing, uint64_t deadline) {
    for (;;) {
        uint64_t now = now_ms();
        if (now >= deadline) return 0;
        unsigned remaining = (unsigned)(deadline - now);
        struct timeval tv = {(long)(remaining / 1000), (long)(remaining % 1000) * 1000};
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        int n = select(fd + 1, writing ? NULL : &set,
                       writing ? &set : NULL, NULL, &tv);
        if (n < 0 && errno == EINTR) continue;
        return n;
    }
}

static EcpReply reply(EcpResult result, int status, const char *message) {
    EcpReply r;
    r.result = result;
    r.http_status = status;
    snprintf(r.message, sizeof(r.message), "%s", message);
    return r;
}

static bool same_ascii(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

static bool chunked_reply(const char *response, const char *headers_end) {
    const char *line = strstr(response, "\r\n");
    if (!line) return false;
    line += 2;
    while (line < headers_end) {
        const char *end = strstr(line, "\r\n");
        if (!end) return false;
        if (end - line >= 18 && same_ascii(line, "Transfer-Encoding:", 18)) {
            for (const char *p = line + 18; p + 7 <= end; ++p)
                if (same_ascii(p, "chunked", 7)) return true;
        }
        line = end + 2;
    }
    return false;
}

/* Decode available chunk bytes; the last chunk may still be arriving. */
static size_t decode_chunks(char *dest, const char *src, size_t size) {
    size_t in = 0, out = 0;
    while (in < size) {
        size_t end = in;
        while (end + 1 < size && !(src[end] == '\r' && src[end + 1] == '\n')) ++end;
        if (end + 1 >= size) break;
        size_t amount = 0, digits = 0;
        for (size_t i = in; i < end && src[i] != ';'; ++i) {
            unsigned char ch = (unsigned char)src[i];
            if (!isxdigit(ch) || ++digits > 8) goto complete;
            unsigned value = isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
            amount = amount * 16 + value;
            if (amount > RESPONSE_LIMIT) goto complete;
        }
        if (!digits || !amount) break;
        in = end + 2;
        size_t available = size - in;
        size_t copy = amount < available ? amount : available;
        memcpy(dest + out, src + in, copy);
        out += copy;
        in += copy;
        if (copy < amount || size - in < 2) break;
        if (src[in] != '\r' || src[in + 1] != '\n') break;
        in += 2;
    }
complete:
    dest[out] = 0;
    return out;
}

EcpReply ecp_request(const char *ip, const char *key, unsigned timeout_ms) {
    if (!ecp_valid_ip(ip)) return reply(ECP_BAD_IP, 0, "Enter a valid Roku IP address.");
    if (key && !valid_key(key)) return reply(ECP_BAD_KEY, 0, "Unknown remote button.");
    if (!timeout_ms || timeout_ms > 10000) timeout_ms = 2200;
    uint64_t deadline = now_ms() + timeout_ms;
    EcpReply out = reply(ECP_NETWORK, 0, "Cannot reach Roku. Check its IP and Wi-Fi.");
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return out;
    char *response = NULL, *decoded = NULL;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) goto done;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8060);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) goto done;
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS && errno != EWOULDBLOCK && errno != EALREADY) goto done;
        int ready = wait_socket(fd, true, deadline);
        if (!ready) goto timed_out;
        if (ready < 0) goto done;
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error) goto done;
    }

    char request[384];
    int request_len = snprintf(request, sizeof(request),
        "%s /%s%s HTTP/1.1\r\nHost: %s:8060\r\n"
        "User-Agent: RokuRemote3DS/1.0\r\nAccept: application/xml\r\n"
        "Connection: close\r\nContent-Length: 0\r\n\r\n",
        key ? "POST" : "GET", key ? "keypress/" : "query/device-info", key ? key : "", ip);
    if (request_len < 0 || (size_t)request_len >= sizeof(request)) goto done;
    size_t sent = 0;
    while (sent < (size_t)request_len) {
        int ready = wait_socket(fd, true, deadline);
        if (!ready) goto timed_out;
        if (ready < 0) goto done;
#ifdef MSG_NOSIGNAL
        int n = send(fd, request + sent, (size_t)request_len - sent, MSG_NOSIGNAL);
#else
        int n = send(fd, request + sent, (size_t)request_len - sent, 0);
#endif
        if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)) continue;
        if (n <= 0) goto done;
        sent += (size_t)n;
    }

    response = malloc(RESPONSE_LIMIT + 1);
    if (!response) {
        out = reply(ECP_NETWORK, 0, "Not enough memory for the Roku response.");
        goto done;
    }
    size_t used = 0;
    int status = 0;
    bool headers_read = false;
    for (;;) {
        int ready = wait_socket(fd, false, deadline);
        if (!ready) goto timed_out;
        if (ready < 0) goto done;
        int n = recv(fd, response + used, RESPONSE_LIMIT - used, 0);
        if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)) continue;
        if (n < 0) goto done;
        if (!n) break;
        used += (size_t)n;
        response[used] = 0;
        char *body = strstr(response, "\r\n\r\n");
        if (body && !headers_read) {
            if ((strncmp(response, "HTTP/1.1 ", 9) && strncmp(response, "HTTP/1.0 ", 9)) ||
                response[9] < '1' || response[9] > '5' ||
                response[10] < '0' || response[10] > '9' ||
                response[11] < '0' || response[11] > '9' ||
                (response[12] != ' ' && response[12] != '\r')) break;
            status = (response[9] - '0') * 100 + (response[10] - '0') * 10 + response[11] - '0';
            headers_read = true;
            if (status == 401 || status == 403) {
                out = reply(ECP_DENIED, status, "Roku denied control. Check Control by mobile apps.");
                goto done;
            }
            if ((key && status != 200 && status != 204) || (!key && status != 200)) {
                out = reply(ECP_HTTP_ERROR, status, "Roku rejected this command.");
                snprintf(out.message, sizeof(out.message), "Roku returned HTTP %d. This control may be unsupported.", status);
                goto done;
            }
            if (key) {
                out = reply(ECP_OK, status, "Command accepted.");
                goto done;
            }
        }
        if (headers_read && body) {
            const char *xml = body + 4;
            if (chunked_reply(response, body)) {
                if (!decoded) decoded = malloc(RESPONSE_LIMIT + 1);
                if (!decoded) {
                    out = reply(ECP_NETWORK, status, "Not enough memory for the Roku response.");
                    goto done;
                }
                decode_chunks(decoded, xml, used - (size_t)(xml - response));
                xml = decoded;
            }
            const char *root = strstr(xml, "<device-info");
            if (root && (root[12] == '>' || root[12] == ' ' || root[12] == '\r' || root[12] == '\n')) {
                out = reply(ECP_OK, status, "Roku found. Try a remote button.");
                goto done;
            }
        }
        if (used == RESPONSE_LIMIT) break;
    }
    out = reply(ECP_PROTOCOL, status, "Unexpected response. Check that this is your Roku IP.");
    goto done;
timed_out:
    out = reply(ECP_TIMEOUT, 0, "No reply from Roku. Check its IP and Wi-Fi.");
done:
    free(decoded);
    free(response);
    close(fd);
    return out;
}

#ifndef ROKU_ECP_H
#define ROKU_ECP_H
#include <stdbool.h>

typedef enum {
    ECP_OK, ECP_BAD_IP, ECP_NETWORK, ECP_TIMEOUT, ECP_PROTOCOL,
    ECP_DENIED, ECP_HTTP_ERROR, ECP_BAD_KEY
} EcpResult;

typedef struct {
    EcpResult result;
    int http_status;
    char message[128];
} EcpReply;

bool ecp_valid_ip(const char *ip);
/* key == NULL performs a read-only Roku device check. No automatic retries. */
EcpReply ecp_request(const char *ip, const char *key, unsigned timeout_ms);
#endif

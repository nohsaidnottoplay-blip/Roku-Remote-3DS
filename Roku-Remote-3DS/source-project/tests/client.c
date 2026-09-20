#include "ecp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc < 2) return 99;
    if (!strcmp(argv[1], "validate")) {
        for (int i = 2; i < argc; ++i) printf("%d\n", ecp_valid_ip(argv[i]));
        return 0;
    }
    const char *key = argc > 2 && strcmp(argv[2], "check") ? argv[2] : NULL;
    EcpReply r = ecp_request(argv[1], key, argc > 3 ? (unsigned)atoi(argv[3]) : 500);
    printf("%d %d %s\n", r.result, r.http_status, r.message);
    return r.result;
}

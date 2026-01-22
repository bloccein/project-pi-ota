#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_compat.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <zephyr/shell/shell.h>

/* ---- Configure these ---- */
#define SERVER_IP   "192.168.1.145"
#define SERVER_PORT 9997
#define ENDPOINT_PORT 6500

#define DEVICE_ID "node03"

#define LOG_LEVEL LOG_LEVEL_DBG
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(smp_sample);

#include "dfu_hooks.h"

static int get_hash(const char *ep_ip, uint16_t ep_port)
{
    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        LOG_ERR("socket() failed: %d", errno);
        return -errno;
    }

    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(ep_port);

    int rc = zsock_inet_pton(AF_INET, ep_ip, &dst.sin_addr);
    if (rc != 1)
    {
        LOG_ERR("inet_pton() failed for %s", ep_ip);
        zsock_close(sock);
        return -EINVAL;
    }

    char *msg = "get_hash";

    rc = zsock_sendto(sock, msg, strlen(msg), 0,
                      (struct sockaddr *)&dst, sizeof(dst));
    if (rc < 0)
    {
        rc = -errno;
        LOG_ERR("sendto() failed: %d", rc);
        goto fin;
    }

    rc = zsock_recv(sock, good_hash, sizeof(good_hash), MSG_WAITALL);
    if (rc < 0)
    {
        rc = -errno;
        LOG_ERR("Err receiving hash: %d", rc);
    }
    else if (rc != sizeof(good_hash))
    {
        LOG_ERR("Received %d/32 bytes (hash req)", rc);
        rc = -EINVAL;
    }
    else
    {
        rc = 0;
        char sha_hex[(IMG_MGMT_DATA_SHA_LEN * 2) + 1];
        bin2hex(good_hash, sizeof(good_hash), sha_hex, sizeof(sha_hex));
        LOG_WRN("got sha256=%s", sha_hex);
    }

fin:
    zsock_close(sock);
    return rc;
}

static int send_ready_ping(const char *server_ip, uint16_t server_port)
{
    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        LOG_ERR("socket() failed: %d", errno);
        return -errno;
    }

    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(server_port);

    int rc = zsock_inet_pton(AF_INET, server_ip, &dst.sin_addr);
    if (rc != 1)
    {
        LOG_ERR("inet_pton() failed for %s", server_ip);
        zsock_close(sock);
        return -EINVAL;
    }

    /* Minimal JSON payload */
    char msg[192];
    int len = snprintk(msg, sizeof(msg),
                       "{\"kind\":\"ping\",\"device_id\":\"%s\",\"version\":\"%s\",\"smp_port\":1337}",
                       DEVICE_ID, APP_VERSION_STRING);

    if (len <= 0 || len >= sizeof(msg))
    {
        LOG_ERR("message formatting failed");
        zsock_close(sock);
        return -EINVAL;
    }

    rc = zsock_sendto(sock, msg, len, 0,
                      (struct sockaddr *)&dst, sizeof(dst));
    if (rc < 0)
    {
        LOG_ERR("sendto() failed: %d", errno);
        zsock_close(sock);
        return -errno;
    }

    LOG_INF("Sent ready ping to %s:%d (%d bytes)", server_ip, server_port, rc);
    zsock_close(sock);
    return 0;
}

static int cmd_fwup(const struct shell *shell, size_t argc, char **argv)
{
    const char *server_ip = SERVER_IP;
    const char *endpoint_ip = SERVER_IP;
    uint16_t server_port = SERVER_PORT;
    uint16_t endpoint_port = ENDPOINT_PORT;

    if (argc > 1)
    {
        server_ip = argv[1];
    }

    if (argc > 2)
    {
        endpoint_ip = argv[2];
    }

    int rc = get_hash(endpoint_ip, endpoint_port);
    if (rc != 0)
    {
        shell_error(shell, "Failed to get hash: %d", rc);
        return rc;
    }

    rc = send_ready_ping(server_ip, server_port);
    if (rc != 0)
    {
        shell_error(shell, "Ready ping failed: %d", rc);
        return rc;
    }

    shell_print(shell, "Ready ping sent to %s:%u", server_ip, server_port);
    return 0;
}

SHELL_CMD_REGISTER(fwup, NULL, "Readiness helpers", cmd_fwup);

int main(void)
{

    /* Register the built-in mcumgr command handlers. */
    dfu_hooks_register();

    /* using __TIME__ ensure that a new binary will be built on every
     * compile which is convenient when testing firmware upgrade.
     */
    LOG_ERR("Build Time: " __DATE__ " " __TIME__);

    /* The system work queue handles all incoming mcumgr requests.  Let the
     * main thread idle while the mcumgr server runs.
     */
    // for (int i = 1; i < 30; i++) {
    // for (int i = 1; true; i++) {
    // 	k_sleep(K_MSEC(10000));
    // 	send_ready_ping(SERVER_IP, SERVER_PORT);
    // }

    return 0;
}

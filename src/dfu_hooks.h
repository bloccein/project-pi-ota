#ifndef DFU_LOGGING_H_
#define DFU_LOGGING_H_

#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt_client.h>

void dfu_hooks_register(void);

extern uint8_t good_hash[IMG_MGMT_DATA_SHA_LEN];

#endif

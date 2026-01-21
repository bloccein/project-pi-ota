#ifndef DFU_LOGGING_H_
#define DFU_LOGGING_H_

#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>

void dfu_hooks_register(void);

void dfu_hook_image_update_completed(void);
void dfu_hook_image_confirmed(unsigned int image);

extern uint8_t good_hash[IMG_MGMT_DATA_SHA_LEN];

#endif

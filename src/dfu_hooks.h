#ifndef DFU_LOGGING_H_
#define DFU_LOGGING_H_

void dfu_hooks_register(void);

void dfu_hook_image_update_completed(void);
void dfu_hook_image_confirmed(unsigned int image);

#endif

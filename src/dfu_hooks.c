/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dfu_hooks.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dfu_hooks, LOG_LEVEL_INF);

#include <errno.h>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

uint8_t good_hash[IMG_MGMT_DATA_SHA_LEN];

static int find_tlv_area(const struct flash_area *fa, size_t start_off, uint16_t magic,
						 size_t *tlvs_start, size_t *tlvs_end)
{
	struct image_tlv_info tlv_info;
	int rc = flash_area_read(fa, start_off, &tlv_info, sizeof(tlv_info));
	if (rc != 0)
	{
		return rc;
	}

	if (tlv_info.it_magic != magic)
	{
		return -ENOENT;
	}

	*tlvs_start = start_off + sizeof(tlv_info);
	*tlvs_end = start_off + tlv_info.it_tlv_tot;
	return 0;
}

static int read_image_sha256_from_area(uint8_t area_id, uint8_t out_sha[IMG_MGMT_DATA_SHA_LEN])
{
	const struct flash_area *fa = NULL;
	int rc = flash_area_open(area_id, &fa);
	if (rc != 0)
	{
		return rc;
	}

	const size_t start_off = boot_get_image_start_offset(area_id);
	struct image_header hdr;

	rc = flash_area_read(fa, start_off, &hdr, sizeof(hdr));
	if (rc != 0)
	{
		goto out;
	}

	if (hdr.ih_magic != IMAGE_MAGIC)
	{
		rc = -EINVAL;
		goto out;
	}

	size_t tlv_info_off = start_off + hdr.ih_hdr_size + hdr.ih_img_size;
	size_t tlvs_start;
	size_t tlvs_end;

	if (find_tlv_area(fa, tlv_info_off, IMAGE_TLV_PROT_INFO_MAGIC, &tlvs_start, &tlvs_end) == 0)
	{
		tlv_info_off = tlvs_end;
	}

	rc = find_tlv_area(fa, tlv_info_off, IMAGE_TLV_INFO_MAGIC, &tlvs_start, &tlvs_end);
	if (rc != 0)
	{
		goto out;
	}

	for (size_t off = tlvs_start; off + sizeof(struct image_tlv) <= tlvs_end;)
	{
		struct image_tlv tlv;
		rc = flash_area_read(fa, off, &tlv, sizeof(tlv));
		if (rc != 0)
		{
			goto out;
		}

		if (tlv.it_type == IMAGE_TLV_SHA256 && tlv.it_len == IMG_MGMT_DATA_SHA_LEN)
		{
			const size_t val_off = off + sizeof(tlv);
			if (val_off + tlv.it_len > tlvs_end)
			{
				rc = -EINVAL;
				goto out;
			}

			rc = flash_area_read(fa, val_off, out_sha, tlv.it_len);
			goto out;
		}

		off += sizeof(tlv) + tlv.it_len;
	}

	rc = -ENOENT;
out:
	flash_area_close(fa);
	return rc;
}

void dfu_hooks_register(void)
{
#if defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)
	mgmt_callback_register(&dfu_cb);
#else
	LOG_WRN("DFU hooks disabled: enable CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS");
#endif
}

static bool dfu_hook_image_update_completed(void)
{
	uint8_t sha[IMG_MGMT_DATA_SHA_LEN];
	char sha_hex[(IMG_MGMT_DATA_SHA_LEN * 2) + 1];

	const int area_id = g_img_mgmt_state.area_id;
	if (area_id < 0 || area_id > UINT8_MAX)
	{
		LOG_INF("Image upload completed (unknown flash area)");
		return;
	}

	int rc = read_image_sha256_from_area((uint8_t)area_id, sha);
	if (rc != 0)
	{
		LOG_INF("Image upload completed (sha256 unavailable: %d)", rc);
		return;
	}

	bin2hex(sha, sizeof(sha), sha_hex, sizeof(sha_hex));
	LOG_WRN("Image upload completed (sha256=%s)", sha_hex);

	return memcmp(sha, good_hash, sizeof(sha)) == 0;
}

static void dfu_hook_image_confirmed(unsigned int image)
{
	LOG_INF("Image %u confirmed", image);
}

#if defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>

static enum mgmt_cb_return dfu_mgmt_event_cb(uint32_t event, enum mgmt_cb_return prev_status,
											 int32_t *rc, uint16_t *group, bool *abort_more,
											 void *data, size_t data_size)
{
	ARG_UNUSED(prev_status);
	ARG_UNUSED(rc);
	ARG_UNUSED(group);
	ARG_UNUSED(abort_more);
	ARG_UNUSED(data_size);

	switch (event)
	{
	case MGMT_EVT_OP_IMG_MGMT_DFU_PENDING:
		bool ok = dfu_hook_image_update_completed(MGMT_CB_ERROR_ERR);
		if (!ok)
		{
			LOG_ERR("BAD HASH! ABANDONING!");

			struct img_mgmt_client img_client;
			img_mgmt_client_erase(&img_client, 1); // todo
		}
		break;
	case MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED:
		if ((data != NULL) && (data_size == sizeof(struct img_mgmt_image_confirmed)))
		{
			const struct img_mgmt_image_confirmed *confirmed = data;
			dfu_hook_image_confirmed(confirmed->image);
		}
		else
		{
			dfu_hook_image_confirmed(0);
		}
		break;
	default:
		break;
	}

	return MGMT_CB_OK;
}

static struct mgmt_callback dfu_cb = {
	.callback = dfu_mgmt_event_cb,
	.event_id = MGMT_EVT_OP_IMG_MGMT_DFU_PENDING | MGMT_EVT_OP_IMG_MGMT_DFU_CONFIRMED,
};
#endif

#include "bsp/board.h"
#include "tusb.h"
#include "config.h"
#include "sd_config.h"

#define VID "PicoMC"
#define PID "Mass Storage"
#define REV "1.0"


/* invoked when received SCSI_CMD_INQUIRY */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return;

	const char vid[] = VID;
	const char pid[] = PID;
	const char rev[] = REV;

	memcpy(vendor_id  , vid, strlen(vid));
	memcpy(product_id , pid, strlen(pid));
	memcpy(product_rev, rev, strlen(rev));
}

/* invoked when received Test Unit Ready command */
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return false;

	if(p_sd->m_Status != 0) {
		// Additional Sense 3A-00 is NOT_FOUND
		tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
		return false;
	}

	return true;
}

/* invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size */
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return;

	*block_count = (uint32_t) p_sd->sectors;
	*block_size  = (uint16_t) BLOCK_SIZE;
}

/*
	invoked when received Start Stop Unit command
	- Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
	- Start = 1 : active mode, if load_eject = 1 : load disk storage
*/
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
	(void) power_condition;

	if ( load_eject )
	{
		if(start) {
			return true;
		}
	}

	return true;
}

/* callback invoked when received READ10 command */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return -1;							// not valid drive

	if(lba < 0 || lba >= p_sd->sectors) return -1;	// invalid sector
	if(bufsize != BLOCK_SIZE) return -1;			// invalid transfer unit
	if(offset != 0) return -1;						// cannot read unaligned sectors

	int status = sd_read_blocks(p_sd, (uint8_t*) buffer, (uint64_t) lba, 1);
	if(status != SD_BLOCK_DEVICE_ERROR_NONE) return -1;		// read failed

	return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun)
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return false;
	return true;
}

/* callback invoked when received WRITE10 command */
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
	sd_card_t* p_sd = sd_get_by_num(lun);
	if (!p_sd) return -1;							// not valid drive

	if(lba < 0 || lba >= p_sd->sectors) return -1;	// invalid sector
	if(bufsize != BLOCK_SIZE) return -1;			// invalid transfer unit
	if(offset != 0) return -1;						// writes must be sector aligned

	int status = sd_write_blocks(p_sd, buffer, lba, 1);
	if(status != SD_BLOCK_DEVICE_ERROR_NONE) return -1;		// write failed

	return (int32_t) bufsize;
}

/*
	callback invoked when received an SCSI command not in built-in list below
	- READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
	- READ10 and WRITE10 has their own callbacks
*/
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
	void const* response = NULL;
	int32_t resplen = 0;

	// most scsi handled is input
	bool in_xfer = true;

	switch(scsi_cmd[0]) {
		case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
			resplen = 0;
			break;
		default:
			// Set Sense = Invalid Command Operation
			tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

			// negative means error -> tinyusb could stall and/or response with failed status
			resplen = -1;
			break;
	}

	// return resplen must not larger than bufsize
	if(resplen > bufsize) resplen = bufsize;

	if(response && (resplen > 0)) {
		if(in_xfer) {
			memcpy(buffer, response, (size_t) resplen);
		} else {
			// SCSI output
		}
	}

	return (int32_t) resplen;
}
#include "bsp/board.h"
#include "tusb.h"
#include "ram_disk.h"
#include "pico/time.h"
#include "config.h"

#define VID "PicoMC"
#define PID "Mass Storage"
#define REV "1.0"

alarm_id_t alarm_id = -2;	// -1 used to indicate error, -2 indicates uninitialized

/* invoked when received SCSI_CMD_INQUIRY */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
	(void) lun;

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
	(void) lun;

	if(RAM_disk_status() != 0) {
		// Additional Sense 3A-00 is NOT_FOUND
		tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
		return false;
	}
	return true;
}

/* invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size */
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
	(void) lun;

	*block_count = DISK_BLOCK_NUM;
	*block_size  = DISK_BLOCK_SIZE;
}

/*
	invoked when received Start Stop Unit command
	- Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
	- Start = 1 : active mode, if load_eject = 1 : load disk storage
*/
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
	(void) lun;
	(void) power_condition;

	if ( load_eject )
	{
		if(start) {
			RAM_disk_initialize();
		} else {
			RAM_disk_export_lfs_memcard();
			RAM_disk_deinitialize();
		}
	}

	return true;
}

/* callback invoked when received READ10 command */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  if(lba < 0 || lba >= DISK_BLOCK_NUM) return -1;	// invalid sector
  if(bufsize != DISK_BLOCK_SIZE) return -1;			// invalid transfer unit
  if(offset != 0) return -1;						// cannot read outside sector boundries
  uint32_t status = RAM_disk_read(buffer, lba, 1);
  if(status != RES_OK) return -1;					// read failed
  return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun)
{
  (void) lun;
  return true;
}

int64_t sync_callback(alarm_id_t id, void *user_data) {
	RAM_disk_export_lfs_memcard();
}

/* callback invoked when received WRITE10 command */
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
	(void) lun;
	if(lba < 0 || lba >= DISK_BLOCK_NUM) return -1;	// invalid sector
	if(bufsize != DISK_BLOCK_SIZE) return -1;		// invalid transfer unit
	if(offset != 0) return -1;						// writes must be sector aligned
	if(alarm_id >= 0)
		if(cancel_alarm(alarm_id))
			alarm_id = -2;
	uint32_t status = RAM_disk_write(buffer, lba, 1);
	alarm_id = add_alarm_in_ms(MSC_WRITE_SYNC_TIMEOUT, sync_callback, NULL, false);
	if(status != RES_OK) return -1;					// write failed
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
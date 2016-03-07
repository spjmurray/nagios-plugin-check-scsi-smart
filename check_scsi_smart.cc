/*
 * SMART Nagios/Icinga Disk Check
 * ------------------------------
 *
 * License
 * -------
 * (C) 2015-2016 Simon Murray <spjmurray@yahoo.co.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Description
 * -----------
 * Checks ATA devices for failures via SMART disk checks.  Unlike the old
 * and flawed check_ide_smart this check uses the SCSI protocol to access
 * drives.  This allows the SCSI command to be translated by the relevant
 * SAT in the IO chain, be it linux's libata for SATA controllers, an HBA
 * for direct attached SAS controllers or SAS expander.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include <iostream>
#include <memory>
#include <sstream>

using namespace std;

#define BINARY "check_scsi_smart"
#define VERSION "1.1.0"

#define MAX(a,b) ((a)>(b)?(a):(b))

/* Nagios return codes */
#define NAGIOS_OK       0
#define NAGIOS_WARNING  1
#define NAGIOS_CRITICAL 2
#define NAGIOS_UNKNOWN  3

/* SCSI primary commands */
#define SBC_ATA_PASS_THROUGH 0x85

/* ATA commands */
#define ATA_IDENTIFY_DEVICE 0xec
#define ATA_SMART           0xb0

/* ATA protocols */
#define ATA_PROTOCOL_PIO_DATA_IN 0x4

/* ATA Log Addresses */
#define ATA_LOG_ADDRESS_DIRECTORY 0x0
#define ATA_LOG_ADDRESS_SMART     0x1

/* SMART functions */
#define SMART_READ_DATA       0xd0
#define SMART_READ_THRESHOLDS 0xd1
#define SMART_READ_LOG        0xd5
#define SMART_RETURN_STATUS   0xda

/* SMART off-line status */
#define SMART_OFF_LINE_STATUS_NEVER_STARTED  0x00
#define SMART_OFF_LINE_STATUS_COMPLETED      0x02
#define SMART_OFF_LINE_STATUS_IN_PROGRESS    0x03
#define SMART_OFF_LINE_STATUS_SUSPENDED      0x04
#define SMART_OFF_LINE_STATUS_ABORTED_HOST   0x05
#define SMART_OFF_LINE_STATUS_ABORTED_DEVICE 0x06

#define SECTOR_SIZE 512
#define SMART_ATTRIBUTE_NUM 30

/*
 * Struct: sbc_ata_pass_through
 * ----------------------------
 * SCSI CDB for tunneling ATA commands over the SCSI command protocol
 * to a SAT which then translates to a native ATA command to the actual
 * device.  May be handled by Linux for directly attached devices or
 * via a SAS HBA/expander.
 */
typedef struct {
  uint8_t operation_code;
  uint8_t extend: 1;
  uint8_t protocol: 4;
  uint8_t multiple_count: 3;
  uint8_t t_length: 2;
  uint8_t byte_block: 1;
  uint8_t t_dir: 1;
  uint8_t t_type: 1;
  uint8_t ck_cond: 1;
  uint8_t off_line: 2;
  uint8_t features_15_8;
  uint8_t features_7_0;
  uint8_t count_15_8;
  uint8_t count_7_0;
  uint8_t lba_31_24;
  uint8_t lba_7_0;
  uint8_t lba_39_32;
  uint8_t lba_15_8;
  uint8_t lba_47_40;
  uint8_t lba_23_16;
  uint8_t device;
  uint8_t command;
  uint8_t control;
} sbc_ata_pass_through;

/*
 * Struct: ata_log_directory
 * -------------------------
 * Structure defining the directory version and number of logs available
 * for each address.  Index 0 is the version, this is kept as an array to
 * enable resue of ATA_LOG_ADDRESS_* macros.
 */
typedef struct {
  uint16_t data_blocks[256];
} ata_log_directory;

/*
 * Struct: smart_attribute
 * -----------------------
 * Vendor specific SMART attribute as returned by a SMART READ DATA ATA
 * command.
 */
typedef struct __attribute__((packed)) {
  uint8_t  id;
  uint16_t flags;
  uint8_t  value;
  uint8_t  worst;
  uint32_t raw_lo;
  uint16_t raw_hi;
  uint8_t  pad;
} smart_attribute;

/*
 * Struct: smart_threshold
 * -----------------------
 * Vendor specific SMART threshold as returned by a SMART READ THRESHOLDS
 * ATA command.  This is now obsolete, and should be rolled up by the device
 * into and LBA field which can be attained via the SMART RETURN STATUS
 * command
 */
typedef struct __attribute__((packed)) {
  uint8_t id;
  uint8_t threshold;
  uint8_t pad[10];
} smart_threshold;

/*
 * Struct: smart_data
 * ------------------
 * Standardized ATA SMART data returned by SMART READ DATA
 */
typedef struct __attribute__((packed)) {
  uint16_t        version;
  smart_attribute attributes[SMART_ATTRIBUTE_NUM];
  uint8_t         offline_data_collection_status;
  uint8_t         self_test_execution_status;
  uint16_t        offline_collection_time;
  uint8_t         vendor_specific1;
  uint8_t         offline_collection_capability;
  uint16_t        smart_capability;
  uint8_t         error_logging_capbility;
  uint8_t         vendor_specific2;
  uint8_t         short_self_test_polling_time;
  uint8_t         extended_self_test_polling_time;
  uint8_t         conveyance_self_test_polling_time;
  uint16_t        extended_self_test_routine_polling_time;
  uint8_t         reserved[9];
  uint8_t         vendor_specific3[125];
  uint8_t         checksum;
} smart_data;

/*
 * Struct: smart_thresholds
 * ------------------------
 * Standardized ATA SMART threshold data returned by SMART READ THRESHOLDS
 */
typedef struct __attribute__((packed)) {
  uint16_t        version;
  smart_threshold thresholds[SMART_ATTRIBUTE_NUM];
  uint8_t         reserved[149];
  uint8_t         checksum;
} smart_thresholds;

/*
 * Struct: smart_log_command
 * -------------------------
 * SMART log command
 */
typedef struct __attribute__((packed)) {
  uint8_t command;
  uint8_t feature;
  uint32_t lba: 24;
  uint32_t count: 8;
  uint8_t device;
  uint8_t init;
  uint32_t timestamp;
} smart_log_command;

/*
 * Struct: smart_log_error
 * -----------------------
 * SMART log error structure defining LBA, device, status, timestamp etc.
 */
typedef struct __attribute__((packed)) {
  uint8_t reserved;
  uint8_t error;
  uint32_t lba: 24;
  uint32_t count: 8;
  uint8_t device;
  uint8_t status;
  uint8_t extended[19];
  uint8_t state;
  uint16_t timestamp;
} smart_log_error;

/*
 * Struct: smart_log_data
 * ----------------------
 * Sructure to hold an error and the preceding commands leading up to it
 */
typedef struct __attribute__((packed)) {
  smart_log_command command[5];
  smart_log_error error;
} smart_log_data;

/* Struct: smart_log_summary
 * -------------------------
 * Top level log summary containing upto 5 errors
 */
typedef struct __attribute__((packed)) {
  uint8_t version;
  uint8_t index;
  smart_log_data data[5];
  uint16_t count;
  uint8_t reserved[57];
  uint8_t checksum;
} smart_log_summary;

/*
 * Function: version
 * -----------------
 * Print out the version string
 */
void version() {
  printf(BINARY " v" VERSION "\n");
}

/*
 * Function: usage
 * ---------------
 * Print out the usage syntax
 */
void usage() {

  printf("Usage:\n");
  printf(BINARY " [-d <device>]\n");

}

/*
 * Function: help
 * --------------
 * Print out the verbose help screen
 */
void help() {

  version();
  printf("(C) 2015 Simon Murray <spjmurray@yahoo.co.uk>\n");
  printf("\n");
  usage();
  printf("\n");
  printf("Options:\n");
  printf("-h, --help\n");
  printf("   Print detailed help\n");
  printf("-v, --version\n");
  printf("   Print version information\n");
  printf("-d, --device=DEVICE\n");
  printf("   Select device DEVICE\n");
  printf("\n");

}

/*
 * Function: smart_id_to_text
 * --------------------------
 * Converts a SMART attribute ID to ascii text
 *
 * id: SMART attribute identifier
 *
 * returns: Text representation for the SMART attribute identifier if valid
 *          or "Unknown Attribute" otherwise
 */
void dump_smart_attribute_id(ostream& o, uint8_t id) {

  static struct attribute_meta {
    unsigned char id;
    const char* name;
  } attribute_meta[] = {
    { 0x01, "read_error_rate" },
    { 0x02, "throughput_performance" },
    { 0x03, "spin_up_time" },
    { 0x04, "start_stop_count" },
    { 0x05, "reallocated_sectors_count" },
    { 0x06, "read_channel_margin" },
    { 0x07, "seek_error_rate" },
    { 0x08, "seek_time_performance" },
    { 0x09, "power_on_hours" },
    { 0x0a, "spin_retry_count" },
    { 0x0b, "recalibration_retries" },
    { 0x0c, "power_cycle_count" },
    { 0x0d, "soft_read_error_rate" },
    { 0x16, "current_helium_level" },
    { 0xaa, "available_reserved_space" },
    { 0xab, "ssd_program_fail_count" },
    { 0xac, "ssd_erase_fail_count" },
    { 0xad, "ssd_wear_leveling_count" },
    { 0xae, "unexpected_power_loss_count" },
    { 0xaf, "power_loss_protection_failure" },
    { 0xb0, "erase_fail_count" },
    { 0xb1, "wear_range_delta" },
    { 0xb3, "used_reserved_block_count_total" },
    { 0xb4, "unused_reserved_block_count_total" },
    { 0xb5, "program_fail_count_total" },
    { 0xb6, "erase_fail_count" },
    { 0xb7, "sata_downshift_error_count" },
    { 0xb8, "end_to_end_error" },
    { 0xb9, "head_stability" },
    { 0xba, "induced_op_vibration_detection" },
    { 0xbb, "reported_uncorrectable_errors" },
    { 0xbc, "command_timeout" },
    { 0xbd, "high_fly_writes" },
    { 0xbe, "airflow_temperature" },
    { 0xbf, "g_sense_error_rate" },
    { 0xc0, "power_off_retract_count" },
    { 0xc1, "load_cycle_count" },
    { 0xc2, "temperature" },
    { 0xc3, "hardware_ecc_recovered" },
    { 0xc4, "reallocation_event_count" },
    { 0xc5, "current_pending_sector_count" },
    { 0xc6, "uncorrectable_sector_count" },
    { 0xc7, "ultradma_crc_error_count" },
    { 0xc8, "multi_zone_error_rate" },
    { 0xc9, "soft_read_error_rate" },
    { 0xca, "data_address_mark_errors" },
    { 0xcb, "run_out_cancel" },
    { 0xcc, "soft_ecc_correction" },
    { 0xcd, "thermal_asperity_rate" },
    { 0xce, "flying_height" },
    { 0xcf, "spin_height_current" },
    { 0xd0, "spin_buzz" },
    { 0xd1, "offline_seek_performance" },
    { 0xd2, "vibration_during_write" },
    { 0xd3, "wibration_during_write" },
    { 0xd4, "shock_during_write" },
    { 0xdc, "disk_shift" },
    { 0xdd, "g_sense_error_rate" },
    { 0xde, "loaded_hours" },
    { 0xdf, "load_unload_retry_count" },
    { 0xe0, "load_friction" },
    { 0xe1, "load_unload_cycle_count" },
    { 0xe2, "load_in_time" },
    { 0xe3, "torque_amplification_count" },
    { 0xe4, "power_off_retract_cycle" },
    { 0xe6, "drive_life_protection_status" },
    { 0xe7, "temperature" },
    { 0xe8, "available_reserved_space" },
    { 0xe9, "media_wearout_indicator" },
    { 0xea, "average_erase_count" },
    { 0xeb, "good_block_count" },
    { 0xf0, "flying_head_hours" },
    { 0xf1, "total_lbas_written" },
    { 0xf2, "total_lbas_read" },
    { 0xf3, "total_lbas_written_expanded" },
    { 0xf4, "total_lbas_read_expanded" },
    { 0xf9, "nand_writes_1gib" },
    { 0xfa, "read_error_retry_rate" },
    { 0xfb, "minimum_spares_remaining" },
    { 0xfc, "newly_added_bad_flash_block" },
    { 0xfe, "free_fall_protection" },
    { 0x00, 0 }
  };

  struct attribute_meta* meta = attribute_meta;
  for(; meta->id; meta++) {
    if(meta->id == id) {
      o << (unsigned int)id << "_" << meta->name;
      return;
    }
  }

  o << std::dec << (unsigned int)id << "_unknown";

}

/*
 * Function: sgio
 * --------------
 * Sends a CDB to the target device and recieves a response
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * cmdp: Pointer to a SCSI CDB
 * cmd_len: Length of the CDB
 * dxferp: Pointer to the SCSI data buffer
 * dxfer_len: Length of the SCSI data buffer
 */
void sgio(int fd, unsigned char* cmdp, int cmd_len, unsigned char* dxferp, int dxfer_len) {

  sg_io_hdr_t sgio_hdr;
  unsigned char sense[32];

  memset(&sgio_hdr, 0, sizeof(sg_io_hdr_t));
  sgio_hdr.interface_id = 'S';
  sgio_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
  sgio_hdr.cmd_len = cmd_len;
  sgio_hdr.mx_sb_len = 32;
  sgio_hdr.dxfer_len = dxfer_len;
  sgio_hdr.dxferp = dxferp;
  sgio_hdr.cmdp = cmdp;
  sgio_hdr.sbp = sense;

  if(ioctl(fd, SG_IO, &sgio_hdr) < 0) {
    fprintf(stderr, "UNKNOWN: SG_IO ioctl error\n");
    exit(NAGIOS_UNKNOWN);
  }

}

/*
 * Function: ata_identify
 * ----------------------
 * Send an IDENTIFY command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least 512B
 */
void ata_identify(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;

  memset((unsigned char*)&ata_pass_through, 0, sizeof(sbc_ata_pass_through));
  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = 1; /* transfer from device */
  ata_pass_through.t_length       = 2; /* length encoded in count */
  ata_pass_through.byte_block     = 1; /* length in sectors */
  ata_pass_through.count_7_0      = 1; /* transfer 1 sector */
  ata_pass_through.command        = ATA_IDENTIFY_DEVICE;

  sgio(fd, (unsigned char*)&ata_pass_through, sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_data
 * -----------------------------
 * Send a SMART READ DATA command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least 512B
 */
void ata_smart_read_data(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;

  memset((unsigned char*)&ata_pass_through, 0, sizeof(sbc_ata_pass_through));
  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = 1; /* transfer from device */
  ata_pass_through.t_length       = 2; /* length encoded in count */
  ata_pass_through.byte_block     = 1; /* length in sectors */
  ata_pass_through.count_7_0      = 1; /* transfer 1 sector */
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_DATA;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;

  sgio(fd, (unsigned char*)&ata_pass_through, sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_thresholds
 * -----------------------------------
 * Send a SMART READ THRESHOLDS command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least 512B
 */
void ata_smart_read_thresholds(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;

  memset((unsigned char*)&ata_pass_through, 0, sizeof(sbc_ata_pass_through));
  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = 1; /* transfer from device */
  ata_pass_through.t_length       = 2; /* length encoded in count */
  ata_pass_through.byte_block     = 1; /* length in sectors */
  ata_pass_through.count_7_0      = 1; /* transfer 1 sector */
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_THRESHOLDS;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;

  sgio(fd, (unsigned char*)&ata_pass_through, sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_log
 * ----------------------------
 * Send a SMART READ LOG command to the ATA device and receive the data
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least sectors * 512B
 *      bytes.
 * log: Log to read See A.1 for ATA8-ACS
 */
void ata_smart_read_log(int fd, unsigned char* buf, int log, uint16_t sectors) {

  sbc_ata_pass_through ata_pass_through;

  memset((unsigned char*)&ata_pass_through, 0, sizeof(sbc_ata_pass_through));
  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = 1; /* transfer from device */
  ata_pass_through.t_length       = 2; /* length encoded in count */
  ata_pass_through.byte_block     = 1; /* length in sectors */
  ata_pass_through.count_15_8     = sectors >> 8;
  ata_pass_through.count_7_0      = sectors;
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_LOG;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;
  ata_pass_through.lba_7_0        = log;

  sgio(fd, (unsigned char*)&ata_pass_through, sizeof(ata_pass_through), buf, sectors * SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_log_directory
 * --------------------------------------
 * Reads the SMART flog directory
 * fd: File descriptor pointing at a SCSI or SCSI generic device nod
 * buf: Data buffer to receive the data into, must be at least 512B
 */
void ata_smart_read_log_directory(int fd, unsigned char* buf) {
  ata_smart_read_log(fd, buf, ATA_LOG_ADDRESS_DIRECTORY, 1);
}

/*
 * Function: get_raw
 * -----------------
 * Reads the raw value from a SMART attribute
 * attribute: Pointer to a SMART attribute
 */
uint64_t get_raw(const smart_attribute* attribute) {

  return (((uint64_t)attribute->raw_hi) << 32) | (uint64_t)attribute->raw_lo;

}

/*
 * Function: dump_raw
 * ------------------
 * Apply formatting to and print raw value to a string
 * buf: Output buffer
 * id: SMART attribute ID
 * raw: Raw value
 */
void dump_raw(ostream& o, uint8_t id, uint64_t raw) {

  switch(id) {
    case 3:   // Spin up time
    case 5:   // Reallocated sector count
    case 196: // Reallocated event count
      raw &= 0xffff;
      break;
    case 9:   // Power on hours
    case 240: // Head flying hours
      raw &= 0xffffff;
      break;
    case 190: // Temperature
    case 194: // Temperature
      raw &= 0xff;
      break;
    default:
      break;
  }
  o << raw;

}

/*
 * Function: check_smart_attributes
 * --------------------------------
 * Checks attributes against vendor thresholds
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * rc: Reference to the current return code
 * crit: Reference to a counter of critical attributes
 * warn: Reference to a counter of advisory attributes
 * perfdata: Output stream to dump performance data to
 */
void check_smart_attributes(int fd, int& rc, int& crit, int& warn, ostream& perfdata) {

  // Load the SMART data and thresholds pages
  smart_data sd;
  ata_smart_read_data(fd, (unsigned char*)&sd);

  smart_thresholds st;
  ata_smart_read_thresholds(fd, (unsigned char*)&st);

  // Perform actual SMART threshold checks
  for(int i=0; i<SMART_ATTRIBUTE_NUM; i++) {

    // Attribute is invalid
    if(!sd.attributes[i].id)
      continue;

    if(sd.attributes[i].value > 0x00 &&
       sd.attributes[i].value < 0xfe &&
       sd.attributes[i].value <= st.thresholds[i].threshold) {

      // Predicted failure is within 24 hours, otherwise the device lifespan has been exceeded
      if(sd.attributes[i].flags & 0x1) {
        crit++;
      } else {
        warn++;
      }

    }

    // Accumulate the performance data
    uint8_t id = sd.attributes[i].id;
    perfdata << " ";
    dump_smart_attribute_id(perfdata, id);
    perfdata << "=";
    dump_raw(perfdata, id, get_raw(sd.attributes + i));
  }

  // Determine the state to report
  if(warn) {
    rc = MAX(rc, NAGIOS_WARNING);
  }
  if(crit) {
    rc = MAX(rc, NAGIOS_CRITICAL);
  }

}

/*
 * Function: check_smart_log
 * -------------------------
 * Checks for the existence of SMART logs
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * rc: Reference to the current return code
 * logs: Reference to a count of the number of SMART logs
 */
void check_smart_log(int fd, int& rc, int& logs) {

  // Read the SMART log directory
  ata_log_directory log_directory;
  ata_smart_read_log_directory(fd, (unsigned char*)&log_directory);

  // Calculate the number of SMART log sectors to read and allocate a buffer
  uint16_t smart_log_sectors = log_directory.data_blocks[ATA_LOG_ADDRESS_SMART];
  smart_log_summary* summaries = new smart_log_summary[smart_log_sectors];

  // Read the logs in
  ata_smart_read_log(fd, (unsigned char*)summaries, ATA_LOG_ADDRESS_SMART, smart_log_sectors);

  for(int i=0; i<smart_log_sectors; i++) {
    logs += summaries[i].count;
  }

  delete [] summaries;

  if(logs) {
    rc = MAX(rc, NAGIOS_WARNING);
  }

}


/*
 * Function: main
 * --------------
 * Reads device identity and checks for SMART capability, if so reads
 * the SMART data and thresholds and checks for any predictive failures
 */
int main(int argc, char** argv) {

  int c;

  static struct option long_options[] = {
    { "help",    no_argument,       0, 'h' },
    { "version", no_argument,       0, 'v' },
    { "device",  required_argument, 0, 'd' },
    { 0,         0,                 0, 0   }
  };

  const char* device = 0;

  while((c = getopt_long(argc, argv, "hvd:", long_options, 0)) != -1) {
    switch(c) {
      case 'h':
        help();
        exit(0);
      case 'v':
        version();
        exit(0);
      case 'd':
        device = optarg;
        break;
      default:
        usage();
        exit(1);
        break;
    }
  }

  if(!device) {
    help();
    exit(1);
  }

  int fd = open(device, O_RDWR);
  if(fd == -1) {
    fprintf(stderr, "UNKNOWN: unable to open device %s\n", device);
    exit(NAGIOS_UNKNOWN);
  }

  int sg_version;
  if((ioctl(fd, SG_GET_VERSION_NUM, &sg_version) == -1) || sg_version < 30000) {
    fprintf(stderr, "UNKNOWN: %s is either not an sg device, or the driver is old\n", device);
    exit(NAGIOS_UNKNOWN);
  }

  // Check the device can use SMART and that it is enabled
  uint16_t identify[SECTOR_SIZE / 2];
  ata_identify(fd, (unsigned char*)identify);

  if(~identify[82] & 0x01) {
    printf("OK: SMART feature set unsupported\n");
    exit(NAGIOS_OK);
  }

  if(~identify[85] & 0x01) {
    printf("UNKNOWN: SMART feature set disabled\n");
    exit(NAGIOS_UNKNOWN);
  }

  int rc = NAGIOS_OK;
  int crit = 0;
  int warn = 0;
  int logs = 0;
  stringstream perfdata;

  // Perform the checks
  check_smart_attributes(fd, rc, crit, warn, perfdata);
  check_smart_log(fd, rc, logs);

  // Print out the results and performance data
  const char* statuses[] = { "OK", "WARNING", "CRITICAL" };
  cout << statuses[rc] << ": predicted fails " << crit << ", advisories " << warn
       << ", errors " << logs << " |" << perfdata.str() << endl;

  close(fd);

  return rc;

}

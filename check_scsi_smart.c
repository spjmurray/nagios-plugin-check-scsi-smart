/*
 * SMART Nagios/Icinga Disk Check
 * ------------------------------
 *
 * License
 * -------
 * (C) 2015 Simon Murray <spjmurray@yahoo.co.uk>
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

#define BINARY "check_scsi_smart"
#define VERSION "1.0.0"

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

/* SMART functions */
#define SMART_READ_DATA       0xd0
#define SMART_READ_THRESHOLDS 0xd1
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
  uint32_t raw;
  uint8_t  pad[3];
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
const char* smart_id_to_text(uint8_t id) {

  static struct attribute_meta {
    unsigned char id;
    const char* name;
  } attribute_meta[] = {
    { 0x01, "Read Error Rate" },
    { 0x02, "Throughput Performance" },
    { 0x03, "Spin-Up Time" },
    { 0x04, "Start/Stop Count" },
    { 0x05, "Reallocated Sectors Count" },
    { 0x06, "Read Channel Margin" },
    { 0x07, "Seek Error Rate" },
    { 0x08, "Seek Time Performance" },
    { 0x09, "Power-On Hours" },
    { 0x0a, "Spin Retry Count" },
    { 0x0b, "Recalibration Retries" },
    { 0x0c, "Power Cycle Count" },
    { 0x0d, "Soft Read Error Rate" },
    { 0x16, "Current Helium Level" },
    { 0xaa, "Available Reserved Space" },
    { 0xab, "SSD Program Fail Count" },
    { 0xac, "SSD Erase Fail Count" },
    { 0xad, "SSD Wear Leveling Count" },
    { 0xae, "Unexpected Power Loss Count" },
    { 0xaf, "Power Loss Protection Failure" },
    { 0xb0, "Erase Fail Count" },
    { 0xb1, "Wear Range Delta" },
    { 0xb3, "Used Reserved Block Count Total" },
    { 0xb4, "Unused Reserved Block Count Total" },
    { 0xb5, "Program Fail Count Total" },
    { 0xb6, "Erase Fail Count" },
    { 0xb7, "SATA Downshift Error Count" },
    { 0xb8, "End-to-End Error" },
    { 0xb9, "Head Stability" },
    { 0xba, "Induced Op-Vibration Detection" },
    { 0xbb, "Reported Uncorrectable Errors" },
    { 0xbc, "Command Timeout" },
    { 0xbd, "High Fly Writes" },
    { 0xbe, "Airflow Temperature" },
    { 0xbf, "G-Sense Error Rate" },
    { 0xc0, "Power-Off Retract Count" },
    { 0xc1, "Load Cycle Count" },
    { 0xc2, "Temperature" },
    { 0xc3, "Hardware ECC Recovered" },
    { 0xc4, "Reallocation Event Count" },
    { 0xc5, "Current Pending Sector Count" },
    { 0xc6, "Uncorrectable Sector Count" },
    { 0xc7, "UltraDMA CRC Error Count" },
    { 0xc8, "Multi-Zone Error Rate" },
    { 0xc9, "Soft Read Error Rate" },
    { 0xca, "Data Address Mark Errors" },
    { 0xcb, "Run Out Cancel" },
    { 0xcc, "Soft ECC Correction" },
    { 0xcd, "Thermal Asperity Rate" },
    { 0xce, "Flying Height" },
    { 0xcf, "Spin Height Current" },
    { 0xd0, "Spin Buzz" },
    { 0xd1, "Offline Seek Performance" },
    { 0xd2, "Vibration During Write" },
    { 0xd3, "Vibration During Write" },
    { 0xd4, "Shock During Write" },
    { 0xdc, "Disk Shift" },
    { 0xdd, "G-Sense Error Rate" },
    { 0xde, "Loaded Hours" },
    { 0xdf, "Load/Unload Retry Count" },
    { 0xe0, "Load Friction" },
    { 0xe1, "Load/Unload Cycle Count" },
    { 0xe2, "Load In Time" },
    { 0xe3, "Torque Amplification Attack" },
    { 0xe4, "Power-Off Retract Cycle" },
    { 0xe6, "Drive Life Protection Status" },
    { 0xe7, "Temperature/SSD Life Left" },
    { 0xe8, "Endurance Remaining/Available Reserved Space" },
    { 0xe9, "Power-On Hours/Media Wearout Indicator" },
    { 0xea, "Average Erase Count" },
    { 0xeb, "Good Block Count" },
    { 0xf0, "Flying Head Hours" },
    { 0xf1, "Total LBAs Written" },
    { 0xf2, "Total LBAs Read" },
    { 0xf3, "Total LBAs Written Expanded" },
    { 0xf4, "Total LBAs Read Expanded" },
    { 0xf9, "NAND Writes 1GiB" },
    { 0xfa, "Read Error Retry Rate" },
    { 0xfb, "Minimum Spares Remaining" },
    { 0xfc, "Newly Added Bad Flash Block" },
    { 0xfe, "Free Fall Protection" },
    { 0x00, 0 }
  };

  struct attribute_meta* meta = attribute_meta;
  for(; meta->id; meta++) {
    if(meta->id == id) {
      return meta->name;
    }
  }

  return "Unknown Attribute";

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

  /* Check the device can use SMART and that it is enabled */
  uint16_t identify[SECTOR_SIZE / 2];
  ata_identify(fd, (unsigned char*)identify);

  if(~identify[82] & 0x01) {
    printf("OK: SMART feature set unsupported\n");
    exit(NAGIOS_OK);
  }

  if(~identify[85] & 0x01) {
    printf("WARNING: SMART feature set disabled\n");
    exit(NAGIOS_WARNING);
  }

  /* Check that offline collection is working as designed */
  smart_data sd;
  ata_smart_read_data(fd, (unsigned char*)&sd);

  if((sd.offline_data_collection_status & 0x7f) == SMART_OFF_LINE_STATUS_NEVER_STARTED) {
    printf("Off-line data collection: never started\n\n");
  } else if((sd.offline_data_collection_status & 0x7f) == SMART_OFF_LINE_STATUS_SUSPENDED) {
    printf("Off-line data collection: suspended\n\n");
  } else if((sd.offline_data_collection_status & 0x7f) == SMART_OFF_LINE_STATUS_ABORTED_HOST) {
    printf("Off-line data collection: aborted by host\n\n");
  } else if((sd.offline_data_collection_status & 0x7f) == SMART_OFF_LINE_STATUS_ABORTED_DEVICE) {
    printf("Off-line data collection: aborted by device\n\n");
  } else {
    printf("Off-line data collection: unknown state\n\n");
  }

  /* Perform actual SMART threshold checks */
  int rc = NAGIOS_OK;

  smart_thresholds st;
  ata_smart_read_thresholds(fd, (unsigned char*)&st);

  printf("ID   Name                                          Value  Worst  Thresh  Type       Updated   Raw          Status\n");

  int i=0;
  for(; i<SMART_ATTRIBUTE_NUM; i++) {

    /* Attribute is invalid */
    if(!sd.attributes[i].id) continue;

    /* Only predict failure if the value is valid and less than or equal to the threshold */
    const char* status = "OK";
    if(sd.attributes[i].value > 0x00 && sd.attributes[i].value < 0xfe &&
       sd.attributes[i].value <= st.thresholds[i].threshold) {

      /* Predicted failure is within 24 hours, otherwise the device lifespan has been exceeded */
      if(sd.attributes[i].flags & 0x1) {
        status = "CRITICAL";
        rc = MAX(rc, NAGIOS_CRITICAL);
      } else {
        status = "WARNING";
        rc = MAX(rc, NAGIOS_WARNING);
      }
    }

    printf("%3d  %-44s  %03d    %03d    %03d     %-8s   %-7s   %-11d  %s\n",
      sd.attributes[i].id,
      smart_id_to_text(sd.attributes[i].id),
      sd.attributes[i].value,
      sd.attributes[i].worst,
      st.thresholds[i].threshold,
      (sd.attributes[i].flags & 0x1) ? "Pre-fail" : "Advisory",
      (sd.attributes[i].flags & 0x2) ? "Always" : "Offline",
      sd.attributes[i].raw,
      status);
  }

  close(fd);

  return rc;
}

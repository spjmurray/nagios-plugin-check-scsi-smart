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

#include "scsi.h"
#include "ata.h"
#include "smart.h"

#include <iostream>
#include <memory>
#include <sstream>

using namespace std;

const char* const BINARY  = "check_scsi_smart";
const char* const VERSION = "1.1.0";

/* Nagios return codes */
const int NAGIOS_OK       = 0;
const int NAGIOS_WARNING  = 1;
const int NAGIOS_CRITICAL = 2;
const int NAGIOS_UNKNOWN  = 3;

const size_t SECTOR_SIZE = 512;

/*
 * Function: version
 * -----------------
 * Print out the version string
 */
void version() {

  cout << BINARY << " v" << VERSION << endl;

}

/*
 * Function: usage
 * ---------------
 * Print out the usage syntax
 */
void usage() {

  cout << "Usage:" << endl
       << BINARY << " [-d <device>]" << endl;

}

/*
 * Function: help
 * --------------
 * Print out the verbose help screen
 */
void help() {

  version();

  cout << "(C) 2015 Simon Murray <spjmurray@yahoo.co.uk>" << endl
       << endl;

  usage();

  cout << endl
       << "Options:" << endl
       << "-h, --help" << endl
       << "   Print detailed help" << endl
       << "-v, --version" << endl
       << "   Print version information" << endl
       << "-d, --device=DEVICE" << endl
       << "   Select device DEVICE" << endl
       << endl;

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
    cerr << "UNKNOWN: SG_IO ioctl error" << endl;
    exit(NAGIOS_UNKNOWN);
  }

}

/*
 * Function: ata_identify
 * ----------------------
 * Send an IDENTIFY command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least SECTOR
 */
void ata_identify(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;
  memset(reinterpret_cast<unsigned char*>(&ata_pass_through), 0, sizeof(sbc_ata_pass_through));

  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = ATA_TRANSFER_DIRECTION_FROM_DEVICE;
  ata_pass_through.byte_block     = ATA_TRANSFER_SIZE_BLOCK;
  ata_pass_through.t_type         = ATA_TRANSFER_TYPE_SECTOR;
  ata_pass_through.t_length       = ATA_TRANSFER_LENGTH_COUNT;
  ata_pass_through.count_7_0      = 1;
  ata_pass_through.command        = ATA_IDENTIFY_DEVICE;

  sgio(fd, reinterpret_cast<unsigned char*>(&ata_pass_through), sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_data
 * -----------------------------
 * Send a SMART READ DATA command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least SECTOR
 */
void ata_smart_read_data(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;
  memset(reinterpret_cast<unsigned char*>(&ata_pass_through), 0, sizeof(sbc_ata_pass_through));

  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = ATA_TRANSFER_DIRECTION_FROM_DEVICE;
  ata_pass_through.byte_block     = ATA_TRANSFER_SIZE_BLOCK;
  ata_pass_through.t_type         = ATA_TRANSFER_TYPE_SECTOR;
  ata_pass_through.t_length       = ATA_TRANSFER_LENGTH_COUNT;
  ata_pass_through.count_7_0      = 1;
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_DATA;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;

  sgio(fd, reinterpret_cast<unsigned char*>(&ata_pass_through), sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_thresholds
 * -----------------------------------
 * Send a SMART READ THRESHOLDS command to the ATA device and recieve the data
 *
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least SECTOR
 */
void ata_smart_read_thresholds(int fd, unsigned char* buf) {

  sbc_ata_pass_through ata_pass_through;
  memset(reinterpret_cast<unsigned char*>(&ata_pass_through), 0, sizeof(sbc_ata_pass_through));

  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = ATA_TRANSFER_DIRECTION_FROM_DEVICE;
  ata_pass_through.byte_block     = ATA_TRANSFER_SIZE_BLOCK;
  ata_pass_through.t_type         = ATA_TRANSFER_TYPE_SECTOR;
  ata_pass_through.t_length       = ATA_TRANSFER_LENGTH_COUNT;
  ata_pass_through.count_7_0      = 1;
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_THRESHOLDS;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;

  sgio(fd, reinterpret_cast<unsigned char*>(&ata_pass_through), sizeof(ata_pass_through), buf, SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_log
 * ----------------------------
 * Send a SMART READ LOG command to the ATA device and receive the data
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * buf: Data buffer to receive the data into, must be at least sectors * SECTOR
 *      bytes.
 * log: Log to read See A.1 for ATA8-ACS
 */
void ata_smart_read_log(int fd, unsigned char* buf, int log, uint16_t sectors) {

  sbc_ata_pass_through ata_pass_through;
  memset(reinterpret_cast<unsigned char*>(&ata_pass_through), 0, sizeof(sbc_ata_pass_through));

  ata_pass_through.operation_code = SBC_ATA_PASS_THROUGH;
  ata_pass_through.protocol       = ATA_PROTOCOL_PIO_DATA_IN;
  ata_pass_through.t_dir          = ATA_TRANSFER_DIRECTION_FROM_DEVICE;
  ata_pass_through.byte_block     = ATA_TRANSFER_SIZE_BLOCK;
  ata_pass_through.t_type         = ATA_TRANSFER_TYPE_SECTOR;
  ata_pass_through.t_length       = ATA_TRANSFER_LENGTH_COUNT;
  ata_pass_through.count_15_8     = sectors >> 8;
  ata_pass_through.count_7_0      = sectors;
  ata_pass_through.command        = ATA_SMART;
  ata_pass_through.features_7_0   = SMART_READ_LOG;
  ata_pass_through.lba_23_16      = 0xc2;
  ata_pass_through.lba_15_8       = 0x4f;
  ata_pass_through.lba_7_0        = log;

  sgio(fd, reinterpret_cast<unsigned char*>(&ata_pass_through), sizeof(ata_pass_through), buf, sectors * SECTOR_SIZE);

}

/*
 * Function: ata_smart_read_log_directory
 * --------------------------------------
 * Reads the SMART flog directory
 * fd: File descriptor pointing at a SCSI or SCSI generic device nod
 * buf: Data buffer to receive the data into, must be at least SECTOR
 */
void ata_smart_read_log_directory(int fd, unsigned char* buf) {

  ata_smart_read_log(fd, buf, ATA_LOG_ADDRESS_DIRECTORY, 1);

}

/*
 * Function: check_smart_attributes
 * --------------------------------
 * Checks attributes against vendor thresholds
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * code: Reference to the current return code
 * crit: Reference to a counter of critical attributes
 * warn: Reference to a counter of advisory attributes
 * perfdata: Output stream to dump performance data to
 */
void check_smart_attributes(int fd, int& code, int& crit, int& warn, ostream& perfdata) {

  // Load the SMART data and thresholds pages
  smart_data sd;
  ata_smart_read_data(fd, reinterpret_cast<unsigned char*>(&sd));

  smart_thresholds st;
  ata_smart_read_thresholds(fd, reinterpret_cast<unsigned char*>(&st));

  // Perform actual SMART threshold checks
  for(int i=0; i<SMART_ATTRIBUTE_NUM; i++) {

    SmartAttribute attribute(sd.attributes[i]);
    SmartThreshold threshold(st.thresholds[i]);

    if(!attribute.idValid())
      continue;

    // Check the validity of the attribute value and whether the threshold has been exceeded
    if(attribute.valueValid() && (attribute <= threshold)) {

      // Predicted failure is within 24 hours, otherwise the device lifespan has been exceeded
      if(attribute.getPreFail())
        crit++;
      else
        warn++;

    }

    // Accumulate the performance data
    perfdata << " " << attribute;

  }

  // Determine the state to report
  if(warn)
    code = max(code, NAGIOS_WARNING);

  if(crit)
    code = max(code, NAGIOS_CRITICAL);

}

/*
 * Function: check_smart_log
 * -------------------------
 * Checks for the existence of SMART logs
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * fd: File descriptor pointing at a SCSI or SCSI generic device node
 * code: Reference to the current return code
 * logs: Reference to a count of the number of SMART logs
 */
void check_smart_log(int fd, int& code, int& logs) {

  // Read the SMART log directory
  smart_log_directory log_directory;
  ata_smart_read_log_directory(fd, reinterpret_cast<unsigned char*>(&log_directory));

  // Calculate the number of SMART log sectors to read and allocate a buffer
  uint16_t smart_log_sectors = log_directory.data_blocks[ATA_LOG_ADDRESS_SMART];
  smart_log_summary* summaries = new smart_log_summary[smart_log_sectors];

  // Read the logs in
  ata_smart_read_log(fd, reinterpret_cast<unsigned char*>(summaries), ATA_LOG_ADDRESS_SMART, smart_log_sectors);

  // Check for any logged errors
  for(int i=0; i<smart_log_sectors; i++) {
    logs += summaries[i].count;
  }

  if(logs)
    code = max(code, NAGIOS_WARNING);

  delete [] summaries;

}


/*
 * Function: main
 * --------------
 * Reads device identity and checks for SMART capability, if so reads
 * the SMART data and thresholds and checks for any predictive failures
 */
int main(int argc, char** argv) {

  const char* device = 0;

  static struct option long_options[] = {
    { "help",    no_argument,       0, 'h' },
    { "version", no_argument,       0, 'v' },
    { "device",  required_argument, 0, 'd' },
    { 0,         0,                 0, 0   }
  };

  int c;
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
    cerr << "UNKNOWN: unable to open device " << device << endl;
    exit(NAGIOS_UNKNOWN);
  }

  int sg_version;
  if((ioctl(fd, SG_GET_VERSION_NUM, &sg_version) == -1) || sg_version < 30000) {
    cerr << "UNKNOWN: " << device << " is either not an sg device, or the driver is old" << endl;
    exit(NAGIOS_UNKNOWN);
  }

  // Check the device can use SMART and that it is enabled
  uint16_t identify[SECTOR_SIZE / 2];
  ata_identify(fd, reinterpret_cast<unsigned char*>(identify));

  if(~identify[82] & 0x01) {
    cout << "OK: SMART feature set unsupported" << endl;
    exit(NAGIOS_OK);
  }

  if(~identify[85] & 0x01) {
    cout << "UNKNOWN: SMART feature set disabled" << endl;
    exit(NAGIOS_UNKNOWN);
  }

  int code = NAGIOS_OK;
  int crit = 0;
  int warn = 0;
  int logs = 0;
  stringstream perfdata;

  // Perform the checks
  check_smart_attributes(fd, code, crit, warn, perfdata);
  check_smart_log(fd, code, logs);

  // Print out the results and performance data
  const char* status[] = { "OK", "WARNING", "CRITICAL" };
  cout << status[code] << ": predicted fails " << crit << ", advisories " << warn
       << ", errors " << logs << " |" << perfdata.str() << endl;

  close(fd);

  return code;

}

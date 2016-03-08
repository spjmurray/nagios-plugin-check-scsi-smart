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
 */

#ifndef _scsi_H_
#define _scsi_H_

#include <stdint.h>

/* SCSI primary commands */
#define SBC_ATA_PASS_THROUGH 0x85

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

#endif//_scsi_H_

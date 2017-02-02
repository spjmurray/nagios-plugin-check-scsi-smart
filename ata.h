/*
 * SMART Nagios/Icinga Disk Check
 * ------------------------------
 *
 * License
 * -------
 * (C) 2015-2017 Simon Murray <spjmurray@yahoo.co.uk>
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

#ifndef _ata_H_
#define _ata_H_

#include <stdint.h>

/* ATA commands */
const uint8_t ATA_IDENTIFY_DEVICE = 0xec;
const uint8_t ATA_SMART           = 0xb0;

/* ATA protocols */
const uint8_t ATA_PROTOCOL_PIO_DATA_IN = 0x4;

/* ATA transfer direction */
const uint8_t ATA_TRANSFER_DIRECTION_TO_DEVICE   = 0x0;
const uint8_t ATA_TRANSFER_DIRECTION_FROM_DEVICE = 0x1;

/* ATA block transfer mode */
const uint8_t ATA_TRANSFER_SIZE_BYTE  = 0x0;
const uint8_t ATA_TRANSFER_SIZE_BLOCK = 0x1;

/* ATA block transfer type */
const uint8_t ATA_TRANSFER_TYPE_SECTOR         = 0x0;
const uint8_t ATA_TRANSFER_TYPE_LOGICAL_SECTOR = 0x1;

/* ATA transfer length location */
const uint8_t ATA_TRANSFER_LENGTH_NONE     = 0x0;
const uint8_t ATA_TRANSFER_LENGTH_FEATURES = 0x1;
const uint8_t ATA_TRANSFER_LENGTH_COUNT    = 0x2;
const uint8_t ATA_TRANSFER_LENGTH_TPSIU    = 0x3;

/* ATA Log Addresses */
const uint8_t ATA_LOG_ADDRESS_DIRECTORY = 0x0;
const uint8_t ATA_LOG_ADDRESS_SMART     = 0x1;

#endif//_ata_H_

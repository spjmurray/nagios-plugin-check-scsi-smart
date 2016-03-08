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

#ifndef _ata_H_
#define _ata_H_

#include <stdint.h>

/* ATA commands */
const uint8_t ATA_IDENTIFY_DEVICE = 0xec;
const uint8_t ATA_SMART           = 0xb0;

/* ATA protocols */
const uint8_t ATA_PROTOCOL_PIO_DATA_IN = 0x4;

/* ATA Log Addresses */
const uint8_t ATA_LOG_ADDRESS_DIRECTORY = 0x0;
const uint8_t ATA_LOG_ADDRESS_SMART     = 0x1;

#endif//_ata_H_

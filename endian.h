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

#ifndef _endian_H_
#define _endian_H_

#include <byteswap.h>

/**
 * Class: StorageEndian
 * --------------------
 * Provides endian abstraction for loads and stores from ATA/SCSI operations.
 * These unsurprisingly are little-endian and need byteswapping on big endian
 * architectures.
 */
class StorageEndian {
public:
  template<class T>
  static inline T swap(T t) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return t;
#else
    switch(sizeof(T)) {
      case 1:
        return t;
      case 2:
        return bswap_16(t);
      case 4:
        return bswap_32(t);
      case 16:
        return bswap_64(t);
    }
#endif
  }
};

#endif//_endian_H_

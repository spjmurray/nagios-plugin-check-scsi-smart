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

#ifndef _smart_H_
#define _smart_H_

#include <stdint.h>
#include <iostream>

using namespace std;

/* Class Declarations */
class SmartAttribute;
class SmartThreshold;

/* SMART functions */
const uint8_t SMART_READ_DATA       = 0xd0;
const uint8_t SMART_READ_THRESHOLDS = 0xd1;
const uint8_t SMART_READ_LOG        = 0xd5;
const uint8_t SMART_RETURN_STATUS   = 0xda;

/* SMART off-line status */
const uint8_t SMART_OFF_LINE_STATUS_NEVER_STARTED  = 0x00;
const uint8_t SMART_OFF_LINE_STATUS_COMPLETED      = 0x02;
const uint8_t SMART_OFF_LINE_STATUS_IN_PROGRESS    = 0x03;
const uint8_t SMART_OFF_LINE_STATUS_SUSPENDED      = 0x04;
const uint8_t SMART_OFF_LINE_STATUS_ABORTED_HOST   = 0x05;
const uint8_t SMART_OFF_LINE_STATUS_ABORTED_DEVICE = 0x06;

/* Attributes in a smart_data page */
const uint8_t SMART_ATTRIBUTE_NUM = 30;

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
 * Struct: smart_log_directory
 * -------------------------
 * Structure defining the directory version and number of logs available
 * for each address.  Index 0 is the version, this is kept as an array to
 * enable resue of ATA_LOG_ADDRESS_* macros.
 */
typedef struct {
  uint16_t data_blocks[256];
} smart_log_directory;

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

/*
 * Struct: smart_log_summary
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
 * Class: SmartAttribute
 * ---------------
 * Wraps up the smart attribute and formatting
 */
class SmartAttribute {

public:
  /**
   * Function: SmartAttribute::SmartAttribute(const smart_attribute&)
   * ----------------------------------------------------------------
   * Class constructor to munge raw data structures into a sensible format
   * attribute: Reference to a smart_attribute structure
   */
  SmartAttribute(const smart_attribute& attribute);

  /**
   * Function SmartAttribute::getID()
   * --------------------------------
   * Returns the SMART attribute ID
   */
  inline uint8_t getID() const {
    return id;
  }

  /**
   * Function SmartAttribute::getPreFail()
   * -------------------------------------
   * Return whether this attribute predicts failure within 24h
   */
  inline bool getPreFail() const {
    return pre_fail;
  }

  /**
   * Function: SmartAttribute:getRaw()
   * ---------------------------------
   * Return the sanitised raw value
   */
  inline uint64_t getRaw() const {
    return raw;
  }

  /**
   * Function: SmartAttribute::idValid()
   * -----------------------------------
   * Checks whether a SMART ID is valid
   */
  inline bool idValid() const {
    return id != 0;
  }

  /**
   * Function: SmartAttribute::valueValid()
   * --------------------------------------
   * Checks whether a SMART value is within the valid limits
   */
  inline bool valueValid() const {
    return value > 0x0 && value < 0xfe;
  }

  /**
   * Function: SmartAttribute::operator<=(const SmartThreshold&)
   * -----------------------------------------------------------
   * Compares a value to a threshold
   * threshold: Reference to a SmartThreshold object to check against
   */
  bool operator<=(const SmartThreshold& threshold) const;

  friend ostream& operator<<(ostream& o, const SmartAttribute& id);

private:
  uint8_t id;
  bool pre_fail;
  bool offline;
  uint8_t value;
  uint64_t raw;

};

/**
 * Function: operator<<(ostream&, const SmartAttribute&)
 * ----------------------------------------
 * Function to dump human readable text to an output stream
 * o: Class implmenting std::ostream
 * id: Reference to a SmartAttribute class
 */
ostream& operator<<(ostream& o, const SmartAttribute& attribute);

/**
 * Class: SmartThreshold
 * ---------------------
 * Wrapper for a SMART threshold
 */
class SmartThreshold {

public:

  /**
   * Function: SmartThreshold::SmartThreshold(const smart_threshold&)
   * ----------------------------------------------------------------
   * Class constructor to create a SmartThreshold object from raw data
   * threshold: Reference to a smart_threshold object
   */
  SmartThreshold(const smart_threshold& threshold)
  : threshold(threshold.threshold)
  {}

  /**
   * Function: SmartThreshold::getThreshold()
   * ----------------------------------------
   * Accessor method for the threshold value
   */
  inline uint8_t getThreshold() const {
    return threshold;
  }

private:
  uint8_t threshold;

};

#endif//_smart_H_

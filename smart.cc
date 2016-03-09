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

#include "smart.h"

/**
 * Function: SmartAttribute::SmartAttribute(const smart_attribute&)
 * ----------------------------------------------------------------
 * Class constructor to munge raw data structures into a sensible format
 * attribute: Reference to a smart_attribute structure
 */
SmartAttribute::SmartAttribute(const smart_attribute& attribute)
: id(attribute.id),
  pre_fail(attribute.flags & 0x1),
  offline(attribute.flags & 0x2),
  value(attribute.value),
  raw((static_cast<uint64_t>(attribute.raw_hi) << 32) | static_cast<uint64_t>(attribute.raw_lo)) {

  // Logic shamelessly lifted from smartmontools
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

}

/**
 * Function: SmartAttribute::operator<=(const SmartThreshold&)
 * -----------------------------------------------------------
 * Compares a value to a threshold
 * threshold: Reference to a SmartThreshold object to check against
 */
bool SmartAttribute::operator<=(const SmartThreshold& threshold) const {

  return value <= threshold.getThreshold();

}

/**
 * Function: operator<<(ostream&, const SmartAttribute&)
 * ----------------------------------------
 * Function to dump human readable text to an output stream
 * o: Class implmenting std::ostream
 * id: Reference to a SmartAttribute class
 */
ostream& operator<<(ostream& o, const SmartAttribute& attribute) {

  static const char* labels[] = {
    // 0x00
    "unknown",
    "read_error_rate",
    "throughput_performance",
    "spin_up_time",
    "start_stop_count",
    "reallocated_sectors_count",
    "read_channel_margin",
    "seek_error_rate",
    "seek_time_performance",
    "power_on_hours",
    "spin_retry_count",
    "recalibration_retries",
    "power_cycle_count",
    "soft_read_error_rate",
    "unknown",
    "unknown",
    // 0x10
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "current_helium_level",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x20
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x30
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x40
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x50
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x60
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x70
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x80
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0x90
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0xa0
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "available_reserved_space",
    "ssd_program_fail_count",
    "ssd_erase_fail_count",
    "ssd_wear_leveling_count",
    "unexpected_power_loss_count",
    "power_loss_protection_failure",
    // 0xb0
    "erase_fail_count",
    "wear_range_delta",
    "unknown",
    "used_reserved_block_count_total",
    "unused_reserved_block_count_total",
    "program_fail_count_total",
    "erase_fail_count",
    "sata_downshift_error_count",
    "end_to_end_error",
    "head_stability",
    "induced_op_vibration_detection",
    "reported_uncorrectable_errors",
    "command_timeout",
    "high_fly_writes",
    "airflow_temperature",
    "g_sense_error_rate",
    // 0xc0
    "power_off_retract_count",
    "load_cycle_count",
    "temperature",
    "hardware_ecc_recovered",
    "reallocation_event_count",
    "current_pending_sector_count",
    "uncorrectable_sector_count",
    "ultradma_crc_error_count",
    "multi_zone_error_rate",
    "soft_read_error_rate",
    "data_address_mark_errors",
    "run_out_cancel",
    "soft_ecc_correction",
    "thermal_asperity_rate",
    "flying_height",
    "spin_height_current",
    // 0xd0
    "spin_buzz",
    "offline_seek_performance",
    "vibration_during_write",
    "vibration_during_write",
    "shock_during_write",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "disk_shift",
    "g_sense_error_rate",
    "loaded_hours",
    "load_unload_retry_count",
    // 0xe0
    "load_friction",
    "load_unload_cycle_count",
    "load_in_time",
    "torque_amplification_count",
    "power_off_retract_cycle",
    "unknown",
    "drive_life_protection_status",
    "temperature",
    "available_reserved_space",
    "media_wearout_indicator",
    "average_erase_count",
    "good_block_count",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    // 0xf0
    "flying_head_hours",
    "total_lbas_written",
    "total_lbas_read",
    "total_lbas_written_expanded",
    "total_lbas_read_expanded",
    "unknown",
    "unknown",
    "unknown",
    "unknown",
    "nand_writes_1gib",
    "read_error_retry_rate",
    "minimum_spares_remaining",
    "newly_added_bad_flash_block",
    "unknown",
    "free_fall_protection",
    "unknown",
  };

  o << dec << static_cast<unsigned int>(attribute.id) << "_" << labels[attribute.id] << "=" << attribute.raw;

  return o;

}

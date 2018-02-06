# Nagios SMART Drive Checker

## Description

Uses SCSI commands to tunnel SMART checks to ATA hard drives.  Unlike the
venerable check\_ide\_smart check this will work on all modern devices
even those behind SAS HBAs or expanders.  It will also monitor for SMART
error logs which may indicate failure when base SMART attributes do not.

As of version 1.1.0 the API has changed.  The check no longer emits verbose
output, this functionality is delegated to smartctl, however it does provide
all raw values as performance data.  This is the first step in providing
true predictive failure.  This data can be monitored via a Graphite writer
plugin and thresholds explicitly set for your particular device/environment.

## Prerequisites

* g++
* gmake

## Building

    make

## Usage

### Help

    check_scsi_smart v1.2.x
    (C) 2015-2016 Simon Murray <spjmurray@yahoo.co.uk>
    
    Usage:
    check_scsi_smart [-d <device>]
    
    Options:
    -h, --help
       Print detailed help
    -V, --version
       Print version information
    -d, --device=DEVICE
       Select device DEVICE
    -w, --warning=ID:THRESHOLD[,ID:THRESHOLD]
       Specify warning thresholds as a list of integer attributes to integer thresholds
    -c, --critical=ID:THRESHOLD[,ID:THRESHOLD]
       Specify critical thresholds as a list of integer attributes to integer thresholds

### Output

    $ sudo ./check_scsi_smart -d /dev/sdc -w 1:1000,3:1000 -c 187:1
    CRITICAL: prdfail 0, advisory 0, critical 1, warning 1, logs 2 | 1_read_error_rate=151669074;1000;;; 3_spin_up_time=0;1000;;; 4_start_stop_count=26;;;; 5_reallocated_sectors_count=10904;;;; 7_seek_error_rate=8645237955;;;; 9_power_on_hours=23052;;;; 10_spin_retry_count=0;;;; 12_power_cycle_count=25;;;; 183_sata_downshift_error_count=124;;;; 184_end_to_end_error=0;;;; 187_reported_uncorrectable_errors=2;;1;; 188_command_timeout=4295032833;;;; 189_high_fly_writes=1;;;; 190_airflow_temperature=23;;;; 191_g_sense_error_rate=0;;;; 192_power_off_retract_count=18;;;; 193_load_cycle_count=8823;;;; 194_temperature=23;;;; 197_current_pending_sector_count=4288;;;; 198_uncorrectable_sector_count=4288;;;; 199_ultradma_crc_error_count=0;;;; 240_flying_head_hours=22723;;;; 241_total_lbas_written=4595646719;;;; 242_total_lbas_read=1956891669;;;;

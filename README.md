#Nagios SMART Drive Checker

##Description

Uses SCSI commands to tunnel SMART checks to ATA hard drives.  Unlike the
venerable check\_ide\_smart check this will work on all modern devices
even those behind SAS HBAs or expanders.

##Prerequisites

* gcc
* gmake

##Building

    make

##Usage

    $ sudo ./check_scsi_smart -d /dev/sda
    ID   Name                               Value  Worst  Thresh  Type       Updated   Raw          Status
      5  Reallocated Sectors Count          100    100    010     Pre-fail   Always    0            OK
      9  Power-On Hours                     099    099    000     Advisory   Always    1358         OK
     12  Power Cycle Count                  099    099    000     Advisory   Always    139          OK
    177  Wear Range Delta                   099    099    000     Pre-fail   Always    3            OK
    179  Used Reserved Block Count Total    100    100    010     Pre-fail   Always    0            OK
    181  Program Fail Count Total           100    100    010     Advisory   Always    0            OK
    182  Erase Fail Count                   100    100    010     Advisory   Always    0            OK
    183  SATA Downshift Error Count         100    100    010     Pre-fail   Always    0            OK
    187  Reported Uncorrectable Errors      100    100    000     Advisory   Always    0            OK
    190  Airflow Temperature                066    059    000     Advisory   Always    34           OK
    195  Hardware ECC Recovered             200    200    000     Advisory   Always    0            OK
    199  UltraDMA CRC Error Count           099    099    000     Advisory   Always    315          OK
    235  Good Block Count                   099    099    000     Advisory   Always    75           OK
    241  Total LBAs Written                 099    099    000     Advisory   Always    539465178    OK


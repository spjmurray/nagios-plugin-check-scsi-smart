Name: nagios-plugin-check-scsi-smart
Version: 1.0.1
Release: 1
License: GPLv3
Summary: Nagios plugin to perform SMART checks on SATA devices on SCSI buses
Group: System Environment/Base
URL: http://github.com/spjmurray/check_scsi_smart

%description
Tunnels ATA SMART commands over SCSI transports.  This allows ATA SMART checks
to work for SATA drives which are directly attached to a SATA controller a SAS
HBA or a SAS expander.  The SAT translation layer handles decpasulating the
ATA command at the relvant boundary between SCSI and SATA protocols.

%build
make

%install
make install LIBDIR=lib64 DESTDIR=%{buildroot}

%files
/usr/lib64/nagios/plugins/check_scsi_smart

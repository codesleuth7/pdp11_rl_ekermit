PDP-11 RL01/RL02 bare metal ekermit server. This project was created as a way to backup PDP-11 RL01/RL02 disks using minimal hardware without an operating system. If you have a bootable PDP-11 with other storage peripherals then faster options to backup RL disks may work better then this. Using the Kermit protocol to backup megabytes of data is slow, especially with some console serial port speed limitations.

The E-Kermit (Embedded Kermit) package from Columbia University and authored by Frank da Cruz is the foundation of this code. All the PDP-11 hardware support software was added to run as a bare metal program.

Develment environment:
This code was compiled using GCC version 9.3.0 configured for the PDP-11.

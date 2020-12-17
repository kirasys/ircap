#pragma once
//
// Device type           -- in the "User Defined" range."
//
#define SIOCTL_TYPE 40000
//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
#define IOCTL_SIOCTL_METHOD_BUFFERED \
    CTL_CODE( SIOCTL_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS  )

#define DRIVER_NAME  "ircap"
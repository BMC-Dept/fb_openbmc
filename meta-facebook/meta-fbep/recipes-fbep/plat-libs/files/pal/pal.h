/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __PAL_H__
#define __PAL_H__

#include <openbmc/obmc-pal.h>
#include <openbmc/kv.h>
#include "pal_sensors.h"
#include "pal_health.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PWR_OPTION_LIST "status, off, on, cycle"
#define FRU_BIN "/tmp/fruid.bin"
#define FRU_EEPROM "/sys/class/i2c-dev/i2c-6/device/6-0054/eeprom"
#define PDB_BIN "/tmp/pdbid.bin"
#define PDB_EEPROM "/sys/class/i2c-dev/i2c-16/device/16-0054/eeprom"

#define LARGEST_DEVICE_NAME 120
#define ERR_NOT_READY -2

extern size_t pal_pwm_cnt;
extern size_t pal_tach_cnt;
extern const char pal_pwm_list[];
extern const char pal_tach_list[];
extern const char pal_fru_list[];
extern const char pal_server_list[];

enum {
  FRU_ALL = 0,
  FRU_BASE,
  FRU_PDB,
};

enum {
  SERVER_1 = 0x0,
  SERVER_2,
  SERVER_3,
  SERVER_4,
};

enum {
  BMC = 0x0,
  PCH,
};

#define MAX_NUM_FRUS 2
#define MAX_NODES    1

int read_device(const char *device, int *value);
int write_device(const char *device, int value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __PAL_H__ */

/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This file contains code to support IPMI2.0 Specificaton available @
 * http://www.intel.com/content/www/us/en/servers/ipmi/ipmi-specifications.html
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <openbmc/libgpio.h>
#include <openbmc/ipmi.h>
#include "pal.h"

#define LTC4282_DIR(bus, addr, index) \
  "/sys/bus/i2c/drivers/ltc4282/"#bus"-00"#addr"/hwmon/hwmon"#index"/%s"
#define LTC4282_STATUS_PWR_GOOD	"power_good"
#define LTC4282_REBOOT		"reboot"
#define P12V_1_DIR	LTC4282_DIR(16, 53, 6)
#define P12V_2_DIR	LTC4282_DIR(17, 40, 7)
#define P12V_AUX_DIR	LTC4282_DIR(18, 43, 8)

#define DELAY_POWER_CYCLE 10
#define MAX_RETRY 10

#define BMC_IPMB_SLAVE_ADDR 0x16

#define GUID_SIZE 16
#define OFFSET_DEV_GUID 0x1800

#define LAST_KEY "last_key"

const char pal_fru_list[] = "all, mb, pdb, bsm";
const char pal_server_list[] = "mb";

char g_dev_guid[GUID_SIZE] = {0};

enum key_event {
  KEY_BEFORE_SET,
  KEY_AFTER_INI,
};

struct pal_key_cfg {
  char *name;
  char *def_val;
  int (*function)(int, void*);
} key_cfg[] = {
  /* name, default value, function */
  {"identify_sled", "off", NULL},
  {"timestamp_sled", "0", NULL},
  {"base_sensor_health", "1", NULL},
  //{"base_sel_error", "1", NULL},
  /* Add more Keys here */
  {LAST_KEY, LAST_KEY, NULL} /* This is the last key of the list */
};

int write_device(const char *device, int value)
{
  FILE *fp;
  char val[8] = {0};

  fp = fopen(device, "w");
  if (fp == NULL) {
#ifdef DEBUG
    syslog(LOG_WARNING, "failed to write device %s", device);
#endif
    return -1;
  }

  snprintf(val, 8, "%d", value);
  fputs(val, fp);
  fclose(fp);

  return 0;
}

int read_device(const char *device, int *value)
{
  FILE *fp;
  int ret = 0;

  fp = fopen(device, "r");
  if (fp == NULL) {
#ifdef DEBUG
    syslog(LOG_WARNING, "failed to read device %s", device);
#endif
    return -1;
  }

  if (fscanf(fp, "%d", value) != 1) {
    ret = -1;
  }
  fclose(fp);
  return ret;
}

static int pal_key_index(char *key)
{
  int i = 0;

  while (strcmp(key_cfg[i].name, LAST_KEY)) {

    // If Key is valid, return success
    if (!strcmp(key, key_cfg[i].name))
      return i;

    i++;
  }

#ifdef DEBUG
  syslog(LOG_WARNING, "pal_key_index: invalid key - %s", key);
#endif
  return -1;
}

int pal_get_key_value(char *key, char *value)
{
  int index;

  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  return kv_get(key, value, NULL, KV_FPERSIST);
}

int pal_set_key_value(char *key, char *value)
{
  int index, ret;
  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  if (key_cfg[index].function) {
    ret = key_cfg[index].function(KEY_BEFORE_SET, value);
    if (ret < 0)
      return ret;
  }

  return kv_set(key, value, 0, KV_FPERSIST);
}

int pal_set_def_key_value()
{
  int i;

  for (i = 0; strcmp(key_cfg[i].name, LAST_KEY) != 0; i++) {
    if (kv_set(key_cfg[i].name, key_cfg[i].def_val, 0, KV_FCREATE | KV_FPERSIST)) {
#ifdef DEBUG
      syslog(LOG_WARNING, "pal_set_def_key_value: kv_set failed.");
#endif
    }
    if (key_cfg[i].function) {
      key_cfg[i].function(KEY_AFTER_INI, key_cfg[i].name);
    }
  }

  return 0;
}

int pal_channel_to_bus(int channel)
{
  switch (channel) {
    case 0:
      return 13; // USB (LCD Debug Board)
    case 1:
      return 1; // MB#1
    case 2:
      return 0; // MB#2
    case 3:
      return 3; // MB#3
    case 4:
      return 2; // MB#4
  }

  // Debug purpose, map to real bus number
  if (channel & 0x80) {
    return (channel & 0x7f);
  }

  return channel;
}

int pal_get_fruid_path(uint8_t fru, char *path)
{
  if (fru == FRU_MB)
    sprintf(path, MB_BIN);
  else if (fru == FRU_PDB)
    sprintf(path, PDB_BIN);
  else if (fru == FRU_BSM)
    sprintf(path, BSM_BIN);
  else
    return -1;

  return 0;
}

int pal_get_fruid_eeprom_path(uint8_t fru, char *path)
{
  if (fru == FRU_MB)
    sprintf(path, MB_EEPROM);
  else if (fru == FRU_PDB)
    sprintf(path, PDB_EEPROM);
  else if (fru == FRU_BSM)
    sprintf(path, BSM_EEPROM);
  else
    return -1;

  return 0;
}

int pal_get_fru_id(char *str, uint8_t *fru)
{
  if (!strcmp(str, "all")) {
    *fru = FRU_ALL;
  } else if (!strcmp(str, "mb")) {
    *fru = FRU_MB;
  } else if (!strcmp(str, "pdb")) {
    *fru = FRU_PDB;
  } else if (!strcmp(str, "bsm")) {
    *fru = FRU_BSM;
  } else {
    syslog(LOG_WARNING, "%s: Wrong fru name %s", __func__, str);
    return -1;
  }

  return 0;
}

int pal_get_fruid_name(uint8_t fru, char *name)
{
  if (fru == FRU_MB)
    sprintf(name, "Base Board");
  else if (fru == FRU_PDB)
    sprintf(name, "PDB");
  else if (fru == FRU_BSM)
    sprintf(name, "BSM");
  else
    return -1;

  return 0;
}

int pal_is_fru_ready(uint8_t fru, uint8_t *status)
{
  if (fru == FRU_MB || fru == FRU_PDB || fru == FRU_BSM)
    *status = 1;
  else
    return -1;

  return 0;
}

int pal_get_fru_name(uint8_t fru, char *name)
{
  if (fru == FRU_MB) {
    strcpy(name, "mb");
  } else if (fru == FRU_PDB) {
    strcpy(name, "pdb");
  } else if (fru == FRU_BSM) {
    strcpy(name, "bsm");
  } else {
    syslog(LOG_WARNING, "%s: Wrong fruid %d", __func__, fru);
    return -1;
  }

  return 0;
}

int pal_get_bmc_ipmb_slave_addr(uint16_t *slave_addr, uint8_t bus_id)
{
  *slave_addr = BMC_IPMB_SLAVE_ADDR;
  return 0;
}

// GUID for System and Device
static int pal_get_guid(uint16_t offset, char *guid) {
  int fd = 0;
  ssize_t bytes_rd;

  errno = 0;

  // Check if file is present
  if (access(MB_EEPROM, F_OK) == -1) {
      syslog(LOG_ERR, "pal_get_guid: unable to access the %s file: %s",
          MB_EEPROM, strerror(errno));
      return errno;
  }

  // Open the file
  fd = open(MB_EEPROM, O_RDONLY);
  if (fd == -1) {
    syslog(LOG_ERR, "pal_get_guid: unable to open the %s file: %s",
        MB_EEPROM, strerror(errno));
    return errno;
  }

  // seek to the offset
  lseek(fd, offset, SEEK_SET);

  // Read bytes from location
  bytes_rd = read(fd, guid, GUID_SIZE);
  if (bytes_rd != GUID_SIZE) {
    syslog(LOG_ERR, "pal_get_guid: read to %s file failed: %s",
        MB_EEPROM, strerror(errno));
    goto err_exit;
  }

err_exit:
  close(fd);
  return errno;
}

static int pal_set_guid(uint16_t offset, char *guid) {
  int fd = 0;
  ssize_t bytes_wr;

  errno = 0;

  // Check for file presence
  if (access(MB_EEPROM, F_OK) == -1) {
      syslog(LOG_ERR, "pal_set_guid: unable to access the %s file: %s",
          MB_EEPROM, strerror(errno));
      return errno;
  }

  // Open file
  fd = open(MB_EEPROM, O_WRONLY);
  if (fd == -1) {
    syslog(LOG_ERR, "pal_set_guid: unable to open the %s file: %s",
        MB_EEPROM, strerror(errno));
    return errno;
  }

  // Seek the offset
  lseek(fd, offset, SEEK_SET);

  // Write GUID data
  bytes_wr = write(fd, guid, GUID_SIZE);
  if (bytes_wr != GUID_SIZE) {
    syslog(LOG_ERR, "pal_set_guid: write to %s file failed: %s",
        MB_EEPROM, strerror(errno));
    goto err_exit;
  }

err_exit:
  close(fd);
  return errno;
}

// GUID based on RFC4122 format @ https://tools.ietf.org/html/rfc4122
static void pal_populate_guid(char *guid, char *str) {
  unsigned int secs;
  unsigned int usecs;
  struct timeval tv;
  uint8_t count;
  uint8_t lsb, msb;
  int i, r;

  // Populate time
  gettimeofday(&tv, NULL);

  secs = tv.tv_sec;
  usecs = tv.tv_usec;
  guid[0] = usecs & 0xFF;
  guid[1] = (usecs >> 8) & 0xFF;
  guid[2] = (usecs >> 16) & 0xFF;
  guid[3] = (usecs >> 24) & 0xFF;
  guid[4] = secs & 0xFF;
  guid[5] = (secs >> 8) & 0xFF;
  guid[6] = (secs >> 16) & 0xFF;
  guid[7] = (secs >> 24) & 0x0F;

  // Populate version
  guid[7] |= 0x10;

  // Populate clock seq with randmom number
  //getrandom(&guid[8], 2, 0);
  srand(time(NULL));
  //memcpy(&guid[8], rand(), 2);
  r = rand();
  guid[8] = r & 0xFF;
  guid[9] = (r>>8) & 0xFF;

  // Use string to populate 6 bytes unique
  // e.g. LSP62100035 => 'S' 'P' 0x62 0x10 0x00 0x35
  count = 0;
  for (i = strlen(str)-1; i >= 0; i--) {
    if (count == 6) {
      break;
    }

    // If alphabet use the character as is
    if (isalpha(str[i])) {
      guid[15-count] = str[i];
      count++;
      continue;
    }

    // If it is 0-9, use two numbers as BCD
    lsb = str[i] - '0';
    if (i > 0) {
      i--;
      if (isalpha(str[i])) {
        i++;
        msb = 0;
      } else {
        msb = str[i] - '0';
      }
    } else {
      msb = 0;
    }
    guid[15-count] = (msb << 4) | lsb;
    count++;
  }

  // zero the remaining bytes, if any
  if (count != 6) {
    memset(&guid[10], 0, 6-count);
  }

}

int pal_set_dev_guid(uint8_t fru, char *str) {
  pal_populate_guid(g_dev_guid, str);

  return pal_set_guid(OFFSET_DEV_GUID, g_dev_guid);
}

int pal_get_dev_guid(uint8_t fru, char *guid) {

  pal_get_guid(OFFSET_DEV_GUID, g_dev_guid);

  memcpy(guid, g_dev_guid, GUID_SIZE);

  return 0;
}

int pal_get_server_power(uint8_t fru, uint8_t *status)
{
  int val;
  char device[LARGEST_DEVICE_NAME] = {0};

  if (fru != FRU_MB)
    return -1;

  snprintf(device, LARGEST_DEVICE_NAME, P12V_1_DIR, LTC4282_STATUS_PWR_GOOD);
  if (read_device(device, &val) < 0)
    return -1;

  *status = val;

  snprintf(device, LARGEST_DEVICE_NAME, P12V_2_DIR, LTC4282_STATUS_PWR_GOOD);
  if (read_device(device, &val) < 0)
    return -1;

  *status &= val;
  *status = *status == 1? SERVER_POWER_ON: SERVER_POWER_OFF;

  return 0;
}

int pal_set_usb_path(uint8_t slot, uint8_t endpoint)
{
  int ret = CC_SUCCESS;
  char gpio_name[16] = {0};
  gpio_desc_t *desc;
  gpio_value_t value;

  if (slot > SERVER_4 || endpoint > PCH) {
    ret = CC_PARAM_OUT_OF_RANGE;
    goto exit;
  }

  desc = gpio_open_by_shadow("SEL_USB_MUX");
  if (!desc) {
    ret = CC_UNSPECIFIED_ERROR;
    goto exit;
  }
  value = endpoint == BMC? GPIO_VALUE_LOW: GPIO_VALUE_HIGH;
  if (gpio_set_value(desc, value)) {
    ret = CC_UNSPECIFIED_ERROR;
    goto bail;
  }
  gpio_close(desc);

  strcpy(gpio_name, endpoint == BMC? "USB2_SEL0_U43": "USB2_SEL0_U42");
  desc = gpio_open_by_shadow(gpio_name);
  if (!desc) {
    ret = CC_UNSPECIFIED_ERROR;
    goto exit;
  }
  value = slot%2 == 0? GPIO_VALUE_LOW: GPIO_VALUE_HIGH;
  if (gpio_set_value(desc, value)) {
    ret = CC_UNSPECIFIED_ERROR;
    goto bail;
  }
  gpio_close(desc);

  strcpy(gpio_name, endpoint == BMC? "USB2_SEL1_U43": "USB2_SEL1_U42");
  desc = gpio_open_by_shadow(gpio_name);
  if (!desc) {
    ret = CC_UNSPECIFIED_ERROR;
    goto exit;
  }
  value = slot/2 == 0? GPIO_VALUE_LOW: GPIO_VALUE_HIGH;
  if (gpio_set_value(desc, value)) {
    ret = CC_UNSPECIFIED_ERROR;
    goto bail;
  }

bail:
  gpio_close(desc);
exit:
  return ret;
}

static int server_power_on()
{
  int ret = -1;
  gpio_desc_t *gpio = gpio_open_by_shadow("BMC_IPMI_PWR_ON");

  if (!gpio) {
    return -1;
  }
  if (gpio_set_value(gpio, GPIO_VALUE_HIGH)) {
    goto bail;
  }
  if (gpio_set_value(gpio, GPIO_VALUE_LOW)) {
    goto bail;
  }
  sleep(1);
  if (gpio_set_value(gpio, GPIO_VALUE_HIGH)) {
    goto bail;
  }
  sleep(2);
  if (system("/usr/bin/sv restart fscd >> /dev/null")) {
    syslog(LOG_CRIT, "Restarting FSCD failed!\n");
    goto bail;
  }
  ret = 0;
bail:
  gpio_close(gpio);
  return ret;
}

static int server_power_off()
{
  int ret = -1;
  gpio_desc_t *gpio = gpio_open_by_shadow("BMC_IPMI_PWR_ON");

  if (!gpio) {
    return -1;
  }

  if (system("/usr/bin/sv stop fscd >> /dev/null")) {
    syslog(LOG_CRIT, "Stopping FSCD failed!\n");
    gpio_close(gpio);
    return -1;
  }

  if (gpio_set_value(gpio, GPIO_VALUE_HIGH)) {
    goto bail;
  }
  if (gpio_set_value(gpio, GPIO_VALUE_LOW)) {
    goto bail;
  }
  sleep(1);
  if (gpio_set_value(gpio, GPIO_VALUE_HIGH)) {
    goto bail;
  }
  ret = 0;
bail:
  gpio_close(gpio);
  return ret;
}

static bool is_server_off()
{
  uint8_t status;

  if (pal_get_server_power(FRU_MB, &status) < 0)
    return false;

  return status == SERVER_POWER_OFF? true: false;
}

// Power Off, Power On, or Power Cycle
int pal_set_server_power(uint8_t fru, uint8_t cmd)
{
  uint8_t status;

  if (pal_get_server_power(fru, &status) < 0) {
    return -1;
  }

  switch (cmd) {
    case SERVER_POWER_ON:
        return status == SERVER_POWER_ON? 1: server_power_on();
      break;

    case SERVER_POWER_OFF:
        return status == SERVER_POWER_OFF? 1: server_power_off();
      break;

    case SERVER_POWER_CYCLE:
      if (status == SERVER_POWER_ON) {
        if (server_power_off())
          return -1;

        sleep(DELAY_POWER_CYCLE);

        return server_power_on();
      } else if (status == SERVER_POWER_OFF) {
        return server_power_on();
      }
      break;

    default:
      return -1;
  }

  return 0;
}

void* sled_cycle_handler(void* arg)
{
  sleep(4);
  pal_sled_cycle();
  pthread_exit(0);
}

int pal_chassis_control(uint8_t fru, uint8_t *req_data, uint8_t req_len)
{
  int ret = 0;
  pthread_t tid;

  if (req_len != 1) {
    return CC_INVALID_LENGTH;
  }

  switch (req_data[0]) {
    case 0x00:  // power off
      ret = pal_set_server_power(fru, SERVER_POWER_OFF);
      break;
    case 0x01:  // power on
      ret = pal_set_server_power(fru, SERVER_POWER_ON);
      break;
    case 0x02:  // power cycle
      ret = pal_set_server_power(fru, SERVER_POWER_CYCLE);
      break;
    case 0xAC:  // sled-cycle with delay 4 secs
      if (pthread_create(&tid, NULL, sled_cycle_handler, NULL) < 0) {
        syslog(LOG_WARNING, "ipmid: bbv_power_cycle_handler pthread_create failed\n");
	ret = -1;
      }
      break;
    default:
      return CC_INVALID_DATA_FIELD;
  }

  return ret < 0? CC_UNSPECIFIED_ERROR: CC_SUCCESS;
}

void pal_get_chassis_status(uint8_t fru, uint8_t *req_data, uint8_t *res_data, uint8_t *res_len)
{
   unsigned char *data = res_data;

   *data++ = is_server_off()? 0x00:0x01;
   *res_len = data - res_data;
}

int pal_sled_cycle(void)
{
  char device[LARGEST_DEVICE_NAME] = {0};

  // reboot LTC4282 for 12V
  snprintf(device, LARGEST_DEVICE_NAME, P12V_AUX_DIR, LTC4282_REBOOT);
  if (write_device(device, 1) < 0) {
    syslog(LOG_CRIT, "SLED Cycle failed!\n");
    return -1;
  }
  return 0;
}

int pal_is_fru_prsnt(uint8_t fru, uint8_t *status)
{
  if (fru == FRU_MB || fru == FRU_PDB)
    *status = 1;
  else
    return -1;

  return 0;
}

void pal_sensor_assert_handle(uint8_t fru, uint8_t snr_num, float val, uint8_t thresh)
{
  gpio_desc_t *fan_ok;
  gpio_desc_t *fan_fail;

  switch (snr_num) {
    case MB_FAN0_TACH_I:
    case MB_FAN0_TACH_O:
    case MB_FAN0_VOLT:
    case MB_FAN0_CURR:
      fan_ok = gpio_open_by_shadow("FAN0_OK");
      fan_fail = gpio_open_by_shadow("FAN0_FAIL");
      break;
    case MB_FAN1_TACH_I:
    case MB_FAN1_TACH_O:
    case MB_FAN1_VOLT:
    case MB_FAN1_CURR:
      fan_ok = gpio_open_by_shadow("FAN1_OK");
      fan_fail = gpio_open_by_shadow("FAN1_FAIL");
      break;
    case MB_FAN2_TACH_I:
    case MB_FAN2_TACH_O:
    case MB_FAN2_VOLT:
    case MB_FAN2_CURR:
      fan_ok = gpio_open_by_shadow("FAN2_OK");
      fan_fail = gpio_open_by_shadow("FAN2_FAIL");
      break;
    case MB_FAN3_TACH_I:
    case MB_FAN3_TACH_O:
    case MB_FAN3_VOLT:
    case MB_FAN3_CURR:
      fan_ok = gpio_open_by_shadow("FAN3_OK");
      fan_fail = gpio_open_by_shadow("FAN3_FAIL");
      break;
    default:
      return;
  }

  if (!fan_ok || !fan_fail) {
    syslog(LOG_WARNING, "FAN LED open failed\n");
    goto exit;
  }

  if (gpio_set_value(fan_fail, GPIO_VALUE_HIGH) ||
      gpio_set_value(fan_ok, GPIO_VALUE_LOW)) {
    syslog(LOG_WARNING, "FAN LED control failed\n");
    goto exit;
  }

exit:
  gpio_close(fan_ok);
  gpio_close(fan_fail);
}

void pal_sensor_deassert_handle(uint8_t fru, uint8_t snr_num, float val, uint8_t thresh)
{
  gpio_desc_t *fan_ok;
  gpio_desc_t *fan_fail;

  switch (snr_num) {
    case MB_FAN0_TACH_I:
    case MB_FAN0_TACH_O:
    case MB_FAN0_VOLT:
    case MB_FAN0_CURR:
      fan_ok = gpio_open_by_shadow("FAN0_OK");
      fan_fail = gpio_open_by_shadow("FAN0_FAIL");
      break;
    case MB_FAN1_TACH_I:
    case MB_FAN1_TACH_O:
    case MB_FAN1_VOLT:
    case MB_FAN1_CURR:
      fan_ok = gpio_open_by_shadow("FAN1_OK");
      fan_fail = gpio_open_by_shadow("FAN1_FAIL");
      break;
    case MB_FAN2_TACH_I:
    case MB_FAN2_TACH_O:
    case MB_FAN2_VOLT:
    case MB_FAN2_CURR:
      fan_ok = gpio_open_by_shadow("FAN2_OK");
      fan_fail = gpio_open_by_shadow("FAN2_FAIL");
      break;
    case MB_FAN3_TACH_I:
    case MB_FAN3_TACH_O:
    case MB_FAN3_VOLT:
    case MB_FAN3_CURR:
      fan_ok = gpio_open_by_shadow("FAN3_OK");
      fan_fail = gpio_open_by_shadow("FAN3_FAIL");
      break;
    default:
      return;
  }

  if (!fan_ok || !fan_fail) {
    syslog(LOG_WARNING, "FAN LED open failed\n");
    goto exit;
  }

  if (gpio_set_value(fan_ok, GPIO_VALUE_HIGH) ||
      gpio_set_value(fan_fail, GPIO_VALUE_LOW)) {
    syslog(LOG_WARNING, "FAN LED control failed\n");
    goto exit;
  }

exit:
  gpio_close(fan_ok);
  gpio_close(fan_fail);
}

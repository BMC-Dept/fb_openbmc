#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <openbmc/kv.h>
#include <openbmc/libgpio.h>
#include <openbmc/obmc-i2c.h>
#include <openbmc/ipmb.h>
#include <openbmc/obmc-sensors.h>
#include "pal.h"

//#define DEBUG

#define FAN_DIR "/sys/bus/platform/devices/1e786000.pwm-tacho-controller/hwmon/hwmon0"
#define MAX_SDR_LEN 64
#define SDR_PATH "/tmp/sdr_%s.bin"

static int read_adc_val(uint8_t adc_id, float *value);
static int read_temp(uint8_t snr_id, float *value);
static int read_hsc_vin(uint8_t hsc_id, float *value);
static int read_hsc_temp(uint8_t hsc_id, float *value);
static int pal_bic_sensor_read_raw(uint8_t fru, uint8_t sensor_num, float *value);

static sensor_info_t g_sinfo[MAX_NUM_FRUS][MAX_SENSOR_NUM] = {0};
static bool sdr_init_done[MAX_NUM_FRUS] = {false};

size_t pal_pwm_cnt = 4;
size_t pal_tach_cnt = 8;
const char pal_pwm_list[] = "0, 1, 2, 4";
const char pal_tach_list[] = "0..7";

const uint8_t bmc_sensor_list[] = {
  BMC_SENSOR_OUTLET_TEMP,
  BMC_SENSOR_INLET_TEMP,
  BMC_SENSOR_P5V,
  BMC_SENSOR_P12V,
  BMC_SENSOR_P3V3_STBY,
  BMC_SENSOR_P1V15_BMC_STBY,
  BMC_SENSOR_P1V2_BMC_STBY,
  BMC_SENSOR_P2V5_BMC_STBY,
  BMC_SENSOR_HSC_TEMP,
  BMC_SENSOR_HSC_IN_VOLT,
};

const uint8_t bic_sensor_list[] = {
  //BIC - threshold sensors
  BIC_SENSOR_INLET_TEMP,
  BIC_SENSOR_OUTLET_TEMP,
  BIC_SENSOR_FIO_TEMP,
  BIC_SENSOR_PCH_TEMP,
  BIC_SENSOR_CPU_THERMAL_MARGIN,
  BIC_SENSOR_CPU_TEMP,
  BIC_SENSOR_DIMMA0_TEMP,
  BIC_SENSOR_DIMMB0_TEMP,
  BIC_SENSOR_DIMMC0_TEMP,
  BIC_SENSOR_DIMMD0_TEMP,
  BIC_SENSOR_DIMME0_TEMP,
  BIC_SENSOR_DIMMF0_TEMP,
  BIC_SENSOR_M2A_TEMP,
  BIC_SENSOR_M2B_TEMP,
  BIC_SENSOR_HSC_TEMP,
  BIC_SENSOR_VCCIN_VR_TEMP,
  BIC_SENSOR_VCCSA_VR_TEMP,
  BIC_SENSOR_VCCIO_VR_Temp,
  BIC_SENSOR_P3V3_STBY_VR_TEMP,
  BIC_SENSOR_PVDDQ_ABC_VR_TEMP,
  BIC_SENSOR_PVDDQ_DEF_VR_TEMP,

  //BIC - voltage sensors
  BIC_SENSOR_P3V3_M2A_VOL,
  BIC_SENSOR_P3V3_M2B_VOL,
  BIC_SENSOR_P12V_STBY_VOL,
  BIC_SENSOR_P3V_BAT_VOL,
  BIC_SENSOR_P3V3_STBY_VOL,
  BIC_SENSOR_P1V05_PCH_STBY_VOL,
  BIC_SENSOR_PVNN_PCH_STBY_VOL,
  BIC_SENSOR_P3V3_VOL,
  BIC_SENSOR_HSC_INPUT_VOL,
  BIC_SENSOR_VCCIN_VR_VOL,
  BIC_SENSOR_VCCSA_VR_VOL,
  BIC_SENSOR_VCCIO_VR_VOL,
  BIC_SENSOR_P3V3_STBY_VR_VOL,
  BIC_PVDDQ_ABC_VR_VOL,
  BIC_PVDDQ_DEF_VR_VOL,

  //BIC - current sensors
  BIC_SENSOR_P3V3_M2A_CUR,
  BIC_SENSOR_P3V3_M2B_CUR,
  BIC_SENSOR_HSC_OUTPUT_CUR1,
  BIC_SENSOR_HSC_OUTPUT_CUR2,
  BIC_SENSOR_VCCIN_VR_CUR,
  BIC_SENSOR_VCCSA_VR_CUR,
  BIC_SENSOR_VCCIO_VR_CUR,
  BIC_SENSOR_P3V3_STBY_VR_CUR,
  BIC_SENSOR_PVDDQ_ABC_VR_CUR,
  BIC_SENSOR_PVDDQ_DEF_VR_CUR,

  //BIC - power sensors
  BIC_SENSOR_P3V3_M2A_PWR,
  BIC_SENSOR_P3V3_M2B_PWR,
  BIC_SENSOR_HSC_INPUT_PWR1,
  BIC_SENSOR_HSC_INPUT_PWR2,
  BIC_SENSOR_VCCIN_VR_POUT,
  BIC_SENSOR_VCCSA_VR_POUT,
  BIC_SENSOR_VCCIO_VR_POUT,
  BIC_SENSOR_P3V3_STBY_VR_POUT,
  BIC_SENSOR_PVDDQ_ABC_VR_POUT,
  BIC_SENSOR_PVDDQ_DEF_VR_POUT,

  //BIC 1OU EXP Sensors
  BIC_1OU_EXP_SENSOR_OUTLET_TEMP,
  BIC_1OU_EXP_SENSOR_P12_VOL,
  BIC_1OU_EXP_SENSOR_P1V8_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_STBY_VR_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_STBY2_VR_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_M2A_PWR,
  BIC_1OU_EXP_SENSOR_P3V3_M2A_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_M2B_PWR,
  BIC_1OU_EXP_SENSOR_P3V3_M2B_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_M2C_PWR,
  BIC_1OU_EXP_SENSOR_P3V3_M2C_VOL,
  BIC_1OU_EXP_SENSOR_P3V3_M2D_PWR,
  BIC_1OU_EXP_SENSOR_P3V3_M2D_VOL,

  //BIC 2OU EXP Sensors
  BIC_2OU_EXP_SENSOR_OUTLET_TEMP,
  BIC_2OU_EXP_SENSOR_P12_VOL,
  BIC_2OU_EXP_SENSOR_P1V8_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_STBY_VR_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_STBY2_VR_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2A_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2A_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2B_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2B_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2C_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2C_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2D_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2D_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2E_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2E_VOL,
  BIC_2OU_EXP_SENSOR_P3V3_M2F_PWR,
  BIC_2OU_EXP_SENSOR_P3V3_M2F_VOL,
};

const uint8_t nic_sensor_list[] = {
  NIC_SENSOR_MEZZ_TEMP,
};

// List of MB discrete sensors to be monitored
const uint8_t bmc_discrete_sensor_list[] = {
};
 
//ADM1278
PAL_ADM1278_INFO adm1278_info_list[] = {
  {ADM1278_VOLTAGE, 19599, 0, 100},
  {ADM1278_CURRENT, 800 * ADM1278_RSENSE, 20475, 10},
  {ADM1278_POWER, 6123 * ADM1278_RSENSE, 0, 100},
  {ADM1278_TEMP, 42, 31880, 10},
};

//{SensorName, ID, FUNCTION, PWR_STATUS, {UCR, UNR, UNC, LCR, LNR, LNC, Pos, Neg}
PAL_SENSOR_MAP sensor_map[] = {
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x00
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x01
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x02
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x03
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x04
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x05
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x06
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x07
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x08
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x09
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x0F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x10
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x11
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x12
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x13
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x14
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x15
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x16
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x17
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x18
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x19
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x1F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x20
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x21
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x22
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x23
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x24
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x25
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x26
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x27
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x28
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x29
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x2F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x30
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x31
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x32
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x33
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x34
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x35
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x36
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x37
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x38
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x39
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x3F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x40
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x41
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x42
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x43
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x44
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x45
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x46
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x47
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x48
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x49
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x4F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x50
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x51
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x52
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x53
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x54
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x55
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x56
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x57
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x58
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x59
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x5F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x60
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x61
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x62
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x63
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x64
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x65
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x66
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x67
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x68
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x69
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x6F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x70
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x71
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x72
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x73
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x74
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x75
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x76
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x77
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x78
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x79
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x7F

  {"BMC_INLET_TEMP",  TEMP_INLET,  read_temp, true, {40, 0, 0, 20, 0, 0, 0, 0}, TEMP}, //0x80
  {"BMC_OUTLET_TEMP", TEMP_OUTLET, read_temp, true, {80, 0, 0, 20, 0, 0, 0, 0}, TEMP}, //0x81
  {"NIC_SENSOR_MEZZ_TEMP", TEMP_MEZZ, read_temp, true, {0, 0, 0, 0, 0, 0, 0, 0}, TEMP}, //0x82
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x83
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x84
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x85
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x86
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x87
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x88
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x89
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x8A
  {"BMC_SENSOR_P5V", ADC0, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x8B
  {"BMC_SENSOR_P12V", ADC1, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x8C
  {"BMC_SENSOR_P3V3_STBY", ADC2, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x8D
  {"BMC_SENSOR_P1V15_STBY", ADC3, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x8E
  {"BMC_SENSOR_P1V2_STBY", ADC4, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x8F

  {"BMC_SENSOR_P2V5_STBY", ADC5, read_adc_val, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x90
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x91
  {"BMC_SENSOR_HSC_IN_VOLT", 0, read_hsc_vin, true, {0, 0, 0, 0, 0, 0, 0, 0}, VOLT}, //0x92
  {"BMC_SENSOR_HSC_TEMP", 0, read_hsc_temp, true, {0, 0, 0, 0, 0, 0, 0, 0}, TEMP}, //0x93
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x94
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x95
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x96
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x97
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x98
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x99
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9A
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9B
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9C
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9D
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9E
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0x9F

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xA9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAD
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xAF

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0XB2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xB9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBD
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xBF

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xC9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCD
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xCF

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xD9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDD
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xDF

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xE9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xEA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xEB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xEC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xED
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xEE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xEF

  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF0
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF1
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF2
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF3
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF4
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF5
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF6
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF7
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF8
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xF9
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFA
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFB
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFC
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFD
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFE
  {NULL, 0, NULL, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 0}, //0xFF
};

//HSC
PAL_HSC_INFO hsc_info_list[] = {
  {HSC_ID0, ADM1278_SLAVE_ADDR, adm1278_info_list},
};

size_t bmc_sensor_cnt = sizeof(bmc_sensor_list)/sizeof(uint8_t);
size_t nic_sensor_cnt = sizeof(nic_sensor_list)/sizeof(uint8_t);
size_t bic_sensor_cnt = sizeof(bic_sensor_list)/sizeof(uint8_t);

int
pal_get_fru_sensor_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  switch(fru) {
  case FRU_BMC:
    *sensor_list = (uint8_t *) bmc_sensor_list;
    *cnt = bmc_sensor_cnt;
    break;
  case FRU_NIC:
    *sensor_list = (uint8_t *) nic_sensor_list;
    *cnt = nic_sensor_cnt;
    break;
  case FRU_SLOT1:
  case FRU_SLOT2:
  case FRU_SLOT3:
  case FRU_SLOT4:
    *sensor_list = (uint8_t *) bic_sensor_list;
    *cnt = bic_sensor_cnt;
    break;
  default:
    if (fru > MAX_NUM_FRUS)
      return -1;
    // Nothing to read yet.
    *sensor_list = NULL;
    *cnt = 0;
  }
  return 0;
}

int
pal_get_fru_discrete_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  switch(fru) {
    case FRU_BMC:
    default:
      if (fru > MAX_NUM_FRUS)
        return -1;
      // Nothing to read yet.
      *sensor_list = NULL;
      *cnt = 0;
  }
    return 0;
}

uint8_t
pal_get_fan_source(uint8_t fan_num) {

  switch (fan_num)
  {
    case FAN_0:
    case FAN_1:
      return PWM_0;

    case FAN_2:
    case FAN_3:
      return PWM_1;

    case FAN_4:
    case FAN_5:
      return PWM_2;

    case FAN_6:
    case FAN_7:
      return PWM_3;

    default:
      syslog(LOG_WARNING, "[%s] Catch unknown fan number - %d\n", __func__, fan_num);
      break;
  }

  return 0xff;
}

int pal_set_fan_speed(uint8_t fan, uint8_t pwm)
{
  char label[32] = {0};
  uint8_t pwm_num = fan;

  if (pwm_num > pal_pwm_cnt ||
      snprintf(label, sizeof(label), "pwm%d", pwm_num + 1) > sizeof(label)) {
    return -1;
  }
  return sensors_write_fan(label, (float)pwm);
}

int pal_get_fan_speed(uint8_t fan, int *rpm)
{
  char label[32] = {0};
  float value;
  int ret;

  if (fan > pal_tach_cnt ||
      snprintf(label, sizeof(label), "fan%d", fan + 1) > sizeof(label)) {
    syslog(LOG_WARNING, "%s: invalid fan#:%d", __func__, fan);
    return -1;
  }
  ret = sensors_read_fan(label, &value);
  *rpm = (int)value;
  return ret;
}

int pal_get_fan_name(uint8_t num, char *name)
{
  if (num > pal_tach_cnt) {
    syslog(LOG_WARNING, "%s: invalid fan#:%d", __func__, num);
    return -1;
  }

  sprintf(name, "Fan %d",num);

  return 0;
}

int pal_get_pwm_value(uint8_t fan, uint8_t *pwm)
{
  char label[32] = {0};
  float value;
  int ret;
  uint8_t fan_src = 0;

  fan_src = pal_get_fan_source(fan);

  if (fan >= pal_tach_cnt || fan_src == 0xff ||
      snprintf(label, sizeof(label), "pwm%d", fan_src + 1) > sizeof(label)) {
    syslog(LOG_WARNING, "%s: invalid fan#:%d", __func__, fan);
    return -1;
  }

  ret = sensors_read_fan(label, &value);
  if (ret == 0)
    *pwm = (int)value;
  return ret;
}

static int
read_temp(uint8_t id, float *value) {
  struct {
    const char *chip;
    const char *label;
  } devs[] = {
    {"lm75-i2c-12-4e",  "BMC_INLET_TEMP"},
    {"lm75-i2c-12-4f",  "BMC_OUTLET_TEMP"},
    {"tmp421-i2c-8-1f", "NIC_SENSOR_MEZZ_TEMP"},
  };
  if (id >= ARRAY_SIZE(devs)) {
    return -1;
  }

  return sensors_read(devs[id].chip, devs[id].label, value);
}

static int
read_adc_val(uint8_t adc_id, float *value) {
  const char *adc_label[] = {
    "BMC_SENSOR_P5V",
    "BMC_SENSOR_P12V",
    "BMC_SENSOR_P3V3_STBY",
    "BMC_SENSOR_P1V15_STBY",
    "BMC_SENSOR_P1V2_STBY",
    "BMC_SENSOR_P2V5_STBY",
  };
  if (adc_id >= ARRAY_SIZE(adc_label)) {
    return -1;
  }
  return sensors_read_adc(adc_label[adc_id], value);
}

static void
get_adm1278_info(uint8_t hsc_id, uint8_t type, uint8_t *addr, float* m, float* b, float* r) {

 *addr = hsc_info_list[hsc_id].slv_addr;
 *m = hsc_info_list[hsc_id].info[type].m;
 *b = hsc_info_list[hsc_id].info[type].b;
 *r = hsc_info_list[hsc_id].info[type].r;

 return;
}

static int
read_hsc_temp(uint8_t hsc_id, float *value) {
  uint8_t tbuf[1] = {0x00};
  uint8_t rbuf[2] = {0x00};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  uint8_t addr = hsc_info_list[hsc_id].slv_addr;
  float m, b, r;
  int retry = 3;
  int ret = -1;
  int fd = 0;

  fd = open("/dev/i2c-11", O_RDWR);
  if (fd < 0) {
    syslog(LOG_WARNING, "Failed to open bus 11");
    goto error_exit;
  }

  get_adm1278_info(HSC_ID0, ADM1278_TEMP, &addr, &m, &b, &r);

  tbuf[0] = PMBUS_READ_TEMP1;
  tlen = 1;
  rlen = 2;

  while ( ret < 0 && retry-- > 0 ) {
    ret = i2c_rdwr_msg_transfer(fd, addr, tbuf, tlen, rbuf, rlen);
  }

  if ( ret < 0 ) {
    ret = READING_NA;
    goto error_exit;
  }

  *value = ((float)(rbuf[1] << 8 | rbuf[0]) * r - b) / m;
error_exit:
  if ( fd > 0 ) close(fd);

  return ret;
}

static int
read_hsc_vin(uint8_t hsc_id, float *value) {
  uint8_t tbuf[1] = {0x00};
  uint8_t rbuf[2] = {0x00};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  uint8_t addr = hsc_info_list[hsc_id].slv_addr;
  float m, b, r;
  int retry = 3;
  int ret = -1;
  int fd = 0;

  fd = open("/dev/i2c-11", O_RDWR);
  if (fd < 0) {
    syslog(LOG_WARNING, "Failed to open bus 11");
    goto error_exit;
  }
  
  get_adm1278_info(HSC_ID0, ADM1278_VOLTAGE, &addr, &m, &b, &r);

  tbuf[0] = PMBUS_READ_VIN;
  tlen = 1;
  rlen = 2;

  while ( ret < 0 && retry-- > 0 ) {
    ret = i2c_rdwr_msg_transfer(fd, addr, tbuf, tlen, rbuf, rlen);
  }

  if ( ret < 0 ) {
    ret = READING_NA;
    goto error_exit;
  }

  *value = ((float)(rbuf[1] << 8 | rbuf[0]) * r - b) / m;
error_exit:
  if ( fd > 0 ) close(fd);

  return ret;
}

static int
pal_bic_sensor_read_raw(uint8_t fru, uint8_t sensor_num, float *value){
#define BIC_SENSOR_READ_NA 0x20
  int ret = 0;
  uint8_t power_status = 0;
  ipmi_sensor_reading_t sensor = {0};
  sdr_full_t *sdr = NULL;

  if ( bic_get_server_power_status(fru, &power_status) < 0 || power_status != SERVER_POWER_ON) {
    //syslog(LOG_WARNING, "%s() Failed to run bic_get_server_power_status(). fru%d, snr#0x%x, pwr_sts:%d", __func__, fru, sensor_num, power_status);
    return READING_NA;
  }

  if ( (bic_is_m2_exp_prsnt(fru) == 1 || bic_is_m2_exp_prsnt(fru) == 3) && (sensor_num >= 0x40 && sensor_num <= 0x7F)) { // 1OU Exp
    ret = bic_get_sensor_reading(fru, sensor_num, &sensor, FEXP_BIC_INTF);
  } else if ( (bic_is_m2_exp_prsnt(fru) == 2 || bic_is_m2_exp_prsnt(fru) == 3) && (sensor_num >= 0x80 && sensor_num <= 0xBF)) { // 2OU Exp
    ret = bic_get_sensor_reading(fru, sensor_num, &sensor, REXP_BIC_INTF);
  } else {
    ret = bic_get_sensor_reading(fru, sensor_num, &sensor, NONE_INTF);
  }
  if ( ret < 0 ) {
    syslog(LOG_WARNING, "%s() Failed to run bic_get_sensor_reading(). snr#0x%x", __func__, sensor_num);
    return READING_NA;
  }

  if (sensor.flags & BIC_SENSOR_READ_NA) {
    //syslog(LOG_WARNING, "%s() sensor@0x%x flags is NA", __func__, sensor_num);
    return READING_NA;
  }

  sdr = &g_sinfo[fru-1][sensor_num].sdr;
  //syslog(LOG_WARNING, "%s() fru %x, sensor_num:0x%x, val:0x%x, type: %x", __func__, fru, sensor_num, sensor.value, sdr->type);
  if ( sdr->type != 1 ) {
    *value = sensor.value;
    return 0;
  } 

  // y = (mx + b * 10^b_exp) * 10^r_exp
  int x;
  uint8_t m_lsb, m_msb;
  uint16_t m = 0;
  uint8_t b_lsb, b_msb;
  uint16_t b = 0;
  int8_t b_exp, r_exp;
  x = sensor.value;

  m_lsb = sdr->m_val;
  m_msb = sdr->m_tolerance >> 6;
  m = (m_msb << 8) | m_lsb;

  b_lsb = sdr->b_val;
  b_msb = sdr->b_accuracy >> 6;
  b = (b_msb << 8) | b_lsb;

  // exponents are 2's complement 4-bit number
  b_exp = sdr->rb_exp & 0xF;
  if (b_exp > 7) {
    b_exp = (~b_exp + 1) & 0xF;
    b_exp = -b_exp;
  }

  r_exp = (sdr->rb_exp >> 4) & 0xF;
  if (r_exp > 7) {
    r_exp = (~r_exp + 1) & 0xF;
    r_exp = -r_exp;
  }

  //syslog(LOG_WARNING, "%s() snr#0x%x raw:%x m=%x b=%x b_exp=%x r_exp=%x", __func__, sensor_num, x, m, b, b_exp, r_exp);  
  *value = ((m * x) + (b * pow(10, b_exp))) * (pow(10, r_exp));

  return ret;
}

int
pal_sensor_read_raw(uint8_t fru, uint8_t sensor_num, void *value) {
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char fru_name[32];
  int ret=0;
  uint8_t id=0;

  pal_get_fru_name(fru, fru_name);
  sprintf(key, "%s_sensor%d", fru_name, sensor_num);
  id = sensor_map[sensor_num].id;

  switch(fru) {
    case FRU_SLOT1:
    case FRU_SLOT2:
    case FRU_SLOT3:
    case FRU_SLOT4:
      ret = pal_bic_sensor_read_raw(fru, sensor_num, (float*)value);
      break;
    case FRU_BMC:
    case FRU_NIC:
      ret = sensor_map[sensor_num].read_sensor(id, (float*) value);
      break;
      
    default:
      return -1;
  }

  if (ret) {
    if (ret == READING_NA || ret == -1) {
      strcpy(str, "NA");
    } else {
      return ret;
    }
  } else {
    sprintf(str, "%.2f",*((float*)value));
  }
  if(kv_set(key, str, 0, 0) < 0) {
    syslog(LOG_WARNING, "pal_sensor_read_raw: cache_set key = %s, str = %s failed.", key, str);
    return -1;
  } else {
    return ret;
  }

  return 0;
}

int
pal_get_sensor_name(uint8_t fru, uint8_t sensor_num, char *name) {
  switch(fru) {
    case FRU_BMC:
    case FRU_NIC:
      sprintf(name, "%s", sensor_map[sensor_num].snr_name);
      break;
    default:
      return -1;
  }
  return 0;
}

int
pal_get_sensor_threshold(uint8_t fru, uint8_t sensor_num, uint8_t thresh, void *value) {
  float *val = (float*) value;
  *val = 0;
  return 0;
}

int
pal_sensor_sdr_path(uint8_t fru, char *path) {
  char fru_name[16] = {0};

  switch(fru) {
    case FRU_SLOT1:
      sprintf(fru_name, "%s", "slot1");
    break;
    case FRU_SLOT2:
      sprintf(fru_name, "%s", "slot2");
    break;
    case FRU_SLOT3:
      sprintf(fru_name, "%s", "slot3");
    break;
    case FRU_SLOT4:
      sprintf(fru_name, "%s", "slot4");
    break;
    case FRU_BMC:
      sprintf(fru_name, "%s", "bmc");
    break;
    case FRU_NIC:
      sprintf(fru_name, "%s", "nic");
    break;

    default:
      syslog(LOG_WARNING, "%s() Wrong fru id %d", __func__, fru);
    return -1;
  }

  sprintf(path, SDR_PATH, fru_name);
  if (access(path, F_OK) == -1) {
    return -1;
  }

  return 0;
}

static int
_sdr_init(char *path, sensor_info_t *sinfo) {
  int fd;
  int ret = 0;
  uint8_t buf[MAX_SDR_LEN] = {0};
  uint8_t bytes_rd = 0;
  uint8_t snr_num = 0;
  sdr_full_t *sdr;
  int retry = 3;

  while ( access(path, F_OK) == -1 && retry > 0 ) {
    syslog(LOG_WARNING, "%s() Failed to access %s, wait a second.\n", __func__, path);
    retry--;
    sleep(3);
  }

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    syslog(LOG_WARNING, "%s() Failed to open %s after retry 3 times\n", __func__, path);
    goto error_exit;
  }

  ret = pal_flock_retry(fd);
  if (ret == -1) {
    syslog(LOG_WARNING, "%s() Failed to flock on %s", __func__, path);
    goto error_exit;
  }

  while ((bytes_rd = read(fd, buf, sizeof(sdr_full_t))) > 0) {
    if (bytes_rd != sizeof(sdr_full_t)) {
      syslog(LOG_WARNING, "%s() read returns %d bytes\n", __func__, bytes_rd);
      goto error_exit;
    }

    sdr = (sdr_full_t *) buf;
    snr_num = sdr->sensor_num;
    sinfo[snr_num].valid = true;
    memcpy(&sinfo[snr_num].sdr, sdr, sizeof(sdr_full_t));
    //syslog(LOG_WARNING, "%s() copy num: 0x%x:%s success", __func__, snr_num, sdr->str);
  }

error_exit:

  if ( fd > 0 ) {
    if ( pal_unflock_retry(fd) < 0 ) syslog(LOG_WARNING, "%s() Failed to unflock on %s", __func__, path);
    close(fd);
  }

  return ret;
}

static bool
pal_is_sdr_init(uint8_t fru) {
  return sdr_init_done[fru - 1];
}

static void
pal_set_sdr_init(uint8_t fru, bool set) {
  sdr_init_done[fru - 1] = set;
}

int
pal_sensor_sdr_init(uint8_t fru, sensor_info_t *sinfo) {
  char path[64] = {0};
  int retry = 3;
  int ret = 0;

  //syslog(LOG_WARNING, "%s() pal_is_sdr_init  bool %d, fru %d, snr_num: %x\n", __func__, pal_is_sdr_init(fru), fru, g_sinfo[fru-1][1].sdr.sensor_num);
  if ( true == pal_is_sdr_init(fru) ) {
    memcpy(sinfo, g_sinfo[fru-1], sizeof(sensor_info_t) * MAX_SENSOR_NUM); 
    goto error_exit;
  }

  ret = pal_sensor_sdr_path(fru, path);
  if ( ret < 0 ) {
    //syslog(LOG_WARNING, "%s() Failed to run pal_sensor_sdr_path\n", __func__);
    goto error_exit;
  }
  
  while ( retry-- > 0 ) {
    ret = _sdr_init(path, sinfo);
    if ( ret < 0 ) {
      syslog(LOG_WARNING, "%s() Failed to run _sdr_init, retry..%d\n", __func__, retry);
      sleep(1);
      continue;
    } else {
      memcpy(g_sinfo[fru-1], sinfo, sizeof(sensor_info_t) * MAX_SENSOR_NUM);
      pal_set_sdr_init(fru, true);
      break;
    }
  }

error_exit:
  return ret;
}

int
pal_sdr_init(uint8_t fru) {

  if ( false == pal_is_sdr_init(fru) ) {

    sensor_info_t *sinfo = g_sinfo[fru-1];

    if (pal_sensor_sdr_init(fru, sinfo) < 0) {
      //syslog(LOG_WARNING, "%s() Failed to run pal_sensor_sdr_init fru%d\n", __func__, fru);
      return ERR_NOT_READY;
    }

    pal_set_sdr_init(fru, true);
  }

  return 0;
}

int
pal_get_sensor_units(uint8_t fru, uint8_t sensor_num, char *units) {
  int ret = 0;
  uint8_t scale = sensor_map[sensor_num].units;

  if (fru == FRU_SLOT1 || fru == FRU_SLOT2 || \
      fru == FRU_SLOT3 || fru == FRU_SLOT4) {
    ret = pal_sdr_init(fru);
    strcpy(units, "");
    return ret;
  }

  switch(scale) {
    case TEMP:
      sprintf(units, "C");
      break;
    case FAN:
      sprintf(units, "RPM");
      break;
    case VOLT:
      sprintf(units, "Volts");
      break;
    case CURR:
      sprintf(units, "Amps");
      break;
    case POWER:
      sprintf(units, "Watts");
      break;
    default:
      syslog(LOG_WARNING, "%s() unknown sensor number %x", __func__, sensor_num);
    break;
  }
  return 0;
}


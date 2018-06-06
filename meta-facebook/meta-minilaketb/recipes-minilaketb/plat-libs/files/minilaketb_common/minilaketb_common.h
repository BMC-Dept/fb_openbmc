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

#ifndef __MINILAKETB_COMMON_H__
#define __MINILAKETB_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_FRUS 2
enum {
  FRU_ALL   = 0,
  FRU_SLOT1 = 1,
  FRU_SPB   = 2,
};

enum {
  SLOT_TYPE_SERVER = 0,
  SLOT_TYPE_CF     = 1,
  SLOT_TYPE_GP     = 2,
  SLOT_TYPE_NULL   = 3,
};

enum {
  IPMB_BUS_SLOT1 = 2,
};

enum {
  PCIE_CONFIG_4xTL      = 0x00,
  PCIE_CONFIG_2xCF_2xTL = 0x11,
  PCIE_CONFIG_2xGP_2xTL = 0x22,
};

enum {
  TYPE_SV_A_SV     = 0,
  TYPE_CF_A_SV     = 1,
  TYPE_GP_A_SV     = 2,
  TYPE_NULL_A_SV   = 3,
  TYPE_SV_A_CF     = 4,
  TYPE_CF_A_CF     = 5,
  TYPE_GP_A_CF     = 6,
  TYPE_NULL_A_CF   = 7,
  TYPE_SV_A_GP     = 8,
  TYPE_CF_A_GP     = 9,
  TYPE_GP_A_GP     = 10,
  TYPE_NULL_A_GP   = 11,
  TYPE_SV_A_NULL   = 12,
  TYPE_CF_A_NULL   = 13,
  TYPE_GP_A_NULL   = 14,
};

typedef struct {
  char slot_key[32];
  char slot_def_val[32];
} slot_kv_st;


// Structure for Accuracy Sensor Reading (Bridge IC spec v0.6)
typedef struct {
  uint8_t int_value;
  uint8_t dec_value;
  uint8_t flags;
} ipmi_accuracy_sensor_reading_t;

//GPIO definition
#define MAX_SPB_GPIO_NUM                  256

#define GPIO_BMC_READY_N                    0
#define GPIO_PE_BUFF_OE_0_R_N              12
#define GPIO_PE_BUFF_OE_1_R_N              13
#define GPIO_PE_BUFF_OE_2_R_N              14
#define GPIO_PE_BUFF_OE_3_R_N              15
#define GPIO_PWR_BTN                       24
#define GPIO_PWR_SLOT1_BTN_N               25
#define GPIO_UART_SEL0                     32
#define GPIO_UART_SEL1                     33
#define GPIO_UART_SEL2                     34
#define GPIO_UART_RX                       35
#define GPIO_SYSTEM_ID1_LED_N              40
#define GPIO_POSTCODE_0                    48
#define GPIO_POSTCODE_1                    49
#define GPIO_POSTCODE_2                    50
#define GPIO_POSTCODE_3                    51
#define GPIO_FAN_LATCH_DETECT              61
#define GPIO_SLOT1_POWER_EN                64
#define GPIO_CLK_BUFF1_PWR_EN_N            72
#define GPIO_CLK_BUFF2_PWR_EN_N            73
#define GPIO_VGA_SW0                       74
#define GPIO_VGA_SW1                       75
#define GPIO_PWR1_LED                      96

// change I2C_SLOT1_ALERT_N form GPION2(106) to GPIOB0
#define GPIO_I2C_SLOT1_ALERT_N            8
// change GPIO_P12V_STBY_SLOT1_EN  from GPIOO4 to GPIOZ0
#define GPIO_P12V_STBY_SLOT1_EN           200
// END : GPIO_P12V_STBY_SLOT1_EN  from GPIOO4 to GPIOZ0

#define GPIO_POSTCODE_4                   124
#define GPIO_POSTCODE_5                   125
#define GPIO_POSTCODE_6                   126
#define GPIO_POSTCODE_7                   127
/*change DBG_CARD_PRSNT form GPIOR3 to GPIOE4*/
#define GPIO_DBG_CARD_PRSNT               36
#define GPIO_RST_SLOT1_SYS_RESET_N        144
#define GPIO_BOARD_REV_ID0                192
#define GPIO_BOARD_REV_ID1                193
#define GPIO_BOARD_REV_ID2                194
#define GPIO_BOARD_ID                     195

#define GPIO_HAND_SW_ID1                  212
#define GPIO_HAND_SW_ID2                  213
#define GPIO_HAND_SW_ID4                  214
#define GPIO_HAND_SW_ID8                  215
#define GPIO_RST_BTN                      216
#define GPIO_BMC_SELF_HW_RST              218
#define GPIOAB4_RESERVED_PIN              220 //GPIOAB4 is reserved and could not be used
#define GPIOAB5_RESERVED_PIN              221 //GPIOAB5 is reserved and could not be used
#define GPIOAB6_RESERVED_PIN              222 //GPIOAB6 is reserved and could not be used
#define GPIOAB7_RESERVED_PIN              223 //GPIOAB7 is reserved and could not be used


int minilaketb_common_fru_name(uint8_t fru, char *str);
int minilaketb_common_fru_id(char *str, uint8_t *fru);
int minilaketb_common_crashdump(uint8_t fru);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __MINILAKETB_COMMON_H__ */

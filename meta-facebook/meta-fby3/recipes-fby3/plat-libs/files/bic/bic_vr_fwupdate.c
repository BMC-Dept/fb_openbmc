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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bic_vr_fwupdate.h"
#include "bic_ipmi.h"

#define DEBUG

/****************************/
/*       VR fw update       */                            
/****************************/
#define MAX_RETRY 3
#define VR_BUS 0x8

typedef struct {
  uint8_t command;
  uint8_t data_len;
  uint8_t data[16];
} vr_data;

typedef struct {
  uint8_t addr;
  uint8_t devid[6];
  uint8_t devid_len;
  int data_cnt;
  vr_data pdata[2048];
} vr;

static int vr_cnt = 0;

//4 VRs are on the server board
static vr vr_list[4] = {0};

static uint8_t
cal_crc8(uint8_t crc, uint8_t const *data, uint8_t len)
{
  uint8_t const crc8_table[] = 
  { 0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3};

  if ( NULL == data )
  {
    return 0;
  }

  crc &= 0xff;
  uint8_t const *end = data + len;

  while ( data < end )
  {
    crc = crc8_table[crc ^ *data++];
  }

  return crc;  
}

static uint8_t 
char_to_hex(char c) {
  if (c >= 'A')
    return c - 'A' + 10;
  else
    return c - '0';
}

static uint8_t 
string_to_byte(char *c) {
  return char_to_hex(c[0]) * 16 + char_to_hex(c[1]);
}

static int
start_with(const char *str, const char *c) {
  int len = strlen(c);
  int i;

  for ( i=0; i<len; i++ )
  {
    if ( str[i] != c[i] )
    {
       return 0;
    }
  }
  return 1;
}

static int
vr_ISL_polling_status(uint8_t slot_id, uint8_t addr) {
  int ret = 0;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};
  uint8_t tlen = 0;
  uint8_t rlen = 0;

  tbuf[0] = (VR_BUS << 1) + 1;
  tbuf[1] = addr;
  tbuf[2] = 0x00; //read cnt
  tbuf[3] = 0xC7; //command code
  tbuf[4] = 0x07; //data0
  tbuf[5] = 0x07; //data1
  tlen = 6;
  ret = bic_ipmb_send(slot_id, NETFN_APP_REQ, CMD_APP_MASTER_WRITE_READ, tbuf, tlen, rbuf, &rlen, NONE_INTF);
  if ( ret < 0 ) {
    syslog(LOG_WARNING, "Failed to send PROGRAMMER_STATUS command");
    goto error_exit;
  }

  tbuf[2] = 0x04; //read cnt
  tbuf[3] = 0xC5; //command code
  tlen = 4;
  ret = bic_ipmb_send(slot_id, NETFN_APP_REQ, CMD_APP_MASTER_WRITE_READ, tbuf, tlen, rbuf, &rlen, NONE_INTF);
  if ( ret < 0 ) {
    syslog(LOG_WARNING, "Failed to get PROGRAMMER_STATUS");
    goto error_exit;
  }

  return rbuf[3] & 0x1;

error_exit:

  return ret;
}

static int
vr_ISL_program(uint8_t slot_id, uint8_t addr, vr_data *list, int len) {
  int i = 0;
  int ret = 0;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  int retry = MAX_RETRY;

  tbuf[0] = (VR_BUS << 1) + 1;
  tbuf[1] = addr;
  tbuf[2] = 0x00; //read cnt

  for ( i=0; i<len; i++ ) {

    //prepare data
    tbuf[3] = list[i].command ;//command code
    memcpy(&tbuf[4], list[i].data, list[i].data_len);
    tlen = 4 + list[i].data_len;
    ret = bic_ipmb_send(slot_id, NETFN_APP_REQ, CMD_APP_MASTER_WRITE_READ, tbuf, tlen, rbuf, &rlen, NONE_INTF);
    if ( ret < 0 ) {
      syslog(LOG_WARNING, "Failed to send data...%d", i);
      break;
    }

    //check the status
    retry = MAX_RETRY;
    do {
      if ( vr_ISL_polling_status(slot_id, addr) == 1 ) {
        break;
      } else {
        retry--;
        msleep(500);
      }
    } while ( retry > 0 );

    if ( retry == 0 ) {
      printf("Failed to send data...%d to add 0x%x after retry %d times\n", i, addr, MAX_RETRY);
      ret = -1;
      break;
    }    
  } 
  
  return ret;
}

static int 
vr_ISL_hex_parser(char *image) {
  int ret = 0;
  FILE *fp = NULL;
  char tmp_buf[64] = "\0";
  int tmp_len = sizeof(tmp_buf);
  int i = 0, j = 0;
  int data_cnt = 0;
  uint8_t addr = 0;
  uint8_t cnt = 0;
  uint8_t crc8 = 0;
  uint8_t crc8_check = 0;
  uint8_t data[16] = {0};
  uint8_t data_end = 0;
  
  if ( (fp = fopen(image, "r") ) == NULL ) {
    printf("Invalid file: %s\n", image);
    ret = -1;
    goto error_exit;
  }
 
  while( NULL != fgets(tmp_buf, tmp_len, fp) ) {
    /*search for 0xAD command*/
    if ( start_with(tmp_buf, "49") ) {
      if ( string_to_byte(&tmp_buf[6]) == 0xad ) {
          if (vr_list[vr_cnt].addr != 0x0 ) vr_cnt++; //move on to the next addr if 4 VRs are included a file
        vr_list[vr_cnt].addr = string_to_byte(&tmp_buf[4]);
        vr_list[vr_cnt].devid[0] = string_to_byte(&tmp_buf[14]);
        vr_list[vr_cnt].devid[1] = string_to_byte(&tmp_buf[12]);
        vr_list[vr_cnt].devid[2] = string_to_byte(&tmp_buf[10]);
        vr_list[vr_cnt].devid[3] = string_to_byte(&tmp_buf[8]);
        vr_list[vr_cnt].devid_len = 4;
#ifdef DEBUG
        printf("[Get] ID ");
        for ( i = 0; i < vr_list[vr_cnt].devid_len; i++ ) {
          printf("%x ", vr_list[vr_cnt].devid[i]); 
        }
        printf(",Addr %x\n", vr_list[vr_cnt].addr);
#endif
      }
    } else if ( start_with(tmp_buf, "00") ) {
      //initialize the metadata
      j = 0;
      data_cnt = vr_list[vr_cnt].data_cnt;
      cnt = string_to_byte(&tmp_buf[2]);
      addr = string_to_byte(&tmp_buf[4]);
      data_end = cnt * 2;
      crc8 = string_to_byte(&tmp_buf[data_end+2]);
      data[j++] = addr; //put addr to buffer to calculate crc8

      //printf("cnt %x, addr %x, data_end %x, crc8 %x\n", cnt, addr, data_end, crc8);
      if ( addr != vr_list[vr_cnt].addr ) {
        printf("[%s] Failed to parse this line since the addr is not match. 0x%x != 0x%x\n", __func__, addr, vr_list[vr_cnt].addr);
        printf("[%s] %s\n", __func__, tmp_buf);
        ret = -1;
        break;
      }

      //get the data
      for ( i = 6; i <= data_end; i += 2, j++ ) {
        data[j] = string_to_byte(&tmp_buf[i]);
      }

      //calculate crc8
      crc8_check = 0;
      crc8_check = cal_crc8(crc8_check, data, cnt-1);

      if ( crc8_check != crc8 ) {
        printf("[%s] CRC8 is not match. Expected CRC8: 0x%x, Acutal CRC8: 0x%x\n", __func__, crc8, crc8_check);
        ret = -1;
        break;
      }

      vr_list[vr_cnt].pdata[data_cnt].command = data[1];
      vr_list[vr_cnt].pdata[data_cnt].data_len = cnt - 3;
      memcpy(vr_list[vr_cnt].pdata[data_cnt].data, &data[2], vr_list[vr_cnt].pdata[data_cnt].data_len);
#ifdef DEBUG
      printf(" cmd: %x, data_len: %x, data: ", vr_list[vr_cnt].pdata[data_cnt].command, vr_list[vr_cnt].pdata[data_cnt].data_len);
      for ( i = 0; i < vr_list[vr_cnt].pdata[data_cnt].data_len; i++){
        printf("%x ", vr_list[vr_cnt].pdata[data_cnt].data[i]);
      }
      printf("\n");
#endif
      vr_list[vr_cnt].data_cnt++;
    } 
  }

#ifdef DEBUG
  printf("\n\n");
  for ( i = 0; i < vr_list[vr_cnt].data_cnt; i++) {
    printf(" cmd: %x, data_len: %x, data: ", vr_list[vr_cnt].pdata[i].command, vr_list[vr_cnt].pdata[i].data_len);
    for ( j = 0; j < vr_list[vr_cnt].pdata[i].data_len; j++)
      printf("%x ", vr_list[vr_cnt].pdata[i].data[j]);
    printf("\n");
  }
#endif

error_exit:
  if ( fp != NULL ) fclose(fp);

  return ret;
}

static char* get_vr_name(uint8_t addr) {
  struct dev_table {
    uint8_t addr;
    char *dev_name;
  } dev_list[] = {
    {0xC0, "VCCIN/VSA"},
    {0xC4, "VCCIO"},
    {0xC8, "VDDQ_ABC"},
    {0xCC, "VDDQ_DEF"},
  };

  int dev_table_size = (sizeof(dev_list)/sizeof(struct dev_table));
  int i;

  for ( i = 0; i< dev_table_size; i++ ) {
    if ( addr == dev_list[i].addr ) {
      return dev_list[i].dev_name;
    }
  }
  return NULL;
}

int 
update_bic_vr(uint8_t slot_id, char *image, uint8_t force) {
  int ret = 0;
  int i = 0;
  uint8_t rbuf[6] = {0};
  uint8_t rlen = 0;

  //step 1 - read the dev id of one of them.
  ret = bic_get_vr_device_id(slot_id, FW_CPLD, rbuf, &rlen, 8 /*bus*/, 0xC0/*addr*/, NONE_INTF);
  if ( ret < 0 ){
    syslog(LOG_WARNING, "[%s] Failed to do bic_get_vr_device_id()", __func__);
    goto error_exit;
  }

  //step 2 - parse the image file.
  //The length of GET_DEVICE_ID of ISL is different to TI.
  if ( rlen > 4 ) {
    //do nothing
  } else {
    ret = vr_ISL_hex_parser(image);
  }

  //step 3 - check DEVID
  if ( memcmp(vr_list[0].devid, rbuf, vr_list[0].devid_len) != 0 ) {
    printf("[%s] Device ID is not match!\n", __func__);
    printf("[%s] Expected ID: ", __func__);
    for ( i = 0 ; i < vr_list[0].devid_len; i++ ) printf("%02X ", vr_list[0].devid[i]);
    printf("\n");
    printf("[%s] Actual ID: ", __func__);
    for ( i = 0 ; i < vr_list[0].devid_len; i++ ) printf("%02X ", rbuf[i]);
    printf("\n");
    ret = -1;
    goto error_exit;
  }


  //step 4 - program
  for ( i = 0; i < vr_cnt + 1; i++ ) { 
    printf("Update %s...", get_vr_name(vr_list[i].addr));
    ret = vr_ISL_program(slot_id, vr_list[i].addr, vr_list[i].pdata, vr_list[i].data_cnt);
    if ( ret < 0 ) {
      printf("Fail.\n");
    } else {
      printf("Success.\n");
    }
  }
error_exit:

  return ret;
}

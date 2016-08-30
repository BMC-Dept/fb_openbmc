/*
 * (C) Copyright 2004-Present
 * Teddy Reed <reed@fb.com>, Facebook, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __FACEBOOK_CONFIG_H
#define __FACEBOOK_CONFIG_H

/*
 * Verified boot options.
 *
 * These are general feature flags related to verified boot.
 *   CONFIG_SPL_FIT_SIGNATURE: Enforce verified boot code in ROM.
 *   CONFIG_SPL: Use a dual SPI for booting U-Boot.
 */

#ifdef CONFIG_SPL
#define CONFIG_SYS_REMAP_BASE     0x30000000
#define CONFIG_SYS_UBOOT_START    0x30008000 /* Must be defined as-is */
#define CONFIG_SYS_ENV_BASE       0x30000000
#define CONFIG_KERNEL_LOAD         "30080000"
#else
#define CONFIG_SYS_REMAP_BASE     0x00000000
#define CONFIG_SYS_UBOOT_START    0x00000000
#define CONFIG_SYS_ENV_BASE       0x00000000
#define CONFIG_KERNEL_LOAD         "20080000"
#endif

/*
 * Requirements:
 * Before including this common configuration, the board must include
 * the CPU/arch platform configuration.
 */

/*
 * Basic boot command configuration based on flash
 */
#define CONFIG_BOOTCOMMAND        "bootm " CONFIG_KERNEL_LOAD /* Location of FIT */
#define CONFIG_AUTOBOOT_PROMPT		"autoboot in %d seconds (stop with 'Delete' key)...\n"
#define CONFIG_AUTOBOOT_STOP_STR	"\x1b\x5b\x33\x7e"	/* 'Delete', ESC[3~ */
#define CONFIG_AUTOBOOT_KEYED
#define CONFIG_ZERO_BOOTDELAY_CHECK

/*
 * Environment configuration
 * This used to have:
 *   CONFIG_ENV_IS_IN_FLASH
 *   CONFIG_ENV_IS_IN_SPI_FLASH
 *   CONFIG_ENV_IS_NOWHERE
 */
#define CONFIG_ASPEED_WRITE_DEFAULT_ENV
#define CONFIG_ENV_IS_IN_FLASH
#define CONFIG_ENV_OFFSET        0x60000 /* environment starts here  */
#define CONFIG_ENV_SIZE          0x20000 /* # of bytes of env, 128k */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_ADDR          (CONFIG_SYS_ENV_BASE + CONFIG_ENV_OFFSET)
#define CONFIG_EXTRA_ENV_SETTINGS                       \
    "verify=yes\0"                                      \
    "spi_dma=no\0"                                      \
    ""

/*
 * Flash configuration
 * It is possible to run using the SMC and not enable flash
 *   CONFIG_CMD_FLASH
 *   CONFIG_SYS_NO_FLASH
 */

/*
 * Serial configuration
 */
#define CONFIG_SYS_NS16550_MEM32
#define CONFIG_SYS_NS16550_REG_SIZE -4

/*
 * Watchdog timer configuration
 */
#define CONFIG_ASPEED_ENABLE_WATCHDOG
#define CONFIG_ASPEED_WATCHDOG_TIMEOUT	(5*60) /* 5 minutes */

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_HZ     1000
#define CONFIG_SYS_TIMERBASE	AST_TIMER_BASE 	/* use timer 1 */

/*
 * NIC configuration
 */
#define CONFIG_NET_RANDOM_ETHADDR
#define CONFIG_LIB_RAND

/*
 * Memory Test configuration
 */
#define CONFIG_SYS_MEMTEST_ITERATION 10

/*
 * Command configuration
 */
#define CONFIG_CMD_MII
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_DIAG
#define CONFIG_CMD_MEMINFO
#define CONFIG_CMD_MEMTEST
#define CONFIG_CMD_MEMTEST2
#define CONFIG_CMD_SDRAM
#define CONFIG_CMD_FLASH

/*
 * Additional command configuration
 *   CONFIG_CMD_I2C
 *   CONFIG_CMD_EEPROM
 */

#ifdef CONFIG_SPL
/* Define the base address to search for a FIT within the SPL. */
#define CONFIG_SYS_SPL_FIT_BASE     CONFIG_SYS_REMAP_BASE
#define CONFIG_SPL_LOAD_FIT_OFFSET  0x8000

#ifdef CONFIG_SPL_BUILD
/* This is an SPL build */

#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_MAX_FOOTPRINT  0x100000

/* During an SPL build the base is 0x0. */
#define CONFIG_SYS_TEXT_BASE      0x00000000
#define CONFIG_SPL_TEXT_BASE      CONFIG_SYS_TEXT_BASE

/* Grow the stack down from 0x6000 to an expected max of 12kB. */
#define CONFIG_SPL_STACK          (CONFIG_SYS_SRAM_BASE + 0x6000)
#define CONFIG_SYS_INIT_SP_ADDR   CONFIG_SPL_STACK

/* Establish an 'arena' for heap from 0x1000 - 0x3000, 8k */
#define CONFIG_SYS_SPL_MALLOC_START (CONFIG_SYS_SRAM_BASE + 0x1000)
#define CONFIG_SYS_SPL_MALLOC_SIZE  0x2000 /* 8kB */

/* General SPL build feature includes. */
#define CONFIG_SPL_DISPLAY_PRINT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT

/* Verified boot required features. */
#define CONFIG_SPL_CRYPTO_SUPPORT
#define CONFIG_SPL_HASH_SUPPORT
#define CONFIG_SPL_SHA256

/* This will increase binary size by +10kB */
#define CONFIG_FIT_SPL_PRINT
#else
/* This is a U-Boot build */

/* During the U-Boot build the base address is the SPL FIT start address. */
#define CONFIG_SYS_TEXT_BASE    CONFIG_SYS_UBOOT_START
#endif
#endif

#endif /* __FACEBOOK_CONFIG_H */

#!/bin/sh

# Setup Endpoint for NICs, using devmem to read GPIO value since
# the gpio pin is occupied by gpio-monitor, read the value from
# gpioget is not possible.

# Setup slot1 NIC
gpio_val=$(devmem 0x1e780088 8)
# PRSNT_NIC0 is at the 6th bit (GPIOU5), 0 means NIC is present
if [ $((gpio_val & 0x20)) -eq 0 ]; then
    systemctl start setup-nic-endpoint-slot@0.service
fi

# Setup slot2 & 3 NIC
gpio_val=$(devmem 0x1e780020 8)
# PRSNT_NIC1 is at the 1th bit (GPIOE0), 0 means NIC is present
if [ $((gpio_val & 0x01)) -eq 0 ]; then
    systemctl start setup-nic-endpoint-slot@1.service
fi
# PRSNT_NIC2 is at the 2th bit (GPIOE1), 0 means NIC is present
if [ $((gpio_val & 0x02)) -eq 0 ]; then
    systemctl start setup-nic-endpoint-slot@2.service
fi

# Setup slot4 NIC
gpio_val=$(devmem 0x1e780078 8)
# PRSNT_NIC3 is at the 4th bit (GPIOM3), 0 means NIC is present
if [ $((gpio_val & 0x08)) -eq 0 ]; then
    systemctl start setup-nic-endpoint-slot@3.service
fi

# Get EID from all Sentinal Dome BICs
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c0" 6 0x07 0xec 0x80 0x01 0x00 0x00
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c0" 6 0x07 0xec 0x80 0x01 0x00 0x05
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c0" 6 0x07 0xec 0x80 0x01 0x00 0x0a
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c0" 6 0x07 0xec 0x80 0x01 0x00 0x0f

busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c1" 6 0x07 0xec 0x80 0x01 0x00 0x14
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c1" 6 0x07 0xec 0x80 0x01 0x00 0x19
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c1" 6 0x07 0xec 0x80 0x01 0x00 0x1e
busctl call xyz.openbmc_project.MCTP /xyz/openbmc_project/mctp au.com.CodeConstruct.MCTP LearnEndpoint say "mctpi3c1" 6 0x07 0xec 0x80 0x01 0x00 0x23

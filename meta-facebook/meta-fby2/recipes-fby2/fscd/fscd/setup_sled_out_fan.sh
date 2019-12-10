#!/bin/sh

. /usr/local/fbpackages/utils/ast-functions

sv stop fscd

pwm_value=100 

fan0_rpm=`cat /sys/devices/platform/ast_pwm_tacho.0/tacho0_rpm`
fan1_rpm=`cat /sys/devices/platform/ast_pwm_tacho.0/tacho1_rpm`

if [ $fan0_rpm -le "500" ] || [ $fan1_rpm -le "500" ]; then
    # for fan fail condition
    pwm_value=100
else
    sku_type=0
    server_type=0
    for i in `seq 1 1 4`
    do
        tmp_sku=$(get_slot_type $i)
        sku_type=$(($(($tmp_sku << $(($(($i*4)) - 4))))+$sku_type))
        tmp_server=$(get_server_type $i)
        server_type=$(($(($tmp_server << $(($(($i*4)) - 4))))+$server_type))
    done
    if [ "$sku_type" == "1028" ] ; then
        # only for GPv2 case
        pwm_value=90
    elif [ "$sku_type" == "0" ] && [ "$server_type" == "17476" ] ; then
        # onle for FBND case
        pwm_value=80
    else
        pwm_value=100
    fi
fi

logger -p user.warning "SLED not seated, fscd stopped, set fan speed to $pwm_value%"
/usr/local/bin/fan-util --set $pwm_value

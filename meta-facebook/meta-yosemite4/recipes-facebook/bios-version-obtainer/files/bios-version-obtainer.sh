#!/bin/bash

covert_text_string_raw_to_string_array() {
    local text_strings=$1
    # Convert the raw data to a string array
    IFS=' ' read -ra text_strings_array <<< "$text_strings"
    string_array=()
    current_string=""
    detect_null=false
    for i in "${text_strings_array[@]}"; do
        if [[ $i == "0" ]]; then
            if [[ $detect_null == false ]]; then
                string_array+=("$current_string")
                current_string=""
            fi
            detect_null=true
            continue
        fi
        detect_null=false
        curr_char=$(printf "\\x$(printf %x "$((10#$i))")")
        current_string+=$curr_char
    done
    eval "$2=\${string_array[@]}"
}

parse_bios_version_from_response() {
    local response=$1

    # Get the string number of the BIOS version (Offset 05h) defined Section 7.1 - Structure
    # definitions: BIOS Information (Type 0) in DSP0134_3.7.1
    # The response for example:
    # pldmtool: Rx: 00 01 05 00 00 00 00 00 05 00 12 00 00 01 02 00 00 03 00 00 00 00 00
    #               00 00 00 00 00 00 00 00 00 00 00 00 4e 2f 41 00 59 34 30 30 35 00 4e 2f 41 00 00
    # Bit "00 01 05" is the command code of GetSMBIOSStructureByType;
    # Bit "00 00 00 00 00 05" is the fixed part of the GetSMBIOSStructureByType's response;
    # Bit "00 12 00 00 01 02 00..." is BIOS Information (Type 0) in DSP0134_3.7.1, the 01 is the BIOS
    # version string number.
    bios_version_str_index=$(echo "$response" | awk '{sub(/^0/, "", $16); print $16}')

    # Extract the Text Strings from the response, Text Strings are defined in Section 6.1.3
    # in DSP0134_3.7.1
    text_strings_raw=$(echo "$response" | awk '{for(i=38;i<=NF;i++) printf "%s ", sprintf("%d", "0x"$i)}')

    string_array=()
    covert_text_string_raw_to_string_array "$text_strings_raw" string_array

    if [ ${#string_array[@]} -gt "$bios_version_str_index" ]; then
        bios_version_from_bic=${string_array[$bios_version_str_index]}
        eval "$2=$bios_version_from_bic"
    else
        echo "Cannot parse the BIOS version from slot${slot_num} SD BIC."
        echo "Response raw data: $bios_version_from_bic"
    fi
}

for slot_num in {1..8}; do
    get_bios_version="busctl get-property xyz.openbmc_project.Settings  /xyz/openbmc_project/software/host${slot_num}/Sentinel_Dome_bios xyz.openbmc_project.Software.Version Version"
    bios_version=$($get_bios_version | awk '{print $2}' | tr -d \")
    if [ -z "$bios_version" ]; then
        # Send the standard PLDM command `GetSMBIOSStructureByType`` in DSP0246_1.0.1 (PLDM SMBIOS)
        # Check if the response contains the BIOS version start with "59 34" (Y4 in ASIC encoding)
        # The response for example:
        # pldmtool: Rx: 00 01 05 00 00 00 00 00 05 00 12 00 00 01 02 00 00 03 00 00 00 00 00
        #               00 00 00 00 00 00 00 00 00 00 00 00 4e 2f 41 00 59 34 30 30 35 00 4e 2f 41 00 00
        # "59 34 30 30 35" is the bios version in ASIC encoding (i.e., Y4005)
        # If there is no value start with "59 34", it means the SD BIC has no BIOS version.
        sd_bic_eid=$((slot_num * 10))
        bios_version_from_bic=$(pldmtool raw -m $sd_bic_eid -v -d 0x80 0x01 0x05 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 2>&1 | grep "59 34")
        if [ -n "$bios_version_from_bic" ]; then
            parsed_bios_version=""
            parse_bios_version_from_response "$bios_version_from_bic" parsed_bios_version
            if [ -n "$parsed_bios_version" ]; then
                echo "Found the BIOS version from slot${slot_num} SD BIC: $parsed_bios_version, update the BIOS version to Settingsd."
                # Set the BIOS version to Settingsd
                busctl set-property xyz.openbmc_project.Settings  /xyz/openbmc_project/software/host${slot_num}/Sentinel_Dome_bios xyz.openbmc_project.Software.Version Version s "$parsed_bios_version"
            fi
        fi
    fi
done

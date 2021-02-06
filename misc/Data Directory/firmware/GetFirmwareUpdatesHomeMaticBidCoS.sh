#!/bin/bash
SCRIPTDIR="$( cd "$(dirname $0)" && pwd )"
FIRMWAREDIR=/tmp/HomegearTemp
DEVICE_DESCRIPTION_DIR="/opt/Homegear-HomeMaticBidCoS/misc/Device Description Files"
LOG_FILE="$SCRIPTDIR"/firmware_download.log
FIRMWARE_ERROR_SIZE=20000

rm -f "$SCRIPTDIR"/0000.*
echo "download started at `date +%Y-%m-%d_%H:%M:%S`" > "$LOG_FILE"
readarray -t FIRMWARES < <(curl -s 'https://update.homematic.com/firmware/api/firmware/search/DEVICE' | sed s/'homematic.com.setDeviceFirmwareVersions('/''/ | sed s/');'/''/ |  jq -r '.[] | "\(.type);\(.version);https://ccu3-update.homematic.com/firmware/download?cmd=download&serial=0&product=\(.type)" | select(test("HM-"))')
echo "found ${#FIRMWARES[@]} with update service" >> "$LOG_FILE"
# add firmwares not available via api to array, mind the ; separator
FIRMWARES+=('HM-ES-TX-WM;2.5.0;https://www.eq-3.de/downloads/software/firmware/HM-ES-TX-WM_update_V2_5_191209.tgz')
FIRMWARES+=('HM-LC-Sw1PBU-FM;2.8.2;https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.tgz') # firmware link in former script not valid anymore
FIRMWARES+=('HM-Sen-RD-O;1.4.003;https://www.eq-3.de/Downloads/Software/Firmware/hm-sen-rd-o_update_V1_4_003_130930.tgz')
for i in "${!FIRMWARES[@]}"; do
    FILE_SIZE=0
    # split array entry
	DEVICE_NAME=`echo ${FIRMWARES[i]}| cut -d';' -f1`
	FIRMWARE_VERSION=`echo ${FIRMWARES[i]}| cut -d';' -f2`
	FILE_URL=`echo ${FIRMWARES[i]}| cut -d';' -f3`
	echo "downloading firmware for $DEVICE_NAME (version: $FIRMWARE_VERSION) from $FILE_URL" >> "$LOG_FILE"
    # extract real filenames for later processing
	FILE_NAME_FULL_PATH=`wget -nv --content-disposition -P "$FIRMWAREDIR" "$FILE_URL" 2>&1 | cut -d\" -f2`
	FILE_NAME_NO_EXTENSION=$(cut -d\/ -f4 <<< "$FILE_NAME_FULL_PATH" | cut -d\. -f1)
	FILE_SIZE=`stat -c%s "$FILE_NAME_FULL_PATH"`
    echo "downloaded file $FILE_NAME_FULL_PATH with $FILE_SIZE bytes" >> "$LOG_FILE"
    if [ $FILE_SIZE -le $FIRMWARE_ERROR_SIZE ]; then
        echo "Firmware URL:'$FILE_URL' doesn't seem to be valid because the file is only $FILE_SIZE bytes and more than $FIRMWARE_ERROR_SIZE is expected." >> "$LOG_FILE"
        continue
    fi
	FILE_NAME_BASE=$(sed -nE 's/(.*_V)([[:digit:]]*)(_)([[:digit:]]*)(_)([[:digit:]]*)(.*)/\1\2\3\4\5\6/p' <<< $FILE_NAME_NO_EXTENSION)
	FIRMWARE_VERSION=$(sed -nE 's/(.*_V)([[:digit:]]*)(_)([[:digit:]]*)(_)([[:digit:]]*)(.*)/\2\4/p' <<< $FILE_NAME_NO_EXTENSION)
	case "$DEVICE_NAME" in 
		HM-CC-RT-DN)		DEVICE_TYPE_NUMBER="95" ;;
		HM-Dis-EP-WM55)		DEVICE_TYPE_NUMBER="FB" ;;
		HM-ES-PMSw1-Pl-DN-R1)	DEVICE_TYPE_NUMBER="D7" ;;
		HM-ES-PMSw1-Pl-DN-R2)	DEVICE_TYPE_NUMBER="E2" ;;
		HM-ES-PMSw1-Pl-DN-R3)	DEVICE_TYPE_NUMBER="E3" ;;
		HM-ES-PMSw1-Pl-DN-R4)	DEVICE_TYPE_NUMBER="E4" ;;
		HM-ES-PMSw1-Pl-DN-R5)	DEVICE_TYPE_NUMBER="E5" ;;
		HM-LC-Bl1PBU-FM)	DEVICE_TYPE_NUMBER="6A" ;;
		HM-LC-Dim1L-Pl-3)	DEVICE_TYPE_NUMBER="B3" ;;
		HM-LC-Dim1PWM-CV)	DEVICE_TYPE_NUMBER="72" ;;
		HM-LC-Dim1T-DR)		DEVICE_TYPE_NUMBER="105" ;;
		HM-LC-Dim1TPBU-FM)	DEVICE_TYPE_NUMBER="68" ;;
		HM-LC-Dim1T-Pl-3)	DEVICE_TYPE_NUMBER="B4" ;;
		HM-LC-RGBW-WM)		DEVICE_TYPE_NUMBER="F4" ;;
		HM-LC-Sw1-Pl-DN-R1)	DEVICE_TYPE_NUMBER="D8" ;;
		HM-LC-Sw1-Pl-DN-R2)	DEVICE_TYPE_NUMBER="E6" ;;
		HM-LC-Sw1-Pl-DN-R3)	DEVICE_TYPE_NUMBER="E7" ;;
		HM-LC-Sw1-Pl-DN-R4)	DEVICE_TYPE_NUMBER="E8" ;;
		HM-LC-Sw1-Pl-DN-R5)	DEVICE_TYPE_NUMBER="E9" ;;
		HM-LC-Sw1-Pl-DN-R5)	DEVICE_TYPE_NUMBER="E9" ;;
		HM-MOD-Re-8)		DEVICE_TYPE_NUMBER="BE" ;;
		HM-OU-CFM-TW)		DEVICE_TYPE_NUMBER="FA" ;;
		HM-Sen-MDIR-WM55)	DEVICE_TYPE_NUMBER="DB" ;;
		HM-TC-IT-WM-W-EU)	DEVICE_TYPE_NUMBER="AD" ;;
		HM-ES-TX-WM)		DEVICE_TYPE_NUMBER="DE" ;;
		HM-LC-Sw1PBU-FM)	DEVICE_TYPE_NUMBER="69" ;;
		HM-Sen-RD-O)		DEVICE_TYPE_NUMBER="A7" ;;
		*)	echo "no device type number found for $DEVICE_NAME" >> "$LOG_FILE"; 
            continue;; # no valid device found
	esac
	# uncomment the following lines to verify the device type numbers via the XML files
	#echo "$DEVICE_NAME has firmware FILE_NAME_FULL_PATH $DEVICE_TYPE_NUMBER"
	#echo "device type number for $DEVICE_NAME from XML files is " `xmlstarlet sel -T -t -v "//*[@id='$DEVICE_NAME']/typeNumber" "$DEVICE_DESCRIPTION_DIR"/*.xml`
	DEVICE_TYPE_NUMBER=$(printf "%08x\n" $((16#$DEVICE_TYPE_NUMBER))) # pad hex number with up to 8 leading 0's
	FIRMWARE_NAME="0000.${DEVICE_TYPE_NUMBER^^}"
	echo "firmware name is $FIRMWARE_NAME" >> "$LOG_FILE"
    if ! tar zxf $FILE_NAME_FULL_PATH -C $FIRMWAREDIR; then echo "error expanding $FILE_NAME_FULL_PATH" >> "$LOG_FILE"; continue; fi
    # sometimes the date part of th eq3 file name is not identical to the date part of the tar archive, therefore we need a wildcard
    if ! mv "$FIRMWAREDIR"/"$FILE_NAME_BASE"*.eq3 "$SCRIPTDIR"/"$FIRMWARE_NAME.fw"; then echo "error renaming firmware file from " >> "$LOG_FILE"; continue; fi 
    if ! echo "$FIRMWARE_VERSION" > "$SCRIPTDIR"/"$FIRMWARE_NAME.version"; then echo "error writing version information" >> "$LOG_FILE"; continue; fi
    if [ -e "$FILE_NAME_FULL_PATH" ]; then rm "$FILE_NAME_FULL_PATH"; fi
    if [ -e "$FIRMWAREDIR/changelog.txt" ]; then rm "$FIRMWAREDIR/changelog.txt"; fi
    if [ -e "$FIRMWAREDIR/info" ]; then rm "$FIRMWAREDIR/info"; fi
done

chown homegear:homegear "$SCRIPTDIR"/*.fw
chown homegear:homegear "$SCRIPTDIR"/*.version
chmod 444 "$SCRIPTDIR"/*.fw
chmod 444 "$SCRIPTDIR"/*.version

echo "script finished at `date +%Y-%m-%d_%H:%M:%S`" >> "$LOG_FILE"
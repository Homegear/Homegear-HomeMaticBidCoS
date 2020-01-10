#!/bin/sh
SCRIPTDIR="$( cd "$(dirname $0)" && pwd )"
FIRMWAREDIR=/tmp/HomegearTemp

rm -f $SCRIPTDIR/0000.*

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm-ou-cfm-tw_update_V1_2_160418.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm-ou-cfm-tw_update_V1_2_160418.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm-ou-cfm-tw_update_V1_2_160418.eq3 $SCRIPTDIR/0000.000000FA.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm-ou-cfm-tw_update_V1_2_160418.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "12" > $SCRIPTDIR/0000.000000FA.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/downloads/software/firmware/HM-ES-TX-WM_update_V2_5_191209.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-ES-TX-WM_update_V2_5_191209.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-ES-TX-WM_update_V2_5_191209.eq3 $SCRIPTDIR/0000.000000DE.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-ES-TX-WM_update_V2_5_191209.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "25" > $SCRIPTDIR/0000.000000DE.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Dim1PWM-CV_update_V2_9_0005_150730.tar.gz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Dim1PWM-CV_update_V2_9_0005_150730.tar.gz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Dim1PWM-CV_update_V2_9_0005_150730.eq3 $SCRIPTDIR/0000.00000067.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Dim1PWM-CV_update_V2_9_0005_150730.tar.gz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "29" > $SCRIPTDIR/0000.00000067.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Dim1L-Pl-3_update_V2_9_0007_150803.tar.gz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Dim1L-Pl-3_update_V2_9_0007_150803.tar.gz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Dim1L-Pl-3_update_V2_9_0007_150803.eq3 $SCRIPTDIR/0000.000000B3.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Dim1L-Pl-3_update_V2_9_0007_150803.tar.gz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "29" > $SCRIPTDIR/0000.000000B3.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Dim1TPBU-FM_update_V2_9_0005_150730.tar.gz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Dim1TPBU-FM_update_V2_9_0005_150730.tar.gz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Dim1TPBU-FM_update_V2_9_0005_150730.eq3 $SCRIPTDIR/0000.00000068.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Dim1TPBU-FM_update_V2_9_0005_150730.tar.gz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "29" > $SCRIPTDIR/0000.00000068.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Dim1T-Pl-3_update_V2_9_0005_150730.tar.gz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Dim1T-Pl-3_update_V2_9_0005_150730.tar.gz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Dim1T-Pl-3_update_V2_9_0005_150730.eq3 $SCRIPTDIR/0000.000000B4.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Dim1T-Pl-3_update_V2_9_0005_150730.tar.gz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "29" > $SCRIPTDIR/0000.000000B4.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm-mod-re-8_update_V1_2_150911.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm-mod-re-8_update_V1_2_150911.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm-mod-re-8_update_V1_2_150911.eq3 $SCRIPTDIR/0000.000000BE.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm-mod-re-8_update_V1_2_150911.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "29" > $SCRIPTDIR/0000.000000BE.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.eq3 $SCRIPTDIR/0000.00000069.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "28" > $SCRIPTDIR/0000.00000069.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-ES-PMSw1-Pl_update_V2_5_0009_150217.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-ES-PMSw1-Pl_update_V2_5_0009_150217.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-ES-PMSw1-Pl_update_V2_5_0009_150217.eq3 $SCRIPTDIR/0000.000000AC.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-ES-PMSw1-Pl_update_V2_5_0009_150217.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "25" > $SCRIPTDIR/0000.000000AC.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm_cc_rt_dn_update_V1_4_001_141020.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm_cc_rt_dn_update_V1_4_001_141020.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm_cc_rt_dn_update_V1_4_001_141020.eq3 $SCRIPTDIR/0000.00000095.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm_cc_rt_dn_update_V1_4_001_141020.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "14" > $SCRIPTDIR/0000.00000095.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm_tc_it_wm_w_eu_update_V1_3_002_150827.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm_tc_it_wm_w_eu_update_V1_3_002_150827.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm_tc_it_wm_w_eu_update_V1_3_002_150827.eq3 $SCRIPTDIR/0000.000000AD.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm_tc_it_wm_w_eu_update_V1_3_002_150827.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "13" > $SCRIPTDIR/0000.000000AD.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm-sen-rd-o_update_V1_4_003_130930.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm-sen-rd-o_update_V1_4_003_130930.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm-sen-rd-o_update_V1_4_130930.eq3 $SCRIPTDIR/0000.000000A7.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm-sen-rd-o_update_V1_4_003_130930.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "14" > $SCRIPTDIR/0000.000000A7.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Bl1PBU-FM_update_V2_11_1_161212.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_11_1_161212.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_11_1_161212.eq3 $SCRIPTDIR/0000.0000006A.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_11_1_161212.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "2B" > $SCRIPTDIR/0000.0000006A.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/HM-Sen-MDIR-WM55_update_V1_2_0_160825.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_2_0_160825.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_2_0_160825.eq3 $SCRIPTDIR/0000.000000DB.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_2_0_160825.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "12" > $SCRIPTDIR/0000.000000DB.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR https://www.eq-3.de/Downloads/Software/Firmware/hm_dis_ep_wm55_update_V1_1_160927.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/hm_dis_ep_wm55_update_V1_1_160927.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/hm_dis_ep_wm55_update_V1_1_160927.eq3 $SCRIPTDIR/0000.000000FB.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/hm_dis_ep_wm55_update_V1_1_160927.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "11" > $SCRIPTDIR/0000.000000FB.version
	[ $? -ne 0 ] && exit 1
fi


rm -Rf /tmp/HomegearTemp

chown homegear:homegear $SCRIPTDIR/*.fw
chown homegear:homegear $SCRIPTDIR/*.version
chmod 444 $SCRIPTDIR/*.fw
chmod 444 $SCRIPTDIR/*.version

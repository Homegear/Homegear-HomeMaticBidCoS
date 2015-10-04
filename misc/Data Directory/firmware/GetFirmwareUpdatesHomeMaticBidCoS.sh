#!/bin/sh
SCRIPTDIR="$( cd "$(dirname $0)" && pwd )"
FIRMWAREDIR=/tmp/HomegearTemp

rm -f $SCRIPTDIR/0000.*

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Sw1PBU-FM_update_V2_8_2_150713.tgz
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

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/HM-ES-PMSw1-Pl_update_V2_5_0009_150217.tgz
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

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/hm_cc_rt_dn_update_V1_4_001_141020.tgz
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

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/hm_tc_it_wm_w_eu_update_V1_3_002_150827.tgz
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

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/hm-sen-rd-o_update_V1_4_003_130930.tgz
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

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/HM-LC-Bl1PBU-FM_update_V2_8_2_150713.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_8_2_150713.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_8_2_150713.eq3 $SCRIPTDIR/0000.0000006A.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-LC-Bl1PBU-FM_update_V2_8_2_150713.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "28" > $SCRIPTDIR/0000.0000006A.version
	[ $? -ne 0 ] && exit 1
fi

wget -P $FIRMWAREDIR http://www.eq-3.de/Downloads/Software/Firmware/HM-Sen-MDIR-WM55_update_V1_1_2_150413.tgz
if [ $? -eq 0 ]; then
	tar -zxf $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_1_2_150413.tgz -C $FIRMWAREDIR
	[ $? -ne 0 ] && exit 1
	mv $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_1_2_150413.eq3 $SCRIPTDIR/0000.000000DB.fw
	[ $? -ne 0 ] && exit 1
	rm $FIRMWAREDIR/HM-Sen-MDIR-WM55_update_V1_1_2_150413.tgz
	rm $FIRMWAREDIR/changelog.txt
	rm $FIRMWAREDIR/info
	echo "11" > $SCRIPTDIR/0000.000000DB.version
	[ $? -ne 0 ] && exit 1
fi

rm -Rf /tmp/HomegearTemp

chown homegear:homegear $SCRIPTDIR/*.fw
chown homegear:homegear $SCRIPTDIR/*.version
chmod 444 $SCRIPTDIR/*.fw
chmod 444 $SCRIPTDIR/*.version

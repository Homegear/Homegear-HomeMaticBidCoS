#!/bin/tclsh

#Kanal-EasyMode!

#source [file join $env(DOCUMENT_ROOT) config/easymodes/em_common.tcl]
source [file join /www/config/easymodes/em_common.tcl]

#Namen der EasyModes tauchen nicht mehr auf. Der Durchgängkeit werden sie hier noch definiert.
set PROFILES_MAP(0)	"Experte"
set PROFILES_MAP(1)	"TheOneAndOnlyEasyMode"

proc getCheckBox {type param value prn} {
  set checked ""
  if { $value } then { set checked "checked=\"checked\"" }
  set s "<input id='separate_$type\_$prn' type='checkbox' $checked value='dummy' name=$param/>"
  return $s
}

proc getMinValue {param} {
  global psDescr
  upvar psDescr descr
  array_clear param_descr
  array set param_descr $descr($param)
  set min [format {%1.1f} $param_descr(MIN)]
  return "$min"
}

proc getMaxValue {param} {
  global psDescr
  upvar psDescr descr
  array_clear param_descr
  array set param_descr $descr($param)
  set max [format {%1.1f} $param_descr(MAX)]
  return "$max"
}

proc getTextField {type param value prn} {
  global psDescr
  set elemId 'separate_$type\_$prn'

  set s "<input id=$elemId type=\"text\" size=\"30\" value=\"$value\" name=\"$param\" />"
  return $s
}

proc getPasswordField {type param value prn} {
  global psDescr
  set elemId 'separate_$type\_$prn'

  set s "<input id=$elemId type=\"password\" size=\"30\" value=\"$value\" name=\"$param\" />"
  return $s
}

proc getUnit {param} {
  global psDescr
  upvar psDescr descr
  array_clear param_descr
  array set param_descr $descr($param)
  set unit $param_descr(UNIT)

  if {$unit == "minutes"} {
   set unit "\${lblMinutes}"
  }

  if {$unit == "K"} {
    set unit "&#176;C"
  }

  return "$unit"
}

proc getMinMaxValueDescr {param} {
	global psDescr
  upvar psDescr descr
  array_clear param_descr
	array set param_descr $descr($param)
  set min $param_descr(MIN)
  set max $param_descr(MAX)

  # Limit float to 2 decimal places
  if {[llength [split $min "."]] == 2} {
    set min [format {%1.2f} $min]
    set max [format {%1.2f} $max]
  }
  return "($min - $max)"
}

proc getHelpIcon {title text x y} {
  set ret "<img src=\"/ise/img/help.png\" style=\"cursor: pointer; width:18px; height:18px; position:relative; top:2px\" onclick=\"showParamHelp('$title', '$text', '$x', '$y')\">"
  return $ret
}

proc set_htmlParams {iface address pps pps_descr special_input_id peer_type} {

	global env iface_url psDescr

	upvar PROFILES_MAP  PROFILES_MAP
	upvar HTML_PARAMS   HTML_PARAMS
	upvar PROFILE_PNAME PROFILE_PNAME
	upvar $pps          ps
	upvar $pps_descr    ps_descr

	set DEVICE "DEVICE"
	set hlpBoxWidth 450
	set hlpBoxHeight 160

	puts "<script type=\"text/javascript\">"
		puts "showParamHelp = function(title, text, x , y) {"
			puts "var width = (! isNaN(x)) ? x : 450;"
			puts "var height = (! isNaN(y)) ? y : 260;"
			puts "MessageBox.show(title, text,\"\" ,width , height);"
		puts "}"
	puts "</script>"
  
	array set psDescr [xmlrpc $iface_url($iface) getParamsetDescription [list string $address] [list string MASTER]]

	append HTML_PARAMS(separate_1) "<table class=\"ProfileTbl\">"
	set prn 1
	set param IP_ADDRESS
	append HTML_PARAMS(separate_1) "<tr><td>IP-Adresse</td>"
	append HTML_PARAMS(separate_1)  "<td>[getTextField $DEVICE $param $ps($param) $prn][getHelpIcon "IP-Adresse" "Geben Sie hier die IP-Adresse Ihrer Foscam ein." $hlpBoxWidth $hlpBoxHeight]</td>"
	append HTML_PARAMS(separate_1) "</tr>"

	incr prn
	set param PORT
	append HTML_PARAMS(separate_1) "<tr><td>Port</td>"
	append HTML_PARAMS(separate_1)  "<td>[getTextField $DEVICE $param $ps($param) $prn][getHelpIcon "Port" "Geben Sie hier die in der Foscam-Konfiguration definierte Portnummer Ihrer Foscam an." $hlpBoxWidth $hlpBoxHeight]</td>"
	append HTML_PARAMS(separate_1) "</tr>"

	incr prn
	set param USER
	append HTML_PARAMS(separate_1) "<tr><td>Benutzername</td>"
	append HTML_PARAMS(separate_1)  "<td>[getTextField $DEVICE $param $ps($param) $prn][getHelpIcon "Benutzername" "Geben Sie hier einen in der Foscam-Konfiguration definierten Benutzernamen an (Standard: admin)." $hlpBoxWidth $hlpBoxHeight]</td>"
	append HTML_PARAMS(separate_1) "</tr>"
	
	incr prn
	set param PASSWORD
	append HTML_PARAMS(separate_1) "<tr><td>Kennwort</td>"
	append HTML_PARAMS(separate_1)  "<td>[getPasswordField $DEVICE $param $ps($param) $prn][getHelpIcon "Kennwort" "Geben Sie hier das dem Benutzernamen zugewiesene Kennwort an." $hlpBoxWidth $hlpBoxHeight]</td>"
	append HTML_PARAMS(separate_1) "</tr>"
	
	#incr prn
	#set param USE_SSL
	#append HTML_PARAMS(separate_1) "<tr><td>SSL-Verschlüsselung</td><td>"
	#append HTML_PARAMS(separate_1)  [getCheckBox $DEVICE '$param' $ps($param) $prn][getHelpIcon "SSL-Verschlüsselung" "Wenn Sie diese Option aktivieren, wird die Verbindung mit Ihrer Foscam SSL-verschlüsselt." $hlpBoxWidth $hlpBoxHeight]
	#append HTML_PARAMS(separate_1) "</td></tr>"
	append HTML_PARAMS(separate_1) "</table>"
}

constructor

--TEST--
strstr() function
--FILE--
<?php
	var_dump(strstr("test string", "test"));
	var_dump(strstr("test string", "string"));
	var_dump(strstr("test string", "strin"));
	var_dump(strstr("test string", "t s"));
	var_dump(strstr("test string", "g"));
	var_dump(md5(strstr("te".chr(0)."st", chr(0))));
	var_dump(strstr("tEst", "test"));
	var_dump(strstr("teSt", "test"));
	var_dump(@strstr("", ""));
	var_dump(@strstr("a", ""));
	var_dump(@strstr("", "a"));
	var_dump(md5(@strstr("\\\\a\\", "\\a")));
	var_dump(strstr("test string", " ", false));
	var_dump(strstr("test string", " ", true));
?>
--EXPECT--
string(11) "test string"
string(6) "string"
string(6) "string"
string(8) "t string"
string(1) "g"
string(32) "7272696018bdeb2c9a3f8d01fc2a9273"
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
string(32) "6ec19f52f0766c463f3bb240f4396913"
string(7) " string"
string(4) "test"
--UEXPECT--
unicode(11) "test string"
unicode(6) "string"
unicode(6) "string"
unicode(8) "t string"
unicode(1) "g"
unicode(32) "7272696018bdeb2c9a3f8d01fc2a9273"
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
unicode(32) "6ec19f52f0766c463f3bb240f4396913"
unicode(7) " string"
unicode(4) "test"

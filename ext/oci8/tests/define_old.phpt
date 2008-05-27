--TEST--
ocidefinebyname()
--SKIPIF--
<?php if (!extension_loaded('oci8')) die("skip no oci8 extension"); ?>
--FILE--
<?php

require dirname(__FILE__)."/connect.inc";
require dirname(__FILE__)."/create_table.inc";

$insert_sql = "INSERT INTO ".$schema.$table_name." (string) VALUES ('some')";

if (!($s = ociparse($c, $insert_sql))) {
        die("oci_parse(insert) failed!\n");
}

if (!ociexecute($s)) {
        die("oci_execute(insert) failed!\n");
}

$stmt = ociparse($c, "SELECT string FROM ".$table_name."");

/* the define MUST be done BEFORE ociexecute! */

$strong = '';
ocidefinebyname($stmt, "STRING", $string, 20);

ociexecute($stmt);

while (ocifetch($stmt)) {
	var_dump($string);
}

require dirname(__FILE__)."/drop_table.inc";

echo "Done\n";

?>
--EXPECT--
unicode(4) "some"
Done

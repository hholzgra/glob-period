--TEST--
globals in local scope - 3 
--INIT--
variables_order="egpcs"
--FILE--
<?php

function test() {
	include dirname(__FILE__)."/globals.inc";
}

test();

echo "Done\n";
?>
--EXPECTF--	
bool(true)
bool(false)
string(5) "array"
int(%d)
string(%d) "%s"

Notice: Undefined index:  PHP_SELF in %s on line %d
NULL

Notice: Undefined variable: _SERVER in %s on line %d
NULL
Done
--UEXPECTF--
bool(true)
bool(false)
unicode(5) "array"
int(%d)
string(%d) "%s"

Notice: Undefined index:  PHP_SELF in %s on line %d
NULL

Notice: Undefined variable: _SERVER in %s on line %d
NULL
Done

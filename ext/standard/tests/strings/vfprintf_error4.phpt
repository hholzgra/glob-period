--TEST--
Test vfprintf() function : error conditions (various conditions)
--INI--
precision=14
--FILE--
<?php
/* Prototype  : int vfprintf(resource stream, string format, array args)
 * Description: Output a formatted string into a stream 
 * Source code: ext/standard/formatted_print.c
 * Alias to functions: 
 */

// Open handle
$file = 'vfprintf_test.txt';
$fp = fopen( $file, "a+" );

// Set unicode encoding
stream_encoding( $fp, 'unicode' );

echo "\n-- Testing vfprintf() function with other strangeties  --\n";
var_dump( vfprintf( 'foo', 'bar', array( 'baz' ) ) );
var_dump( vfprintf( $fp, 'Foo %$c-0202Sd', array( 2 ) ) );

// Close handle
fclose( $fp );

?>
===DONE===
--CLEAN--
<?php

$file = 'vfprintf_text.txt';
unlink( $file );

?>
--EXPECTF--
-- Testing vfprintf() function with other strangeties  --

Warning: vfprintf(): supplied argument is not a valid stream resource in %s on line %d
bool(false)

Warning: vfprintf(): Zero is not a valid argument number in %s on line %d
bool(false)
===DONE===
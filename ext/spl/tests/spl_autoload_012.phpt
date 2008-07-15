--TEST--
SPL: spl_autoload() capturing multiple Exceptions in __autoload
--FILE--
<?php

function autoload_first($name)
{
  echo __METHOD__ . "\n";
  throw new Exception('first');
}

function autoload_second($name)
{
  echo __METHOD__ . "\n";
  throw new Exception('second');
}

spl_autoload_register('autoload_first');
spl_autoload_register('autoload_second');

class_exists('ThisClassDoesNotExist');

?>
===DONE===
--EXPECTF--
autoload_first
autoload_second

Fatal error: Uncaught exception 'Exception' with message 'first' in %sspl_autoload_012.php:%d
Stack trace:
#0 [internal function]: autoload_first('ThisClassDoesNo...')
#1 [internal function]: spl_autoload_call('ThisClassDoesNo...')
#2 %sspl_autoload_012.php(%d): class_exists('ThisClassDoesNo...')
#3 {main}

Next exception 'Exception' with message 'second' in %sspl_autoload_012.php:%d
Stack trace:
#0 [internal function]: autoload_second('ThisClassDoesNo...')
#1 [internal function]: spl_autoload_call('ThisClassDoesNo...')
#2 %sspl_autoload_012.php(%d): class_exists('ThisClassDoesNo...')
#3 {main}
  thrown in %sspl_autoload_012.php on line %d
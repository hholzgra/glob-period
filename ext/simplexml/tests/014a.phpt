--TEST--
SimpleXML: adding/removing attributes (single)
--SKIPIF--
<?php if (!extension_loaded("simplexml")) print "skip"; ?>
--FILE--
<?php 
$xml =<<<EOF
<people>
   <person name="Joe"></person>
</people>
EOF;

$people = simplexml_load_string($xml);
var_dump($people->person[0]['name']);
var_dump($people->person[0]['age']);
$person = $people->person[0];
$person['name'] = "XXX";
var_dump($people->person[0]['name']);
$people->person[0]['age'] = 30;
var_dump($people->person[0]['age']);
echo "---Unset:---\n";
unset($people->person[0]['age']);
echo "---Unset?---\n";
var_dump($people->person[0]['age']);
var_dump(isset($people->person[0]['age']));
echo "---Unsupported---\n";
var_dump($people->person[0]['age']);
$people->person['age'] += 5;
var_dump($people->person[0]['age']);
?>
===DONE===
--EXPECTF--
object(simplexml_element)#%d (1) {
  [0]=>
  string(3) "Joe"
}
NULL
object(simplexml_element)#%d (1) {
  [0]=>
  string(3) "XXX"
}
object(simplexml_element)#%d (1) {
  [0]=>
  string(2) "30"
}
---Unset:---
---Unset?---
NULL
bool(false)
---Unsupported---
object(simplexml_element)#%d (1) {
  [0]=>
  string(2) "30"
}

Fatal error: Unsupported operand types in %s014.php on line %d

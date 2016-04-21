--TEST--
Test new Timeval() : basic functionality
--FILE--
<?php
$time = new Grpc\Timeval(1234);
var_dump($time);
?>
===DONE===
--EXPECTF--
object(Grpc\Timeval)#1 (0) {
}
===DONE===

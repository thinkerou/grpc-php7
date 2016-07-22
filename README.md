# The grpc-php7 have merged grpc/grpc official!

## grpc-php7-extension

### REASON

- The code forked from [grpc](https://github.com/grpc/grpc/tree/master/src/php).

- Because the php extension only supports PHP5.x, and hopes it supports PHP7.

- Not change the extension code original logic.


### TODO

- [ ] Add phpt test case
- [X] Upgate phpunit, because added macro `ZEND_PARSE_PARAMETERS_START\END`
- [ ] Check memory using Valgrind
- [ ] Review code for checking `zval` or `zval*` for using right


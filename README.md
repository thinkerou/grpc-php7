# grpc-php7-extension

### 1. The code forks from [grpc](https://github.com/grpc/grpc/tree/master/src/php).

### 2. Because the php extension only supports PHP5.\*, and hopes it supports PHP7.

### 3. Not change the extension code original logic.


# TODO
- [ ] Add phpt test case
- [ ] Upgate phpunit, because added macro `ZEND_PARSE_PARAMETERS_START\END`
- [ ] Check memory using Valgrind
- [ ] Review code for checking `zval` or `zval*` for using right


/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "channel.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <ext/standard/info.h>
#include <ext/spl/spl_exceptions.h>
#include "php_grpc.h"

#include <zend_exceptions.h>

#include <stdbool.h>

#include <grpc/grpc.h>
#include <grpc/support/log.h>
#include <grpc/grpc_security.h>

#include "completion_queue.h"
#include "channel_credentials.h"
#include "server.h"
#include "timeval.h"

zend_class_entry *grpc_ce_channel;

static zend_object_handlers channel_object_handlers_channel;

/* Frees and destroys an instance of wrapped_grpc_channel */
static void free_wrapped_grpc_channel(zend_object *object) {
  wrapped_grpc_channel *channel = wrapped_grpc_channel_from_obj(object);
  zend_object_std_dtor(&channel->std);
  if (channel->wrapped != NULL) {
    grpc_channel_destroy(channel->wrapped);
  }
  return;
}

/* Initializes an instance of wrapped_grpc_channel to be associated with an
 * object of a class specified by class_type */
zend_object *create_wrapped_grpc_channel(zend_class_entry *class_type) {
  wrapped_grpc_channel *intern;
  intern = ecalloc(1, sizeof(wrapped_grpc_channel) +
                   zend_object_properties_size(class_type));
  
  zend_object_std_init(&intern->std, class_type);
  object_properties_init(&intern->std, class_type);

  intern->std.handlers = &channel_object_handlers_channel;
  
  return &intern->std;
}

void php_grpc_read_args_array(zval *args_array, grpc_channel_args *args) {
  HashTable *array_hash;
  //HashPosition array_pointer;
  int args_index;
  zval *data;
  zend_string *key;
  //zend_ulong index;
  array_hash = HASH_OF(args_array);
  if (!array_hash) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "array_hash is NULL", 1);
    return;
  }
  args->num_args = zend_hash_num_elements(array_hash);
  args->args = ecalloc(args->num_args, sizeof(grpc_arg));
  args_index = 0;
  ZEND_HASH_FOREACH_STR_KEY_VAL(array_hash, key, data) {
  /*for (zend_hash_internal_pointer_reset_ex(array_hash, &array_pointer);
       (data = zend_hash_get_current_data_ex(array_hash,
                                             &array_pointer)) != NULL;
       zend_hash_move_forward_ex(array_hash, &array_pointer)) {
    if (zend_hash_get_current_key_ex(array_hash, &key, &index,
                                     &array_pointer) != HASH_KEY_IS_STRING) {
      zend_throw_exception(spl_ce_InvalidArgumentException,
                           "args keys must be strings", 1);
      return;
    }*/
    if (key == NULL) {
      zend_throw_exception(spl_ce_InvalidArgumentException,
                           "args keys must be strings", 1);
    }
    args->args[args_index].key = ZSTR_VAL(key);
    switch (Z_TYPE_P(data)) {
      case IS_LONG:
        args->args[args_index].value.integer = (int)Z_LVAL_P(data);
        args->args[args_index].type = GRPC_ARG_INTEGER;
        break;
      case IS_STRING:
        args->args[args_index].value.string = Z_STRVAL_P(data);
        args->args[args_index].type = GRPC_ARG_STRING;
        break;
      default:
        zend_throw_exception(spl_ce_InvalidArgumentException,
                             "args values must be int or string", 1);
        return;
    }
    args_index++;
  }
  ZEND_HASH_FOREACH_END();
  return;
}

/**
 * Construct an instance of the Channel class. If the $args array contains a
 * "credentials" key mapping to a ChannelCredentials object, a secure channel
 * will be created with those credentials.
 * @param string $target The hostname to associate with this channel
 * @param array $args The arguments to pass to the Channel (optional)
 */
PHP_METHOD(Channel, __construct) {
  wrapped_grpc_channel *channel = Z_WRAPPED_GRPC_CHANNEL_P(getThis());
  //char *target;
  //size_t target_length;
  zend_string *target;
  zval *args_array = NULL;
  grpc_channel_args args;
  HashTable *array_hash;
  zval *creds_obj = NULL;
  wrapped_grpc_channel_credentials *creds = NULL;

  /* "Sa" == 1 string, 1 array */
#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa", &target, &args_array)
      == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "Channel expects a string and an array", 1);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STR(target)
    Z_PARAM_ARRAY(args_array)
  ZEND_PARSE_PARAMETERS_END();
#endif

  array_hash = HASH_OF(args_array);
  if ((creds_obj = zend_hash_str_find(array_hash, "credentials",
                                      sizeof("credentials") - 1)) != NULL) {
    if (Z_TYPE_P(creds_obj) == IS_NULL) {
      creds = NULL;
      zend_hash_str_del(array_hash, "credentials", sizeof("credentials") - 1);
    } else if (Z_OBJ_P(creds_obj)->ce != grpc_ce_channel_credentials) {
      zend_throw_exception(spl_ce_InvalidArgumentException,
                           "credentials must be a ChannelCredentials object",
                           1);
      return;
    } else {
      creds = Z_WRAPPED_GRPC_CHANNEL_CREDS_P(creds_obj);
      zend_hash_str_del(array_hash, "credentials", sizeof("credentials") - 1);
    }
  }
  php_grpc_read_args_array(args_array, &args);
  if (creds == NULL) {
    channel->wrapped = grpc_insecure_channel_create(ZSTR_VAL(target),
                                                    &args, NULL);
  } else {
    gpr_log(GPR_DEBUG, "Initialized secure channel");
    channel->wrapped =
        grpc_secure_channel_create(creds->wrapped, ZSTR_VAL(target),
                                   &args, NULL);
  }
  efree(args.args);
}

/**
 * Get the endpoint this call/stream is connected to
 * @return string The URI of the endpoint
 */
PHP_METHOD(Channel, getTarget) {
  wrapped_grpc_channel *channel = Z_WRAPPED_GRPC_CHANNEL_P(getThis());
  RETURN_STRING(grpc_channel_get_target(channel->wrapped));
}

/**
 * Get the connectivity state of the channel
 * @param bool (optional) try to connect on the channel
 * @return long The grpc connectivity state
 */
PHP_METHOD(Channel, getConnectivityState) {
  wrapped_grpc_channel *channel = Z_WRAPPED_GRPC_CHANNEL_P(getThis());
  zend_bool try_to_connect = 0;

  /* "|b" == 1 optional bool */
#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b",
                            &try_to_connect) == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "getConnectivityState expects a bool", 1);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_BOOL(try_to_connect)
  ZEND_PARSE_PARAMETERS_END();
#endif

  RETURN_LONG(grpc_channel_check_connectivity_state(channel->wrapped,
                                                    (int)try_to_connect));
}

/**
 * Watch the connectivity state of the channel until it changed
 * @param long The previous connectivity state of the channel
 * @param Timeval The deadline this function should wait until
 * @return bool If the connectivity state changes from last_state
 *              before deadline
 */
PHP_METHOD(Channel, watchConnectivityState) {
  wrapped_grpc_channel *channel = Z_WRAPPED_GRPC_CHANNEL_P(getThis());
  zend_long last_state;
  zval *deadline_obj;

  /* "lO" == 1 long 1 object */
#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "lO",
          &last_state, &deadline_obj, grpc_ce_timeval) == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
        "watchConnectivityState expects 1 long 1 timeval", 1);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_LONG(last_state)
    Z_PARAM_OBJECT_OF_CLASS(deadline_obj, grpc_ce_timeval)
  ZEND_PARSE_PARAMETERS_END();
#endif

  wrapped_grpc_timeval *deadline = Z_WRAPPED_GRPC_TIMEVAL_P(deadline_obj);
  grpc_channel_watch_connectivity_state(
      channel->wrapped, (grpc_connectivity_state)last_state,
      deadline->wrapped, completion_queue, NULL);
  grpc_event event = grpc_completion_queue_pluck(
      completion_queue, NULL,
      gpr_inf_future(GPR_CLOCK_REALTIME), NULL);
  RETURN_BOOL(event.success);
}

/**
 * Close the channel
 */
PHP_METHOD(Channel, close) {
  wrapped_grpc_channel *channel = Z_WRAPPED_GRPC_CHANNEL_P(getThis());
  if (channel->wrapped != NULL) {
    grpc_channel_destroy(channel->wrapped);
    channel->wrapped = NULL;
  }
}

static zend_function_entry channel_methods[] = {
    PHP_ME(Channel, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Channel, getTarget, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Channel, getConnectivityState, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Channel, watchConnectivityState, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Channel, close, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void grpc_init_channel() {
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "Grpc\\Channel", channel_methods);
  ce.create_object = create_wrapped_grpc_channel;
  grpc_ce_channel = zend_register_internal_class(&ce);
  memcpy(&channel_object_handlers_channel, zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  channel_object_handlers_channel.offset =
    XtOffsetOf(wrapped_grpc_channel, std);
  channel_object_handlers_channel.free_obj = free_wrapped_grpc_channel;
  return;
}

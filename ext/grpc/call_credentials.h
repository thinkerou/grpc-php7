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

#ifndef NET_GRPC_PHP_GRPC_CALL_CREDENTIALS_H_
#define NET_GRPC_PHP_GRPC_CALL_CREDENTIALS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_grpc.h"

#include "grpc/grpc.h"
#include "grpc/grpc_security.h"

/* Class entry for the CallCredentials PHP class */
extern zend_class_entry *grpc_ce_call_credentials;

/* Wrapper struct for grpc_call_credentials that can be associated
 * with a PHP object */
typedef struct wrapped_grpc_call_credentials {
  grpc_call_credentials *wrapped;
  zend_object std;
} wrapped_grpc_call_credentials;

static inline wrapped_grpc_call_credentials
*wrapped_grpc_call_creds_from_obj(zend_object *obj) {
  return
    (wrapped_grpc_call_credentials*)((char*)(obj) -
                                     XtOffsetOf(wrapped_grpc_call_credentials,
                                                std));
}

#define Z_WRAPPED_GRPC_CALL_CREDS_P(zv)           \
  wrapped_grpc_call_creds_from_obj(Z_OBJ_P((zv)))

/* Struct to hold callback function for plugin creds API */
typedef struct plugin_state {
  zend_fcall_info *fci;
  zend_fcall_info_cache *fci_cache;
} plugin_state;

/* Callback function for plugin creds API */
void plugin_get_metadata(void *state, grpc_auth_metadata_context context,
                         grpc_credentials_plugin_metadata_cb cb,
                         void *user_data);

/* Cleanup function for plugin creds API */
void plugin_destroy_state(void *ptr);

/* Initializes the CallCredentials PHP class */
void grpc_init_call_credentials();

#endif /* NET_GRPC_PHP_GRPC_CALL_CREDENTIALS_H_ */

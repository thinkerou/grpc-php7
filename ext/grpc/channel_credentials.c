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

#include "channel_credentials.h"
#include "call_credentials.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <ext/standard/info.h>
#include <ext/spl/spl_exceptions.h>
#include "php_grpc.h"

#include <zend_exceptions.h>
#include <zend_hash.h>

#include <grpc/support/alloc.h>
#include <grpc/grpc.h>
#include <grpc/grpc_security.h>

zend_class_entry *grpc_ce_channel_credentials;

static zend_object_handlers channel_creds_object_handlers_channel_creds;

static char* default_pem_root_certs = NULL;

static grpc_ssl_roots_override_result get_ssl_roots_override(
    char **pem_root_certs) {
  *pem_root_certs = default_pem_root_certs;
  if (default_pem_root_certs == NULL) {
    return GRPC_SSL_ROOTS_OVERRIDE_FAIL;
  }
  return GRPC_SSL_ROOTS_OVERRIDE_OK;
}

/* Frees and destroys an instance of wrapped_grpc_channel_credentials */
static void free_wrapped_grpc_channel_credentials(zend_object *object) {
  wrapped_grpc_channel_credentials *creds =
    wrapped_grpc_channel_creds_from_obj(object);
  if (creds->wrapped != NULL) {
    grpc_channel_credentials_release(creds->wrapped);
  }
  zend_object_std_dtor(&creds->std);
}

/* Initializes an instance of wrapped_grpc_channel_credentials to be
 * associated with an object of a class specified by class_type */
zend_object *create_wrapped_grpc_channel_credentials(
    zend_class_entry *class_type) {
  wrapped_grpc_channel_credentials *intern;
  intern = ecalloc(1, sizeof(wrapped_grpc_channel_credentials) +
                   zend_object_properties_size(class_type));

  zend_object_std_init(&intern->std, class_type);
  object_properties_init(&intern->std, class_type);
  
  intern->std.handlers = &channel_creds_object_handlers_channel_creds;
  
  return &intern->std;
}

void grpc_php_wrap_channel_credentials(grpc_channel_credentials *wrapped,
                                      zval *credentials_object) {
  object_init_ex(credentials_object, grpc_ce_channel_credentials);
  wrapped_grpc_channel_credentials *credentials =
    Z_WRAPPED_GRPC_CHANNEL_CREDS_P(credentials_object);
  credentials->wrapped = wrapped;
}

 /**
  * Set default roots pem.
  * @param string pem_roots PEM encoding of the server root certificates
  * @return void
  */
PHP_METHOD(ChannelCredentials, setDefaultRootsPem) {
  char *pem_roots;
  size_t pem_roots_length;

#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &pem_roots,
                           &pem_roots_length)  == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "setDefaultRootsPem expects 1 string", 1 TSRMLS_CC);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(pem_roots, pem_roots_length)
  ZEND_PARSE_PARAMETERS_END();
#endif

  default_pem_root_certs = gpr_malloc((pem_roots_length + 1) * sizeof(char));
  memcpy(default_pem_root_certs, pem_roots, pem_roots_length + 1);
}

/**
 * Create a default channel credentials object.
 * @return ChannelCredentials The new default channel credentials object
 */
PHP_METHOD(ChannelCredentials, createDefault) {
  grpc_channel_credentials *creds = grpc_google_default_credentials_create();
  grpc_php_wrap_channel_credentials(creds, return_value);
  RETURN_DESTROY_ZVAL(return_value);
}

/**
 * Create SSL credentials.
 * @param string pem_root_certs PEM encoding of the server root certificates
 * @param string pem_private_key PEM encoding of the client's private key
 *     (optional)
 * @param string pem_cert_chain PEM encoding of the client's certificate chain
 *     (optional)
 * @return ChannelCredentials The new SSL credentials object
 */
PHP_METHOD(ChannelCredentials, createSsl) {
  zend_string *pem_root_certs = NULL;
  zend_string *private_key = NULL;
  zend_string *cert_chain = NULL;

  grpc_ssl_pem_key_cert_pair pem_key_cert_pair;
  pem_key_cert_pair.private_key = pem_key_cert_pair.cert_chain = NULL;

  //TODO(thinkerou): add unittest 
  /* "|S!S!S! == 3 optional nullable strings */
#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S!S!S!", &pem_root_certs,
                            &private_key, &cert_chain) == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "createSsl expects 3 optional strings", 1);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(0, 3)
    Z_PARAM_OPTIONAL
    Z_PARAM_STR_EX(pem_root_certs, 1, 0)
    Z_PARAM_STR_EX(private_key, 1, 0)
    Z_PARAM_STR_EX(cert_chain, 1, 0)
  ZEND_PARSE_PARAMETERS_END();
#endif

  // private_key == NULL?
  // cert_chain == NULL?
  pem_key_cert_pair.private_key = ZSTR_VAL(private_key);
  pem_key_cert_pair.cert_chain = ZSTR_VAL(cert_chain);

  //TODO(thinkerou): deal unittest crash!!!
  grpc_channel_credentials *creds = grpc_ssl_credentials_create(
      ZSTR_VAL(pem_root_certs),
      pem_key_cert_pair.private_key == NULL ? NULL : &pem_key_cert_pair, NULL);
  grpc_php_wrap_channel_credentials(creds, return_value);
  RETURN_DESTROY_ZVAL(return_value);
}

/**
 * Create composite credentials from two existing credentials.
 * @param ChannelCredentials cred1 The first credential
 * @param CallCredentials cred2 The second credential
 * @return ChannelCredentials The new composite credentials object
 */
PHP_METHOD(ChannelCredentials, createComposite) {
  zval *cred1_obj;
  zval *cred2_obj;

  /* "OO" == 2 Objects */
#ifndef FAST_ZPP
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "OO", &cred1_obj,
                            grpc_ce_channel_credentials, &cred2_obj,
                            grpc_ce_call_credentials) == FAILURE) {
    zend_throw_exception(spl_ce_InvalidArgumentException,
                         "createComposite expects 2 Credentials", 1);
    return;
  }
#else
  ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT_OF_CLASS(cred1_obj, grpc_ce_channel_credentials)
    Z_PARAM_OBJECT_OF_CLASS(cred2_obj, grpc_ce_call_credentials)
  ZEND_PARSE_PARAMETERS_END();
#endif

  wrapped_grpc_channel_credentials *cred1 =
      Z_WRAPPED_GRPC_CHANNEL_CREDS_P(cred1_obj);
  wrapped_grpc_call_credentials *cred2 =
      Z_WRAPPED_GRPC_CALL_CREDS_P(cred2_obj);
  grpc_channel_credentials *creds =
      grpc_composite_channel_credentials_create(cred1->wrapped,
                                                cred2->wrapped, NULL);
  grpc_php_wrap_channel_credentials(creds, return_value);
  RETURN_DESTROY_ZVAL(return_value);
}

/**
 * Create insecure channel credentials
 * @return null
 */
PHP_METHOD(ChannelCredentials, createInsecure) {
  RETURN_NULL();
}

static zend_function_entry channel_credentials_methods[] = {
  PHP_ME(ChannelCredentials, setDefaultRootsPem, NULL,
         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
  PHP_ME(ChannelCredentials, createDefault, NULL,
         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
  PHP_ME(ChannelCredentials, createSsl, NULL,
         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
  PHP_ME(ChannelCredentials, createComposite, NULL,
         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
  PHP_ME(ChannelCredentials, createInsecure, NULL,
         ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
  PHP_FE_END
};

void grpc_init_channel_credentials() {
  zend_class_entry ce;
  INIT_CLASS_ENTRY(ce, "Grpc\\ChannelCredentials",
                   channel_credentials_methods);
  grpc_set_ssl_roots_override_callback(get_ssl_roots_override);
  ce.create_object = create_wrapped_grpc_channel_credentials;
  grpc_ce_channel_credentials = zend_register_internal_class(&ce);
  memcpy(&channel_creds_object_handlers_channel_creds,
         zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  channel_creds_object_handlers_channel_creds.offset =
    XtOffsetOf(wrapped_grpc_channel_credentials, std);
  channel_creds_object_handlers_channel_creds.free_obj =
    free_wrapped_grpc_channel_credentials;
}

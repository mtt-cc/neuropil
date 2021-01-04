//
// neuropil is copyright 2016-2020 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "event/ev.h"
#include "sodium.h"

#include "np_aaatoken.h"

#include "np_dhkey.h"
#include "util/np_event.h"

#include "dtime.h"
#include "np_log.h"
#include "np_legacy.h"
#include "np_tree.h"
#include "np_treeval.h"
#include "np_key.h"
#include "np_keycache.h"
#include "np_message.h"
#include "core/np_comp_msgproperty.h"
#include "core/np_comp_node.h"
#include "np_threads.h"
#include "np_settings.h"
#include "np_util.h"
#include "np_constants.h"
#include "neuropil.h"
#include "neuropil_data.h"
#include "np_serialization.h"

_NP_GENERATE_MEMORY_IMPLEMENTATION(np_aaatoken_t);

NP_SLL_GENERATE_IMPLEMENTATION_COMPARATOR(np_aaatoken_ptr);
NP_SLL_GENERATE_IMPLEMENTATION(np_aaatoken_ptr);

NP_PLL_GENERATE_IMPLEMENTATION(np_aaatoken_ptr);

void _np_aaatoken_t_new(np_state_t *context, NP_UNUSED uint8_t type, NP_UNUSED size_t size, void* token)
{
    log_trace_msg(LOG_TRACE | LOG_AAATOKEN, "start: void _np_aaatoken_t_new(void* token){");
    np_aaatoken_t* aaa_token = (np_aaatoken_t*) token;

    aaa_token->version = 0.90;

    // aaa_token->issuer;
    memset(aaa_token->realm,    0, 255);
    memset(aaa_token->issuer,   0,  65);
    memset(aaa_token->subject,  0, 255);
    memset(aaa_token->audience, 0, 255);

    aaa_token->private_key_is_set = false;
    aaa_token->crypto.ed25519_secret_key_is_set = false;
    aaa_token->crypto.ed25519_public_key_is_set = false;

    memset(aaa_token->crypto.derived_kx_public_key, 0, crypto_sign_PUBLICKEYBYTES*(sizeof(unsigned char)));
    memset(aaa_token->crypto.derived_kx_secret_key, 0, crypto_sign_SECRETKEYBYTES*(sizeof(unsigned char)));
    memset(aaa_token->crypto.ed25519_public_key, 0, crypto_sign_ed25519_PUBLICKEYBYTES*(sizeof(unsigned char)));
    memset(aaa_token->crypto.ed25519_secret_key, 0, crypto_sign_ed25519_SECRETKEYBYTES*(sizeof(unsigned char)));

    memset(aaa_token->signature, 0, crypto_sign_BYTES*(sizeof(unsigned char)));
    aaa_token->is_signature_verified = false;

    char* uuid = aaa_token->uuid;
    np_uuid_create("generic_aaatoken", 0, &uuid);

    aaa_token->issued_at = np_time_now();
    aaa_token->not_before = aaa_token->issued_at;

    int expire_sec =  ((int)randombytes_uniform(20)+10);

    aaa_token->expires_at = aaa_token->not_before + expire_sec;
    log_debug_msg(LOG_DEBUG | LOG_AAATOKEN, "aaatoken expires in %d sec", expire_sec);


    np_init_datablock(aaa_token->attributes, sizeof(aaa_token->attributes));
    aaa_token->state = AAA_UNKNOWN;

    aaa_token->type = np_aaatoken_type_undefined;
    aaa_token->scope = np_aaatoken_scope_undefined;
    aaa_token->issuer_token = aaa_token;

    log_debug_msg(LOG_DEBUG, "token %p", aaa_token);

}

void _np_aaatoken_t_del (NP_UNUSED np_state_t *context, NP_UNUSED uint8_t type, NP_UNUSED size_t size, void* token)
{
    np_aaatoken_t* aaa_token = (np_aaatoken_t*) token;
    log_debug_msg(LOG_DEBUG, "token %p ", aaa_token);
}

void _np_aaatoken_encode(np_tree_t* data, np_aaatoken_t* token, bool trace)
{
    log_trace_msg(LOG_TRACE | LOG_AAATOKEN, "start: void np_aaatoken_encode(np_tree_t* data, np_aaatoken_t* token){");

    np_state_t* context = np_ctx_by_memory(token);
    // if(trace) _np_aaatoken_trace_info("encode", token);
    // included into np_token_handshake

    np_tree_replace_str( data, "np.t.type", np_treeval_new_ush(token->type));
    np_tree_replace_str( data, "np.t.u",    np_treeval_new_s(token->uuid));
    np_tree_replace_str( data, "np.t.r",    np_treeval_new_s(token->realm));
    np_tree_replace_str( data, "np.t.i",    np_treeval_new_s(token->issuer));
    np_tree_replace_str( data, "np.t.s",    np_treeval_new_s(token->subject));
    np_tree_replace_str( data, "np.t.a",    np_treeval_new_s(token->audience));
    np_tree_replace_str( data, "np.t.p",    np_treeval_new_bin(token->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES));

    np_tree_replace_str( data, "np.t.ex",   np_treeval_new_d(token->expires_at));
    np_tree_replace_str( data, "np.t.ia",   np_treeval_new_d(token->issued_at));
    np_tree_replace_str( data, "np.t.nb",   np_treeval_new_d(token->not_before));
    np_tree_replace_str( data, "np.t.si",   np_treeval_new_bin(token->signature, crypto_sign_BYTES));

    size_t attributes_size;

    np_get_data_size(token->attributes, &attributes_size);
    np_tree_replace_str( data, "np.t.e",   np_treeval_new_bin(token->attributes, attributes_size));

    if(token->scope <= np_aaatoken_scope_private_available) {
        _np_aaatoken_update_attributes_signature(token);
    }
    np_tree_replace_str( data, "np.t.sie", np_treeval_new_bin(token->attributes_signature, sizeof(token->attributes_signature)));

//#ifdef DEBUG
//	char pubkey[65];
//	sodium_bin2hex(pubkey, 65, token->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES);
//	pubkey[64] = '\0';
//	fprintf(stdout, "L: uuid: %s ## subj: %s ## pk: %s\n", token->uuid, token->subject, pubkey);
//#endif

}

void np_aaatoken_encode(np_tree_t* data, np_aaatoken_t* token)
{
    _np_aaatoken_encode(data, token, true);
}

/*
    @return: true if all medatory filds are present
*/
bool np_aaatoken_decode(np_tree_t* data, np_aaatoken_t* token)
{
    assert (NULL != data);
    assert (NULL != token);
    np_ctx_memory(token);

    bool ret = true;
    // get e2e encryption details of sending entity

    np_tree_elem_t* tmp;
    token->scope = np_aaatoken_scope_undefined;
    token->type = np_aaatoken_type_undefined;

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.u")))
    {
        strncpy(token->uuid, np_treeval_to_str(tmp->val, NULL), NP_UUID_BYTES);
    }
    else { ret = false;/*Mandatory field*/ }

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.type")))
    {
        token->type = tmp->val.value.ush;
    }
    else { ret = false;/*Mandatory field*/ }

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.r")))
    {
        strncpy(token->realm,  np_treeval_to_str(tmp->val, NULL), 255);
    }
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.i")))
    {
        strncpy(token->issuer,  np_treeval_to_str(tmp->val, NULL), 65);
    }
    else { ret = false;/*Mandatory field*/ }
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.s")))
    {
        strncpy(token->subject,  np_treeval_to_str(tmp->val, NULL), 255);
    }
    else { ret = false;/*Mandatory field*/ }
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.a")))
    {
        strncpy(token->audience,  np_treeval_to_str(tmp->val, NULL), 255);
    }
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.p")))
    {
        if (NULL == np_cryptofactory_by_public(context, &token->crypto, tmp->val.value.bin)) {
            log_msg(LOG_ERROR, "Could not decode crypto details from token");
            ret = false;/*Mandatory field*/
        }
    }
    else { ret = false; /* Mandatory field*/ }

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.ex")))
    {
        token->expires_at = tmp->val.value.d;
    }
    else { ret = false;/*Mandatory field*/ }

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.ia")))
    {
        token->issued_at = tmp->val.value.d;
    }
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.nb")))
    {
        token->not_before = tmp->val.value.d;
    }

    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.si")))
    {
        memcpy(token->signature, tmp->val.value.bin, crypto_sign_BYTES);
        token->is_signature_verified = false;
    }
    else { ret = false;/*Mandatory field*/ }

    // decode extensions
    if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.e")))
    {

        memcpy(token->attributes, tmp->val.value.bin, fmin(tmp->val.size, sizeof(token->attributes)));

        if (ret && NULL != (tmp = np_tree_find_str(data, "np.t.sie")))
        {
            memcpy(token->attributes_signature, tmp->val.value.bin, fmin(tmp->val.size, crypto_sign_BYTES));
            token->is_signature_attributes_verified = false;
        }
        else { ret = false;/*Mandatory field if extensions provided*/ }
    }

    _np_aaatoken_update_scope(token);

    return ret;
}

void _np_aaatoken_update_scope(np_aaatoken_t* self) {

    assert (NULL != self);

    if(self->private_key_is_set){
        self->scope = np_aaatoken_scope_private;
    } else {
        self->scope = np_aaatoken_scope_public;
    }
}

np_dhkey_t np_aaatoken_get_fingerprint(np_aaatoken_t* self, bool include_extensions)
{
    assert (NULL != self);
    // np_ctx_memory(self);
    np_dhkey_t ret;

	// build a hash to find a place in the dhkey table, not for signing !
	unsigned char* hash_attributes = _np_aaatoken_get_hash(self);
	ASSERT(hash_attributes != NULL, "cannot sign NULL hash");

	unsigned char hash[crypto_generichash_BYTES] = { 0 };
	crypto_generichash_state gh_state;
	crypto_generichash_init(&gh_state, NULL, 0, crypto_generichash_BYTES);
	crypto_generichash_update(&gh_state, hash_attributes, crypto_generichash_BYTES);
	crypto_generichash_update(&gh_state, self->signature, crypto_sign_BYTES);

	if (true == include_extensions) {
		unsigned char* hash = __np_aaatoken_get_attributes_hash(self);
		crypto_generichash_update(&gh_state, hash, crypto_generichash_BYTES);
		free(hash);
	}
	// TODO: generichash_final already produces the dhkey value, just memcpy it.
	crypto_generichash_final(&gh_state, hash, crypto_generichash_BYTES);

	char key[crypto_generichash_BYTES * 2 + 1];
	sodium_bin2hex(key, crypto_generichash_BYTES * 2 + 1, hash, crypto_generichash_BYTES);
	ret = np_dhkey_create_from_hash(key);

	free(hash_attributes);
    // }
    return ret;
}

bool _np_aaatoken_is_valid(np_aaatoken_t* token, enum np_aaatoken_type expected_type)
{
    log_trace_msg(LOG_TRACE | LOG_AAATOKEN, "start: bool _np_aaatoken_is_valid(np_aaatoken_t* token){");

    if (NULL == token) return false;

    np_state_t* context = np_ctx_by_memory(token);

    log_debug(LOG_AAATOKEN, "checking token (%s) validity for token of type %"PRIu32" and scope %"PRIu32, token->uuid, token->type, token->scope);


    if (FLAG_CMP(token->type, expected_type) == false)
    {
        log_warn(LOG_AAATOKEN, "token (%s) for subject \"%s\": is not from correct type (%"PRIu32" != (expected:=)%"PRIu32"). verification failed",
            token->uuid, token->subject, token->type, expected_type);
#ifdef DEBUG
        ASSERT(false, "token (%s) for subject \"%s\": is not from correct type (%"PRIu32" != (expected:=)%"PRIu32"). verification failed",
            token->uuid, token->subject, token->type, expected_type);
#endif // DEBUG

        token->state &= AAA_INVALID;
        log_trace_msg(LOG_AAATOKEN, ".end  .token_is_valid");
        return (false);
    }
    else
    {
        log_debug(LOG_AAATOKEN, "token has expected type");
    }


    // check timestamp
    double now = np_time_now();
    if (now > (token->expires_at))
    {
        log_msg(LOG_WARN, "token (%s) for subject \"%s\": expired (%f = %f - %f). verification failed",
                token->uuid, token->subject, token->expires_at - now, now, token->expires_at);
        token->state &= AAA_INVALID;
        log_trace_msg(LOG_AAATOKEN | LOG_TRACE, ".end  .token_is_valid");
        return (false);
    }
    else
    {
        log_debug_msg(LOG_AAATOKEN, "token has not expired");
    }

    if (token->scope > np_aaatoken_scope_private_available) 
    {
        if (token->is_signature_verified == false) {
            unsigned char* hash = _np_aaatoken_get_hash(token);

            // verify inserted signature first
            unsigned char* signature = token->signature;

            log_debug_msg(LOG_AAATOKEN, "try to check signature checksum");
            int ret = crypto_sign_verify_detached((unsigned char*)signature, hash, crypto_generichash_BYTES, token->crypto.ed25519_public_key);

#ifdef DEBUG
            char signature_hex[crypto_sign_BYTES * 2 + 1] = { 0 };
            sodium_bin2hex(signature_hex, crypto_sign_BYTES * 2 + 1,
                signature, crypto_sign_BYTES);

            char pk_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1] = { 0 };
            sodium_bin2hex(pk_hex, crypto_sign_PUBLICKEYBYTES * 2 + 1,
                token->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES);
            char kx_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1] = { 0 };
            sodium_bin2hex(kx_hex, crypto_sign_PUBLICKEYBYTES * 2 + 1,
            		token->crypto.derived_kx_public_key, crypto_sign_PUBLICKEYBYTES);

            log_debug_msg(LOG_AAATOKEN | LOG_DEBUG,
                "(token: %s) signature is%s valid: (pk: 0x%s) sig: 0x%s = %"PRId32,
                token->uuid, ret != 0? " not":"", pk_hex, signature_hex, ret);
#endif
            free(hash);

            if (ret < 0)
            {
                log_msg(LOG_WARN, "token (%s) for subject \"%s\": checksum verification failed", token->uuid, token->subject);
                log_trace_msg(LOG_AAATOKEN | LOG_TRACE, ".end  .token_is_valid");
                token->state &= AAA_INVALID;
                return (false);
            }
            log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "token (%s) for subject \"%s\": checksum verification success", token->uuid, token->subject);
            token->is_signature_verified = true;
        }

        if (token->is_signature_attributes_verified == false)
        {
            // verify inserted signature first
            log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "try to check attribute signature checksum");
            size_t attr_data_size;
            bool ret = np_get_data_size(token->attributes, &attr_data_size) == np_ok;
            if (!ret) {
                log_msg(LOG_WARN, "token (%s) for subject \"%s\": attributes do have no valid structure (total_size)", token->uuid, token->subject);
            }
            unsigned char * hash = __np_aaatoken_get_attributes_hash(token);
            ret = ret && crypto_sign_verify_detached(token->attributes_signature, hash , crypto_generichash_BYTES, token->crypto.ed25519_public_key) == 0;
            free(hash);

            if (!ret)
            {
                _np_debug_log_bin(token->attributes_signature, sizeof(token->attributes_signature), LOG_AAATOKEN, "token (%s) attribute signature: %s",token->uuid)
                log_msg(LOG_WARN, "token (%s) for subject \"%s\": attribute signature checksum verification failed", token->uuid, token->subject);
                log_trace_msg(LOG_AAATOKEN | LOG_TRACE, ".end  .token_is_valid");
                token->state &= AAA_INVALID;

                return (false);
            }
            log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "token (%s) for subject \"%s\": attribute signature checksum verification success", token->uuid, token->subject);
            token->is_signature_attributes_verified = true;
        }
    }
    /*
        If we received a full token we may already got a handshake token,
        if so we need to validate the new tokens signature against the already received token sig
        and an successfully verifying the new token identity is the same as in the handshake token
    */
    if (FLAG_CMP(token->type, np_aaatoken_type_node))
    {   // check for already received handshake token
		np_dhkey_t handshake_token_dhkey = np_aaatoken_get_fingerprint(token, false);
        np_key_t* handshake_key = _np_keycache_find(context, handshake_token_dhkey);
        if (handshake_key != NULL)
        {
            np_aaatoken_t* existing_token = _np_key_get_token(handshake_key);
            if (existing_token != NULL && existing_token != token /* reference compare! */ &&
                FLAG_CMP(existing_token->type, np_aaatoken_type_handshake) /*&& _np_aaatoken_is_valid(handshake_token)*/)
            {   // FIXME: Change to signature check with other tokens pub key
                if (memcmp(existing_token->crypto.derived_kx_public_key, token->crypto.derived_kx_public_key, crypto_sign_PUBLICKEYBYTES *(sizeof(unsigned char))) != 0) 
                {
                    np_unref_obj(np_key_t, handshake_key, "_np_keycache_find");
                    log_msg(LOG_WARN, "Someone tried to impersonate a token (%s). verification failed", token->uuid);
                    return (false);
                }
            }
            np_unref_obj(np_key_t, handshake_key, "_np_keycache_find");
        }
    }

    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "token checksum verification completed");

    if(FLAG_CMP(token->type,np_aaatoken_type_message_intent)){
        log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "try to find max/msg threshold ");

        np_data_value max_threshold, msg_threshold;
        uint32_t max_threshold_ret=-1, msg_threshold_ret=-1;
        if ((max_threshold_ret = np_get_data(token->attributes, "max_threshold", NULL, &max_threshold)) == np_ok &&
            (msg_threshold_ret = np_get_data(token->attributes, "msg_threshold", NULL, &msg_threshold)) == np_ok)
        {
            log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "found max/msg threshold");
            uint32_t token_max_threshold = max_threshold.unsigned_integer;
            uint32_t token_msg_threshold = msg_threshold.unsigned_integer;

            if (0                   <= token_msg_threshold &&
                token_msg_threshold <= token_max_threshold)
            {
                log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "token (%s) for subject \"%s\": %s can be used for %"PRIu32" msgs", token->uuid, token->subject, token->issuer, token_max_threshold-token_msg_threshold);
            }
            else
            {
                log_msg(LOG_WARN, "verification failed. token (%s) for subject \"%s\": %s was already used, 0<=%"PRIu32"<%"PRIu32, token->uuid, token->subject, token->issuer, token_msg_threshold, token_max_threshold);
                log_trace_msg(LOG_AAATOKEN | LOG_TRACE, ".end  .token_is_valid");
                token->state &= AAA_INVALID;
                return (false);
            }
        }else{
            log_warn(LOG_AAATOKEN, "found NO max(%"PRIu32")/msg(%"PRIu32") threshold in token %s / %s", max_threshold_ret, msg_threshold_ret,token->uuid, token->subject);
        }
    }
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "token (%s) validity for subject \"%s\": verification valid", token->uuid, token->subject);
    token->state |= AAA_VALID;
    return (true);
}

np_dhkey_t _np_aaatoken_get_issuer(np_aaatoken_t* self){
    np_dhkey_t ret =
        np_dhkey_create_from_hash(self->issuer);
    return ret;
}

unsigned char* _np_aaatoken_get_hash(np_aaatoken_t* self) {

    assert(self != NULL);// "cannot get token hash of NULL
    np_ctx_memory(self);
    unsigned char* ret = calloc(1, crypto_generichash_BYTES);
    crypto_generichash_state gh_state;
    crypto_generichash_init(&gh_state, NULL, 0, crypto_generichash_BYTES);

    //crypto_generichash_update(&gh_state, (unsigned char*)&self->type, sizeof(uint8_t));
    //log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting type      : %d", self->type);

    ASSERT(self->uuid != NULL, "cannot get token hash of uuid NULL");
    crypto_generichash_update(&gh_state, (unsigned char*)self->uuid, strnlen(self->uuid, NP_UUID_BYTES));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting uuid      : %s", self->uuid);

    crypto_generichash_update(&gh_state, (unsigned char*)self->realm, strnlen(self->realm, 255));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting realm     : %s", self->realm);

    crypto_generichash_update(&gh_state, (unsigned char*)self->issuer, strnlen(self->issuer, 65));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting issuer    : %s", self->issuer);

    crypto_generichash_update(&gh_state, (unsigned char*)self->subject, strnlen(self->subject, 255));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting subject   : %s", self->subject);

    crypto_generichash_update(&gh_state, (unsigned char*)self->audience, strnlen(self->audience, 255));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting audience  : %s", self->audience);

    crypto_generichash_update(&gh_state, (unsigned char*)self->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES);

#ifdef DEBUG
    char pk_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pk_hex, crypto_sign_PUBLICKEYBYTES * 2 + 1,
        self->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES);
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting public_key: %s", pk_hex);
#else
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting public_key: <...>");
#endif

    // if(FLAG_CMP(self->type, np_aaatoken_type_handshake) == false) {
    // TODO: expires_at in handshake?
    crypto_generichash_update(&gh_state, (unsigned char*)&(self->expires_at), sizeof(double));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting expiration: %f", self->expires_at);
    crypto_generichash_update(&gh_state, (unsigned char*)&(self->issued_at), sizeof(double));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting issued_at : %f", self->issued_at);
    crypto_generichash_update(&gh_state, (unsigned char*)&(self->not_before), sizeof(double));
    log_debug_msg(LOG_AAATOKEN | LOG_DEBUG, "fingerprinting not_before: %f", self->not_before);
    // }
    crypto_generichash_final(&gh_state, ret, crypto_generichash_BYTES);

    _np_debug_log_bin(ret, crypto_generichash_BYTES, LOG_AAATOKEN, "token hash for %s is %s", self->uuid);

    ASSERT(ret != NULL, "generated hash cannot be NULL");
    return ret;
}

int __np_aaatoken_generate_signature(np_state_t* context, unsigned char* hash, unsigned char* private_key, unsigned char* save_to) {

    // unsigned long long signature_len = 0;

    _np_debug_log_bin0(hash, crypto_generichash_BYTES, LOG_AAATOKEN, "token hash key fingerprint: %s");
    _np_debug_log_bin0(private_key, crypto_sign_SECRETKEYBYTES, LOG_AAATOKEN, "token signature private key: %s")

    int ret = crypto_sign_detached(save_to, NULL,
                (const unsigned char*)hash, crypto_generichash_BYTES, private_key);

    ASSERT(ret == 0,  "checksum creation for token failed, using unsigned token");
    return ret;
}


void np_aaatoken_set_partner_fp(np_aaatoken_t*self, np_dhkey_t partner_fp) {
    assert(self != NULL);
    np_ctx_memory(self);

    char id[65]={0};
    _np_dhkey_str(&partner_fp,id);

    uint32_t r = np_set_data(self->attributes,
                                (struct np_data_conf) {
                                    .key = "_np.partner_fp",
                                    .type = NP_DATA_TYPE_STR
                                }, (np_data_value){ .str = id }
                            );
    assert(r == np_ok);
    log_debug(LOG_AAATOKEN, "token (%s) setting \"_np.partner_fp\" result %"PRIu32" to %s", self->uuid, r, id);

    _np_aaatoken_update_attributes_signature(self);
}

np_dhkey_t np_aaatoken_get_partner_fp(np_aaatoken_t* self) {

    assert(self != NULL);
    np_dhkey_t ret = { 0 };

    struct np_data_conf conf;
    np_data_value val;
    enum np_data_return r = np_get_data(self->attributes,"_np.partner_fp", &conf, &val);

    ASSERT(r == np_ok || r == np_key_not_found,"token (%s): \"_np.partner_fp\" extraction error %"PRIu32, self->uuid, r);

    if(np_ok == r){
        ASSERT(conf.data_size == 65,"token (%s): \"_np.partner_fp\" extraction size: %"PRIu32" \n", self->uuid, conf.data_size);

        _np_str_dhkey(val.str, &ret);
    }else{
        _np_str_dhkey(self->issuer, &ret);
    }

    return ret;
}

void _np_aaatoken_set_signature(np_aaatoken_t* self, np_aaatoken_t* signee) {

	assert(self != NULL);
    np_state_t* context = np_ctx_by_memory(self);

    ASSERT(self->crypto.ed25519_public_key_is_set == true, "cannot sign token without public key");

    if (signee != NULL) {
        ASSERT(signee != NULL, "Cannot sign extensions with empty signee");
        ASSERT(signee->private_key_is_set == true, "Cannot sign extensions without private key");
        ASSERT(signee->crypto.ed25519_secret_key_is_set == true, "Cannot sign extensions without private key");
    } else {
        ASSERT(self->scope <= np_aaatoken_scope_private_available, "Cannot sign extensions without a private key");
        ASSERT(self->issuer_token != NULL, "Cannot sign extensions without a private key");
    }

    int ret = 0;

    // create the hash of the core token data
    unsigned char* hash = _np_aaatoken_get_hash(self);

    if (signee == NULL) {
        // set the signature of the token
        ret = __np_aaatoken_generate_signature(context, hash, self->issuer_token->crypto.ed25519_secret_key, self->signature);
    } else {
        // add a field to the exension containing an additional signature
        char signee_token_fp[NP_FINGERPRINT_BYTES*2+1]={0};
        np_dhkey_t my_token_fp = np_aaatoken_get_fingerprint(signee, false);
        _np_dhkey_str(&my_token_fp, signee_token_fp);

        ASSERT( 0 == strncmp(signee_token_fp, self->issuer, NP_FINGERPRINT_BYTES*2),"fingerprint of token and issuer need to be the same. issuer: %s fp: %s",signee_token_fp, self->issuer);

        unsigned char signer_pubsig[crypto_sign_PUBLICKEYBYTES+crypto_sign_BYTES];
        // copy public key
        memcpy(signer_pubsig, signee->crypto.ed25519_public_key, crypto_sign_PUBLICKEYBYTES);
        // add signature of signer to extensions
        ret = __np_aaatoken_generate_signature(context, self->signature, signee->crypto.ed25519_secret_key, signer_pubsig + crypto_sign_PUBLICKEYBYTES );
        // insert into extension table
        struct np_data_conf attr_conf={0};
        attr_conf.type = NP_DATA_TYPE_BIN;
        attr_conf.data_size = sizeof(signer_pubsig);
        strncpy(attr_conf.key, signee_token_fp, 255);
        np_set_data(self->attributes, attr_conf, (np_data_value){ .bin=signer_pubsig});

        _np_aaatoken_update_attributes_signature(self);
    }

    free(hash);

#ifdef DEBUG
    char sign_hex[crypto_sign_BYTES * 2 + 1];
    sodium_bin2hex(sign_hex, crypto_sign_BYTES * 2 + 1, self->signature, crypto_sign_BYTES);
    log_debug_msg(LOG_DEBUG | LOG_AAATOKEN, "signature hash for %s is %s", self->uuid, sign_hex);
#endif

    ASSERT(ret == 0, "Error in token signature creation");
}

void _np_aaatoken_update_attributes_signature(np_aaatoken_t* self) {

    assert(self != NULL);
    ASSERT(self->scope <= np_aaatoken_scope_private_available, "Cannot sign extensions without a private key");
    ASSERT(self->issuer_token != NULL, "Cannot sign extensions without a private key");

    np_ctx_memory(self);

    unsigned char* hash = __np_aaatoken_get_attributes_hash(self);
    int ret = __np_aaatoken_generate_signature(context, hash, self->issuer_token->crypto.ed25519_secret_key, self->attributes_signature);

    ASSERT(ret == 0, "Error in extended token signature creation");

    _np_debug_log_bin(self->attributes_signature, crypto_sign_BYTES, LOG_AAATOKEN,"attribute signature hash for %s is %s", self->uuid)

    free(hash);
}

unsigned char* __np_aaatoken_get_attributes_hash(np_aaatoken_t* self) {
    assert(self != NULL);
    // np_state_t* context = np_ctx_by_memory(self);

    unsigned char* ret = calloc(1, crypto_generichash_BYTES);

    crypto_generichash_state gh_state;
    int c_ret;
    c_ret = crypto_generichash_init(&gh_state, NULL, 0, crypto_generichash_BYTES);
    assert(c_ret==0);

    //unsigned char* hash = np_tree_get_hash(self->extensions);
    //ASSERT(hash != NULL, "cannot sign NULL hash");
    //crypto_generichash_update(&gh_state, hash, crypto_generichash_BYTES);

    // TODO: Maybewe need to validate only till np_get_data_size(self)
    c_ret = crypto_generichash_update(&gh_state, self->attributes, sizeof(self->attributes));
    assert(c_ret==0);
    c_ret = crypto_generichash_update(&gh_state, self->signature, crypto_sign_BYTES);
    assert(c_ret==0);

    c_ret = crypto_generichash_final(&gh_state, ret, crypto_generichash_BYTES);
    assert(c_ret==0);
    //free(hash);

    return ret;
}

void np_aaatoken_ref_list(np_sll_t(np_aaatoken_ptr, sll_list), const char* reason, const char* reason_desc)
{
    np_state_t* context = NULL;

    sll_iterator(np_aaatoken_ptr) iter = sll_first(sll_list);
    while (NULL != iter)
    {
        if (context == NULL && iter->val != NULL) context = np_ctx_by_memory(iter->val);
        np_ref_obj(np_aaatoken_t, (iter->val), reason, reason_desc);
        sll_next(iter);
    }
}
 
void np_aaatoken_unref_list(np_sll_t(np_aaatoken_ptr, sll_list), const char* reason)
{	
    np_state_t* context = NULL;

    sll_iterator(np_aaatoken_ptr) iter = sll_first(sll_list);
    while (NULL != iter)
    {
        if (context == NULL && iter->val!=NULL) context = np_ctx_by_memory(iter->val);
        np_unref_obj(np_aaatoken_t, (iter->val), reason);
        sll_next(iter);
    }
}


#ifdef DEBUG
void _np_aaatoken_trace_info(char* desc, np_aaatoken_t* self) {
    assert(self != NULL);
    np_ctx_memory(self);

    char* info_str = NULL;
    info_str = np_str_concatAndFree(info_str, "%s", desc);

    np_tree_t* data = np_tree_create();
    _np_aaatoken_encode(data, self, false);
    np_tree_elem_t* tmp = NULL;
    bool free_key, free_value;
    char *key, *value;

    char tmp_c[65] = { 0 };
	np_dhkey_t tmp_d = np_aaatoken_get_fingerprint(self, false);
    _np_dhkey_str(&tmp_d, tmp_c);

    info_str = np_str_concatAndFree(info_str, " fingerprint: %s ; TREE: (",tmp_c);
    RB_FOREACH(tmp, np_tree_s, (data))
    {
        key = np_treeval_to_str(tmp->key, &free_key);
        value = np_treeval_to_str(tmp->val, &free_value);
        info_str = np_str_concatAndFree(info_str, "%s:%s |", key, value);
        if (free_value) free(value);
        if (free_key) free(key);
    }	
    np_tree_free(data);
    info_str = np_str_concatAndFree(info_str, "): %s", self->uuid);

    log_debug(LOG_AAATOKEN, "AAATokenTrace_%s", info_str);
    free(info_str);
}
#endif

struct np_token* np_aaatoken4user(struct np_token* dest, np_aaatoken_t* src) {

    assert(src != NULL);
    assert(dest!= NULL);
    np_ctx_memory(src);

    dest->expires_at = src->expires_at;
    dest->issued_at	 = src->issued_at;
    dest->not_before = src->not_before;

    strncpy(dest->uuid, src->uuid, NP_UUID_BYTES);

    // TODO: convert to np_id
    strncpy(dest->issuer, src->issuer, 65);
    strncpy(dest->realm, src->realm, 255);
    strncpy(dest->audience, src->audience, 255);
    strncpy(dest->subject, src->subject, 255);

    assert(crypto_sign_PUBLICKEYBYTES == NP_PUBLIC_KEY_BYTES);
    memcpy(dest->public_key, src->crypto.ed25519_public_key, NP_PUBLIC_KEY_BYTES);
    assert(crypto_sign_SECRETKEYBYTES == NP_SECRET_KEY_BYTES);
    memcpy(dest->secret_key, src->crypto.ed25519_secret_key, NP_SECRET_KEY_BYTES);

    memcpy(dest->signature, src->signature, NP_SIGNATURE_BYTES);

    size_t attr_size;
    if(np_get_data_size(src->attributes,&attr_size) == np_ok && attr_size <= sizeof(src->attributes)) {
        memcpy(dest->attributes, src->attributes, sizeof(src->attributes));
    }
    // else{ TODO: warning/error if NP_EXTENSION_BYTES < src->extensions->byte_size }
    memcpy(dest->attributes_signature, src->attributes_signature, sizeof(dest->attributes_signature));

    return dest;
}

np_aaatoken_t* np_user4aaatoken(np_aaatoken_t* dest, struct np_token* src) {
    assert(src != NULL);
    assert(dest != NULL);
    np_ctx_memory(dest);

    dest->expires_at = src->expires_at;
    dest->issued_at = src->issued_at;
    dest->not_before = src->not_before;

    strncpy(dest->uuid, src->uuid, NP_UUID_BYTES);

    //TODO: convert to np_id
    strncpy(dest->issuer, src->issuer, 65);
    strncpy(dest->realm, src->realm, 255);
    strncpy(dest->audience, src->audience, 255);
    strncpy(dest->subject, src->subject, 255);

    // copy public key
    memcpy(dest->crypto.ed25519_public_key, src->public_key, NP_PUBLIC_KEY_BYTES);
    dest->crypto.ed25519_public_key_is_set = true;

    memcpy(dest->signature, src->signature, NP_SIGNATURE_BYTES);

    ASSERT(sizeof(dest->attributes)==sizeof(src->attributes),"Attribute sizes need to be compatible");
    memcpy(dest->attributes, src->attributes, sizeof(dest->attributes));

    ASSERT(sizeof(dest->attributes_signature)==sizeof(src->attributes_signature),"Signature sizes need to be compatible");
    memcpy(dest->attributes_signature, src->attributes_signature, sizeof(dest->attributes_signature));

    _np_aaatoken_update_scope(dest);

    return dest;
}

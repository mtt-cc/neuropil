/**
 *  copyright 2015 pi-lar GmbH
 *  Stephan Schwichtenberg
 **/

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sodium.h"

#include "neuropil.h"

#include "dtime.h"
#include "log.h"
#include "include.h"
#include "np_aaatoken.h"
#include "np_axon.h"
#include "np_container.h"
#include "np_dendrit.h"
#include "np_glia.h"
#include "np_jobqueue.h"
#include "np_jtree.h"
#include "np_key.h"
#include "np_memory.h"
#include "np_message.h"
#include "np_network.h"
#include "np_node.h"
#include "np_threads.h"
#include "np_route.h"


SPLAY_GENERATE(spt_key, np_key_s, link, key_comp);
RB_GENERATE(rbt_msgproperty, np_msgproperty_s, link, _np_property_comp);


np_bool np_default_authorizefunc (np_state_t* state, np_aaatoken_t* token )
{
	log_msg(LOG_WARN, "using default handler to authorize %s", token->subject );
	log_msg(LOG_WARN, "do you really want the default authorize handler (allow all) ???");
	return TRUE;
}

np_bool np_default_authenticatefunc (np_state_t* state, np_aaatoken_t* token )
{
	log_msg(LOG_WARN, "using default handler to authenticate %s", token->subject);
	log_msg(LOG_WARN, "do you really want the default authenticate handler (trust all) ???");
	return TRUE;
}

np_bool np_default_accountingfunc (np_state_t* state, np_aaatoken_t* token )
{
	log_msg(LOG_WARN, "using default handler to account for %s", token->subject );
	log_msg(LOG_WARN, "do you really want the default accounting handler (account nothing) ???");
	return TRUE;
}

void np_setauthorizing_cb(np_state_t* state, np_aaa_func_t aaaFunc)
{
	log_msg(LOG_INFO, "setting user defined authorization handler, that's good ...");
	state->authorize_func = aaaFunc;
}

void np_setauthenticate_cb(np_state_t* state, np_aaa_func_t aaaFunc)
{
	log_msg(LOG_INFO, "setting user defined authentication handler, that's good ...");
	state->authenticate_func = aaaFunc;
}

void np_setaccounting_cb(np_state_t* state, np_aaa_func_t aaaFunc)
{
	log_msg(LOG_INFO, "setting user defined accounting handler, that's good ...");
	state->accounting_func = aaaFunc;
}

void np_waitforjoin(const np_state_t* state)
{
	while (FALSE == state->my_node_key->node->joined_network)
	{
		dsleep(0.31415);
	}
}

void np_set_listener (np_state_t* state, np_usercallback_t msg_handler, char* subject)
{
	// check whether an handler already exists
	np_msgproperty_t* msg_prop = np_message_get_handler(state, INBOUND, subject);

	if (NULL == msg_prop)
	{
		// create a default set of properties for listening to messages
		np_new_obj(np_msgproperty_t, msg_prop);
		msg_prop->msg_subject = strndup(subject, 255);
		msg_prop->mode_type = INBOUND;
		msg_prop->clb = np_callback_wrapper;
		msg_prop->user_clb = msg_handler;
		np_message_register_handler(state, msg_prop);
	}

	// update informations somewhere in the network
	np_send_msg_interest(state, subject);
}

void np_set_identity(np_state_t* state, np_aaatoken_t* identity)
{
    np_key_t* my_identity_key = NULL;

    unsigned char hash_key[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_state* hash_state = NULL;
    crypto_hash_sha256_init(hash_state);
	crypto_hash_sha256_update(hash_state, (const unsigned char*) identity->realm, strlen(identity->realm));
	crypto_hash_sha256_update(hash_state, (const unsigned char*) identity->issuer, strlen(identity->issuer));
	crypto_hash_sha256_update(hash_state, (const unsigned char*) identity->subject, strlen(identity->subject));
	crypto_hash_sha256_final(hash_state, hash_key);
	char key[65];
	sodium_bin2hex(key, 65, hash_key, 32);

	np_key_t* search_key = key_create_from_hash(key);
	LOCK_CACHE(state)
	{
		my_identity_key = SPLAY_FIND(spt_key, &state->key_cache, search_key);
		if (NULL == my_identity_key )
		{
			SPLAY_INSERT(spt_key, &state->key_cache, search_key);
			my_identity_key = search_key;
			np_ref_obj(np_key_t, my_identity_key);
	    }
		else
		{
	    	np_free_obj(np_key_t, search_key);
	    }
	}

	if (NULL != state->my_identity)
	{
		// delete old identity
		np_unref_obj(np_key_t, state->my_identity);
	}

	if (NULL != my_identity_key)
	{
		// cannot be null, but checker complains
		state->my_identity = my_identity_key;
		state->my_identity->authentication = identity;
	}

    // create encryption parameter
	crypto_sign_keypair(identity->public_key, identity->private_key);
}

void np_set_mx_property(np_state_t* state, char* subject, const char* key, np_jval_t value)
{
	np_msgproperty_t* msg_prop = np_message_get_handler(state, OUTBOUND, subject);
	if (NULL == msg_prop)
	{
		np_new_obj(np_msgproperty_t, msg_prop);
		msg_prop->msg_subject = strndup(subject, 255);
		msg_prop->clb = hnd_msg_out_send;

		np_message_register_handler(state, msg_prop);
	}
}

void np_rem_mx_property(np_state_t* state, char* subject, const char* key)
{
	np_msgproperty_t* msg_prop = np_message_get_handler(state, OUTBOUND, subject);
	if (NULL == msg_prop)
	{
		return;
	}
}


void np_send (np_state_t* state, char* subject, char *data, uint32_t seqnum)
{
	np_msgproperty_t* msg_prop = np_message_get_handler(state, OUTBOUND, subject);
	if (NULL == msg_prop)
	{
		np_new_obj(np_msgproperty_t, msg_prop);
		msg_prop->msg_subject = strndup(subject, 255);
		msg_prop->mep_type = ONE_WAY;
		msg_prop->mode_type = OUTBOUND;
		msg_prop->clb = hnd_msg_out_send;

		np_message_register_handler(state, msg_prop);
	}

	np_message_t* msg = NULL;
	np_new_obj(np_message_t, msg);

	jrb_insert_str(msg->header, NP_MSG_HEADER_SUBJECT, new_jval_s((char*) subject));
	jrb_insert_str(msg->header, NP_MSG_HEADER_FROM, new_jval_s((char*) key_get_as_string(state->my_node_key)));
	jrb_insert_str(msg->body,   NP_MSG_BODY_TEXT, new_jval_s(data));

	jrb_insert_str(msg->properties, NP_MSG_INST_SEQ, new_jval_ul(seqnum));

	msg_prop->msg_threshold++;
	np_send_msg_availability(state, subject);

	np_send_msg(state, subject, msg, msg_prop);

	np_free_obj(np_message_t, msg);
}

uint32_t np_receive (np_state_t* state, char* subject, char **data)
{
	// send out that we want to receive messages
	np_msgproperty_t* msg_prop = np_message_get_handler(state, INBOUND, subject);
	if (NULL == msg_prop)
	{
		np_new_obj(np_msgproperty_t, msg_prop);
		msg_prop->msg_subject = strndup(subject, 255);
		msg_prop->mep_type = ONE_WAY;
		msg_prop->mode_type = INBOUND;
		msg_prop->clb = np_signal;
		// when creating, set to zero because callback function is not used
		msg_prop->max_threshold = 0;

		// register the handler so that message can be received
		np_message_register_handler(state, msg_prop);
	}
	msg_prop->max_threshold++;

	np_send_msg_interest(state, subject);

	np_aaatoken_t* sender_token = NULL;
	np_message_t* msg = NULL;
	char* sender_id = NULL;
	np_bool msg_received = FALSE;

	do
	{	// first check or wait for available messages
		if (0 == sll_size(msg_prop->msg_cache))
		{
			LOCK_CACHE(msg_prop)
			{
				log_msg(LOG_DEBUG, "waiting for signal that a new message arrived %p", msg_prop);
				pthread_cond_wait(&msg_prop->msg_received, &msg_prop->lock);
				log_msg(LOG_DEBUG, "received signal that a new message arrived %p", msg_prop);
			}
		}
		msg = sll_first(msg_prop->msg_cache)->val;

		// next check or wait for valid sender tokens
		sender_id = jrb_find_str(msg->header, NP_MSG_HEADER_FROM)->val.value.s;
		sender_token = np_get_sender_token(state, subject, sender_id);
		if (NULL == sender_token)
		{
			// sleep for a while, token may need some time to arrive
			dsleep(0.31415);
			continue;
		}

		msg_received = TRUE;

	} while (FALSE == msg_received);

	msg_prop->msg_threshold--;

	// in receive function, we can only receive one message per call, different for callback function
	log_msg(LOG_DEBUG, "received message from cache %p ( cache-size: %d)", msg_prop, sll_size(msg_prop->msg_cache));
	msg = sll_head(np_message_t, msg_prop->msg_cache);

	log_msg(LOG_DEBUG, "decrypting message ...");
	np_bool decrypt_ok = np_message_decrypt_payload(state, msg, sender_token);

	if (FALSE == decrypt_ok)
	{
		log_msg(LOG_DEBUG, "decryption of message failed, deleting message");

		np_unref_obj(np_message_t, msg);
		msg_prop->max_threshold--;

		return 0;
	}

	uint32_t received = jrb_find_str(msg->properties, NP_MSG_INST_SEQ)->val.value.ul;
	np_jtree_elem_t* reply_data = jrb_find_str(msg->body, NP_MSG_BODY_TEXT);
	*data = strndup(reply_data->val.value.s, strlen(reply_data->val.value.s));

	uint8_t ack_mode = jrb_find_str(msg->instructions, NP_MSG_INST_ACK)->val.value.ush;
	if (0 < (ack_mode & ACK_DESTINATION))
	{
		np_send_ack(state, msg);
	}

	np_unref_obj(np_message_t, msg);
	msg_prop->max_threshold--;

	log_msg(LOG_INFO, "someone sending us messages %s !!!", *data);

	return received;
}


void np_send_ack(np_state_t* state, np_message_t* in_msg)
{
	// uint8_t ack = ACK_NONE;
	uint32_t seq = 0;
	char* uuid = NULL;

	// np_message_t* in_msg = args->msg;

	if (NULL != jrb_find_str(in_msg->header, NP_MSG_INST_ACK_TO))
	{
		// extract data from incoming message
		seq = jrb_find_str(in_msg->instructions, NP_MSG_INST_SEQ)->val.value.ul;
		// ack = jrb_find_str(in_msg->instructions, NP_MSG_INST_ACK)->val.value.ush;
		uuid = jrb_find_str(in_msg->instructions, NP_MSG_INST_UUID)->val.value.s;

		// create new ack message & handlers
		// np_node_t* ack_node = np_node_decode_from_str(state->nodes, jrb_find_str(in_msg->header, NP_MSG_HEADER_REPLY_TO)->val.value.s);
		np_key_t* ack_key = key_create_from_hash(
				jrb_find_str(in_msg->header, NP_MSG_INST_ACK_TO)->val.value.s);

		np_message_t* ack_msg = NULL;
		np_new_obj(np_message_t, ack_msg);

		np_msgproperty_t* prop = np_message_get_handler(state, TRANSFORM, ROUTE_LOOKUP);

		np_message_create(ack_msg, ack_key, state->my_node_key, NP_MSG_ACK, NULL);
		jrb_insert_str(ack_msg->instructions, NP_MSG_INST_ACK, new_jval_ush(prop->ack_mode));
		jrb_insert_str(ack_msg->instructions, NP_MSG_INST_ACKUUID, new_jval_s(uuid));
		jrb_insert_str(ack_msg->instructions, NP_MSG_INST_SEQ, new_jval_ul(seq));
		// send the ack out
		job_submit_msg_event(state->jobq, 0.0, prop, ack_key, ack_msg);

		np_free_obj(np_message_t, ack_msg);
		np_free_obj(np_key_t, ack_key);
	}
}


/**
 ** np_ping:
 ** sends a PING message to another node. The message is acknowledged in network layer.
 **/
void np_ping (np_state_t* state, np_key_t* key)
{
	/* weired: assume failure of the node now, will be reset with ping reply later */
	if (NULL != key->node)
	{
		key->node->failuretime = dtime();
		np_node_update_stat(key->node, 0);
	}

    np_message_t* out_msg = NULL;
    np_new_obj(np_message_t, out_msg);

    np_message_create (out_msg, key, state->my_node_key, NP_MSG_PING_REQUEST, NULL);
    log_msg(LOG_DEBUG, "ping request to: %s", key_get_as_string(key));

    np_msgproperty_t* prop = np_message_get_handler(state, OUTBOUND, NP_MSG_PING_REQUEST);
	job_submit_msg_event(state->jobq, 0.0, prop, key, out_msg);

	np_free_obj(np_message_t, out_msg);
}

/**
 ** np_init:
 ** initializes neuropil on specified port and returns the const np_state_t* which
 ** contains global state of different neuropil modules.
 **/
np_state_t* np_init(char* proto, char* port)
{
    char name[256];

    // encryption and memory protection
    sodium_init();
    // memory pool
	np_mem_init();
    // np_printpool;

	// initialize key min max ranges
    key_init();

    // global neuropil structure
    np_state_t *state = (np_state_t *) malloc (sizeof (np_state_t));
    if (state == NULL)
	{
    	log_msg(LOG_ERROR, "neuropil_init: state module not created: %s", strerror (errno));
	    exit(1);
	}
    // splay tree initializing
    SPLAY_INIT(&state->key_cache);

    //
    // TODO: read my own identity from file, if a password is given
    //
    // set default aaa functions
    state->authorize_func = np_default_authorizefunc;
    state->authenticate_func = np_default_authenticatefunc;
    state->accounting_func = np_default_accountingfunc;

    pthread_mutex_init (&state->lock, NULL);

	char* np_service = "3141";
	uint8_t np_proto = UDP | IPv6;

	if (NULL != port)
		np_service = port;

	if (NULL != proto)
	{
		np_proto = np_parse_protocol_string(proto);
		log_msg(LOG_DEBUG, "now initializing networking for %s:%s", proto, np_service);
	}
	else
	{
		log_msg(LOG_DEBUG, "now initializing networking for udp6://%s", np_service);
	}

	np_node_t* me = NULL;
    np_new_obj(np_node_t, me);

    // listen on all network interfaces
	me->network = network_init(TRUE, np_proto, NULL, np_service);
	if (NULL == me->network->addr_in)
	{
    	log_msg(LOG_ERROR, "neuropil_init: network_init failed, see log for details");
	    exit(1);
	}
	np_node_update(me, np_proto, me->network->addr_in->ai_canonname, np_service);
	log_msg(LOG_DEBUG, "neuropil_init: network_init for %s:%s:%s",
			           np_get_protocol_string(me->protocol), me->dns_name, me->port);

	state->my_node_key = key_create_from_hostport(me->dns_name, me->port);
    np_ref_obj(np_key_t, state->my_node_key);

	log_msg(LOG_WARN, "node_key %p", state->my_node_key);
	SPLAY_INSERT(spt_key, &state->key_cache, state->my_node_key);

    state->my_node_key->node = me;

    // create a new token for encryption each time neuropil starts
    np_aaatoken_t* auth_token = NULL;
    np_new_obj(np_aaatoken_t, auth_token);
    // crypto_box_keypair(auth_token->public_key, auth_token->private_key); // curve25519xsalsa20poly1305
    crypto_sign_keypair(auth_token->public_key, auth_token->private_key);   // ed25519
    // crypto_scalarmult_base(); // curve25519

	strncpy(auth_token->issuer, (char*) key_get_as_string(state->my_node_key), 255);
	// TODO: aaa subject should be user set-able, could also be another name
	snprintf(auth_token->subject, 255, "%s:%s", name, port);
    auth_token->valid = TRUE;

    state->my_node_key->authentication = auth_token;

    // set and ref additional identity
    state->my_identity = state->my_node_key;
    np_ref_obj(np_key_t, state->my_identity);

    // initialize routing table
    state->routes = route_init (state->my_node_key);
    if (state->routes == NULL)
	{
    	log_msg(LOG_ERROR, "neuropil_init: route_init failed: %s", strerror (errno));
	    exit(1);
	}

    // initialize job queue
    state->jobq = job_queue_create ();
    if (state->jobq == NULL)
	{
    	log_msg(LOG_ERROR, "neuropil_init: job_queue_create failed: %s", strerror (errno));
	    exit(1);
	}

    // initialize message handling system
    _np_message_init (state);
    if (state->msg_tokens == NULL)
	{
    	log_msg(LOG_ERROR, "neuropil_init: message_init failed: %s", strerror (errno));
	    exit(1);
	}

    state->msg_part_cache = make_jtree();

    // initialize real network layer last
    // initialize network reading
    job_submit_event(state->jobq, 0.0, np_network_read);
    // initialize retransmission of packets
    job_submit_event(state->jobq, 0.0, np_cleanup);
    // start leafset checking jobs
    job_submit_event(state->jobq, 0.0, np_check_leafset);
    job_submit_event(state->jobq, 0.0, np_write_log);
    job_submit_event(state->jobq, 0.0, np_retransmit_tokens);

	log_msg(LOG_INFO, "neuropil successfully initialized: %s", key_get_as_string(state->my_node_key));
	log_fflush();

	return state;
}

void np_start_job_queue(np_state_t* state, uint8_t pool_size)
{
	if (pthread_attr_init (&state->attr) != 0)
	{
	    log_msg (LOG_ERROR, "pthread_attr_init: %s", strerror (errno));
	    return;
	}

    if (pthread_attr_setscope (&state->attr, PTHREAD_SCOPE_SYSTEM) != 0)
	{
	    log_msg (LOG_ERROR, "pthread_attr_setscope: %s", strerror (errno));
	    return;
	}

    if (pthread_attr_setdetachstate (&state->attr, PTHREAD_CREATE_DETACHED) != 0)
	{
    	log_msg (LOG_ERROR, "pthread_attr_setdetachstate: %s", strerror (errno));
	    return;
	}

    state->thread_ids = (pthread_t *) malloc (sizeof (pthread_t) * pool_size);

    /* create the thread pool */
    for (uint8_t i = 0; i < pool_size; i++)
    {
        pthread_create (&state->thread_ids[i], &state->attr, job_exec, (void *) state);
    	log_msg(LOG_DEBUG, "neuropil thread started: %p", state->thread_ids[i]);
   	}
}


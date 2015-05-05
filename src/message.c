/*
 ** $Id: message.c,v 1.37 2007/04/04 00:04:49 krishnap Exp $
 **
 ** Matthew Allen
 ** description:
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "sodium.h"

#include "cmp.h"

#include "message.h"

#include "neuropil.h"
#include "log.h"
#include "node.h"
#include "network.h"
#include "jval.h"
#include "threads.h"
#include "route.h"
#include "job_queue.h"
#include "np_dendrit.h"
#include "np_axon.h"
#include "np_glia.h"
#include "np_util.h"
#include "jrb.h"


// default message type enumeration
enum {
	NEUROPIL_PING_REQUEST = 1,
	NEUROPIL_PING_REPLY,

	NEUROPIL_JOIN = 10,
	NEUROPIL_JOIN_ACK,
	NEUROPIL_JOIN_NACK,

	NEUROPIL_AVOID = 20,
	NEUROPIL_DIVORCE,

	NEUROPIL_UPDATE = 30,
	NEUROPIL_PIGGY,

	NEUROPIL_MSG_INTEREST = 50,
	NEUROPIL_MSG_AVAILABLE,

	NEUROPIL_REST_OPERATIONS = 100,
	NEUROPIL_POST,   /*create*/
	NEUROPIL_GET,    /*read*/
	NEUROPIL_PUT,    /*update*/
	NEUROPIL_DELETE, /*delete*/
	NEUROPIL_QUERY,

	NEUROPIL_INTERN_MAX = 1024,
	NEUROPIL_DATA = 1025,

} message_enumeration;


#define NR_OF_ELEMS(x)  (sizeof(x) / sizeof(x[0]))

np_msgproperty_t np_internal_messages[] =
{
	{ ROUTE_LOOKUP, TRANSFORM, DEFAULT_TYPE, 5, 1, 0, "", np_route_lookup }, // default input handling func should be "route_get" ?
	{ NP_MSG_PIGGY_REQUEST, TRANSFORM, DEFAULT_TYPE, 5, 1, 0, "", np_send_rowinfo }, // default input handling func should be "route_get" ?

	{ DEFAULT, INBOUND, DEFAULT_TYPE, 5, 1, 0, "", hnd_msg_in_received },
	// TODO: add garbage collection output
	{ DEFAULT, OUTBOUND, DEFAULT_TYPE, 5, 1, 0, "", hnd_msg_out_send },

	{ NP_MSG_HANDSHAKE, INBOUND, ONEWAY, 5, 0, 0, "", hnd_msg_in_handshake },
	{ NP_MSG_HANDSHAKE, OUTBOUND, ONEWAY, 5, 0, 0, "", hnd_msg_out_handshake },

	{ NP_MSG_ACK, INBOUND, ONEWAY, 5, 0, 0, "", NULL }, // incoming ack handled in network layer, not required
	{ NP_MSG_ACK, OUTBOUND, ONEWAY, 5, 0, 0, "", hnd_msg_out_ack },

	{ NP_MSG_PING_REQUEST, INBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_in_ping },
	{ NP_MSG_PING_REPLY, INBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_in_pingreply },
	{ NP_MSG_PING_REQUEST, OUTBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_out_send },
	{ NP_MSG_PING_REPLY, OUTBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_out_send },

	// join request: node unknown yet, therefore send without ack, explicit ack handling via extra messages
	{ NP_MSG_JOIN_REQUEST, INBOUND, ONEWAY, 5, 2, 5, "C", hnd_msg_in_join_req }, // just for controller ?
	{ NP_MSG_JOIN_REQUEST, OUTBOUND, ONEWAY, 5, 2, 5, "C", hnd_msg_out_send },
	{ NP_MSG_JOIN_ACK, INBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_in_join_ack },
	{ NP_MSG_JOIN_ACK, OUTBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_out_send },
	{ NP_MSG_JOIN_NACK, INBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_in_join_nack },
	{ NP_MSG_JOIN_NACK, OUTBOUND, ONEWAY, 5, 1, 5, "", hnd_msg_out_send },

	{ NP_MSG_PIGGY_REQUEST, INBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_in_piggy },
	{ NP_MSG_PIGGY_REQUEST, OUTBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_out_send },
	{ NP_MSG_UPDATE_REQUEST, INBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_in_update },
	{ NP_MSG_UPDATE_REQUEST, OUTBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_out_send },

	{ NP_MSG_INTEREST, INBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_in_interest },
	{ NP_MSG_AVAILABLE, INBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_in_available },
	{ NP_MSG_INTEREST, OUTBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_out_send },
	{ NP_MSG_AVAILABLE, OUTBOUND, ONEWAY, 5, 1, 5, "C", hnd_msg_out_send }
};

np_message_t* np_message_create_empty() {

	np_message_t* tmp = (np_message_t*) malloc(sizeof(np_message_t));

	tmp->header = make_jrb();
	// log_msg(LOG_DEBUG, "header now (%p: %p->%p)", tmp, tmp->header, tmp->header->flink);

	tmp->properties = make_jrb();
	// log_msg(LOG_DEBUG, "properties now (%p: %p->%p)", tmp, tmp->properties, tmp->properties->flink);
	tmp->instructions = make_jrb();
	// log_msg(LOG_DEBUG, "instructions now (%p: %p->%p)", tmp, tmp->instructions, tmp->instructions->flink);
	tmp->body = make_jrb();
	// log_msg(LOG_DEBUG, "body now (%p: %p->%p)", tmp, tmp->body, tmp->body->flink);
	tmp->footer = make_jrb();
	// log_msg(LOG_DEBUG, "footer now (%p: %p->%p)", tmp, tmp->footer, tmp->footer->flink);

	return tmp;
}

int np_message_serialize(np_message_t* msg, void* target, unsigned long* out_size) {

    cmp_ctx_t cmp;
    cmp_init(&cmp, target, buffer_reader, buffer_writer);

	np_jrb_t* node = NULL;

	cmp_write_array(&cmp, 5);

	// log_msg(LOG_DEBUG, "serializing the header (size %d)", msg->header->size);
	if (!cmp_write_map(&cmp, msg->header->size*2 )) log_msg(LOG_WARN, cmp_strerror(&cmp));
	if (msg->header->size) {
		node = msg->header;
		// log_msg(LOG_DEBUG, "for (%p; %p!=%p; %p=%p) ", msg->header->flink, node, msg->header, node, node->flink);
		jrb_traverse(node, msg->header) {
			serialize_jrb_node_t(node, &cmp);
		}
	}

	// log_msg(LOG_DEBUG, "serializing the instructions (size %d)", msg->header->size);
	if (!cmp_write_map(&cmp, msg->instructions->size*2 ))  log_msg(LOG_WARN, cmp_strerror(&cmp));
	if (msg->instructions->size) {
		jrb_traverse(node, msg->instructions) {
			serialize_jrb_node_t(node, &cmp);
		}
	}

	// log_msg(LOG_DEBUG, "serializing the properties (size %d)", msg->properties->size);
	if (!cmp_write_map(&cmp, msg->properties->size*2 )) log_msg(LOG_WARN, cmp_strerror(&cmp));
	if (msg->properties->size) {
		jrb_traverse(node, msg->properties) {
			serialize_jrb_node_t(node, &cmp);
		}
	}

	// log_msg(LOG_DEBUG, "serializing the body (size %d)", msg->body->size);
	if (!cmp_write_map(&cmp, msg->body->size*2 ))  log_msg(LOG_WARN, cmp_strerror(&cmp));
	if (msg->body->size) {
		jrb_traverse(node, msg->body) {
			serialize_jrb_node_t(node, &cmp);
		}
	}

	// log_msg(LOG_DEBUG, "serializing the footer (size %d)", msg->footer->size);
	if (!cmp_write_map(&cmp, msg->footer->size*2 )) log_msg(LOG_WARN, cmp_strerror(&cmp));
	if (msg->footer->size) {
		jrb_traverse(node, msg->footer) {
			serialize_jrb_node_t(node, &cmp);
		}
	}

	// target = buffer;
	*out_size = cmp.buf-target;
	return 1;
}

np_message_t* np_message_deserialize(void* buffer) {

	np_message_t* tmp = np_message_create_empty();

	cmp_ctx_t cmp;
	cmp_init(&cmp, buffer, buffer_reader, buffer_writer);

	unsigned int array_size;
	if (!cmp_read_array(&cmp, &array_size)) return NULL;
	if (array_size != 5) {
		log_msg(LOG_WARN, "wrong array size when deserializing message");
		return NULL;
	}

	unsigned int map_size = 0;
	log_msg(LOG_DEBUG, "deserializing msg header");
	if (!cmp_read_map(&cmp, &map_size)) return NULL;
	if (map_size) {
		deserialize_jrb_node_t(tmp->header, &cmp, map_size/2);
		if ( (map_size/2) != tmp->header->size)
			log_msg(LOG_WARN, "error deserializing header, continuing ... ");
	}

	log_msg(LOG_DEBUG, "deserializing msg instructions");
	if (!cmp_read_map(&cmp, &map_size)) return NULL;
	if (map_size) {
		deserialize_jrb_node_t(tmp->instructions, &cmp, map_size/2);
		if ( (map_size/2) != tmp->instructions->size)
			log_msg(LOG_WARN, "error deserializing instructions, continuing ... ");
	}

	log_msg(LOG_DEBUG, "deserializing msg properties");
	if (!cmp_read_map(&cmp, &map_size)) return NULL;
	if (map_size) {
		deserialize_jrb_node_t(tmp->properties, &cmp, map_size/2);
		if ( (map_size/2) != tmp->properties->size)
			log_msg(LOG_WARN, "error deserializing properties, continuing ... ");
	}

	log_msg(LOG_DEBUG, "deserializing msg body");
	if (!cmp_read_map(&cmp, &map_size)) return NULL;
	if (map_size) {
		deserialize_jrb_node_t(tmp->body, &cmp, map_size/2);
		if ( (map_size/2) != tmp->body->size)
			log_msg(LOG_WARN, "error deserializing body, continuing ... ");
	}

	log_msg(LOG_DEBUG, "deserializing msg footer");
	if (!cmp_read_map(&cmp, &map_size)) return NULL;
	if (map_size) {
		deserialize_jrb_node_t(tmp->footer, &cmp, map_size/2);
		if ( (map_size/2) != tmp->footer->size)
			log_msg(LOG_WARN, "error deserializing footer, continuing ... ");
	}
	return tmp;
}

/** 
 ** message_create: 
 ** creates the message to the destination #dest# the message format would be like:
 **  [ type ] [ size ] [ key ] [ data ]. It return the created message structure.
 */
np_message_t* np_message_create(np_messageglobal_t *mg, np_key_t* to, np_key_t* from, const char* subject, np_jrb_t* the_data)
{
	np_message_t* new_msg = np_message_create_empty();
	// pn_message_t* new_msg = pn_message();
	jrb_insert_str(new_msg->header, NP_MSG_HEADER_TO,  new_jval_s((char*) key_get_as_string(to)));
	jrb_insert_str(new_msg->header, NP_MSG_HEADER_FROM, new_jval_s((char*) key_get_as_string(from)));
	jrb_insert_str(new_msg->header, NP_MSG_HEADER_REPLY_TO, new_jval_s((char*) key_get_as_string(from)));
	jrb_insert_str(new_msg->header, NP_MSG_HEADER_SUBJECT,  new_jval_s((char*) subject));

//	pn_message_set_address(new_msg, (char*) key_get_as_string(to));
//	pn_message_set_reply_to(new_msg, (char*) key_get_as_string(from));
//	pn_message_set_subject(new_msg, subject);

	if (the_data != NULL) {
		np_message_setbody(new_msg, the_data);
	}
	return new_msg;
}

inline void np_message_setproperties(np_message_t* msg, np_jrb_t* properties) {
	jrb_free_tree(msg->properties);
	msg->properties = properties;
};
inline void np_message_setinstruction(np_message_t* msg, np_jrb_t* instructions) {
	jrb_free_tree(msg->instructions);
	msg->instructions = instructions;
};
inline void np_message_setbody(np_message_t* msg, np_jrb_t* body) {
	jrb_free_tree(msg->body);
	msg->body = body;
};
inline void np_message_setfooter(np_message_t* msg, np_jrb_t* footer) {
	jrb_free_tree(msg->footer); msg->footer = footer;
};

//		if (-1 == np_message_decrypt_part(newmsg->instructions,
//										  enc_nonce->val.value.bin,
//										  session_token->session_key, NULL))
//		{
//			log_msg(LOG_ERROR,
//				"incorrect decryption of message instructions (send from %s:%d)",
//				ipstr, port);
//			job_submit_event(state->jobq, np_network_read);
//			return;
//		}

int np_message_decrypt_part(np_jrb_t* msg_part,
							unsigned char* enc_nonce,
							unsigned char* public_key,
							unsigned char* secret_key)
{
	np_jrb_t* enc_msg_part = jrb_find_str(msg_part, "encrypted");
	if (!enc_msg_part) {
		log_msg(LOG_ERROR, "couldn't find encrypted header");
		return -1;
	}
	unsigned char dec_part[enc_msg_part->val.size - crypto_box_MACBYTES];

	int ret = crypto_secretbox_open_easy(
			dec_part,
			enc_msg_part->val.value.bin,
			enc_msg_part->val.size,
			enc_nonce,
			public_key);
//	int ret = crypto_box_open_easy(
//			dec_part,
//			enc_msg_part->val.value.bin,
//			enc_msg_part->val.size,
//			enc_nonce,
//			public_key,
//			secret_key);
//	int ret = crypto_box_open_easy_afternm(
//			dec_part,
//			enc_msg_part->val.value.bin,
//			enc_msg_part->val.size,
//			enc_nonce,
//			public_key);
	if (ret < 0) {
		log_msg(LOG_ERROR, "couldn't decrypt header with session key %s", public_key);
		return -1;
	}

	cmp_ctx_t cmp;
	cmp_init(&cmp, enc_msg_part->val.value.bin, buffer_reader, buffer_writer);
	unsigned int map_size = 0;
	if (!cmp_read_map(&cmp, &map_size))
		return -1;
	deserialize_jrb_node_t(msg_part, &cmp, map_size/2);

	jrb_delete_node(enc_msg_part);

	return 1;
}

//		if (-1 == np_message_encrypt_part(args->msg->header,
//										  nonce,
//										  target_token->session_key,
//										  NULL))
//		{
//			log_msg(LOG_WARN,
//				"incorrect encryption of message header (not sending to %s:%d)",
//				target_node->dns_name, target_node->port);
//			return;
//		}
//
int np_message_encrypt_part(np_jrb_t* msg_part,
							unsigned char* nonce,
							unsigned char* public_key,
							unsigned char* secret_key)
{
	cmp_ctx_t cmp;
    unsigned char msg_part_buffer[NP_MESSAGE_SIZE];
    void* msg_part_buf_ptr = msg_part_buffer;

    cmp_init(&cmp, msg_part_buf_ptr, buffer_reader, buffer_writer);
	np_jrb_t* iter_node;

	if (!cmp_write_map(&cmp, msg_part->size*2 )) log_msg(LOG_WARN, cmp_strerror(&cmp));
	jrb_traverse(iter_node, msg_part) {
		serialize_jrb_node_t(iter_node, &cmp);
	}
	int msg_part_len = cmp.buf-msg_part_buf_ptr;

	int enc_msg_part_len = msg_part_len + crypto_box_MACBYTES;

	unsigned char* enc_msg_part = (unsigned char*) malloc(enc_msg_part_len);
	int ret = crypto_secretbox_easy(enc_msg_part,
									msg_part_buf_ptr,
									msg_part_len,
									nonce,
									public_key);
//	int ret = crypto_box_easy(enc_msg_part,
//							  msg_part_buf_ptr,
//							  msg_part_len,
//							  nonce,
//							  public_key,
//							  secret_key);
//	int ret = crypto_box_easy_afternm(enc_msg_part,
//								msg_part_buf_ptr,
//								msg_part_len,
//								nonce,
//								public_key);
	if (ret < 0) {
		return -1;
	}
	log_msg(LOG_ERROR, "encrypted header with session key %s", public_key);

	jrb_replace_all_with_str(msg_part, "encrypted", new_jval_bin(enc_msg_part, enc_msg_part_len));
	return 0;
}

/** 
 ** message_free:
 ** free the message and the payload
 */
void np_message_free(np_message_t * msg) {
	jrb_free_tree(msg->header);
	jrb_free_tree(msg->instructions);
	jrb_free_tree(msg->properties);
	jrb_free_tree(msg->body);
	jrb_free_tree(msg->footer);
	free (msg);
}

/**
 ** message_init: chstate, port
 ** Initialize messaging subsystem on port and returns the MessageGlobal * which 
 ** contains global state of message subsystem.
 ** message_init also initiate the network subsystem
 **/
np_messageglobal_t* message_init(int port) {

	int i = 0;
	np_messageglobal_t* mg = (np_messageglobal_t *) malloc(sizeof(np_messageglobal_t));

	mg->in_handlers = make_jrb();
	mg->out_handlers = make_jrb();
	mg->trans_handlers = make_jrb();

	if (pthread_mutex_init(&mg->input_lock, NULL ) != 0) {
		log_msg(LOG_ERROR, "pthread_mutex_init: %s", strerror (errno));
		return (NULL );
	}
	if (pthread_mutex_init(&mg->output_lock, NULL ) != 0) {
		log_msg(LOG_ERROR, "pthread_mutex_init: %s", strerror (errno));
		return (NULL );
	}
	if (pthread_mutex_init(&mg->trans_lock, NULL ) != 0) {
		log_msg(LOG_ERROR, "pthread_mutex_init: %s", strerror (errno));
		return (NULL);
	}
	if (pthread_mutex_init(&mg->interest_lock, NULL ) != 0) {
		log_msg(LOG_ERROR, "pthread_mutex_init: %s", strerror (errno));
		return (NULL);
	}

	/* NEUROPIL_INTERN_MSG_INTEREST HANDLING */
    mg->interest_sources = make_jrb();
    mg->interest_targets = make_jrb();

	/* NEUROPIL_INTERN_MESSAGES */
	for (i = 0; i < NR_OF_ELEMS(np_internal_messages); ++i) {
		if (strlen(np_internal_messages[i].msg_subject) > 0) {
			log_msg(LOG_DEBUG, "register handler (%d): %s", i, np_internal_messages[i].msg_subject);
			np_message_register_handler(mg, &np_internal_messages[i]);
		}
	}

	return mg;
}

np_callback_t np_message_get_callback (np_msgproperty_t *handler)
{
	assert (handler != NULL);
	assert (handler->clb != NULL);

	return handler->clb;
}

/**
 ** registers the handler function #func# with the message type #type#,
 ** it also defines the acknowledgment requirement for this type 
 **/
np_msgproperty_t* np_message_get_handler(np_messageglobal_t *mg, int msg_mode, const char* subject) {

	if (mg == NULL) return NULL;
	if (subject == NULL) return NULL;


	np_msgproperty_t* retVal = NULL;

	switch (msg_mode) {

	case INBOUND: // incoming message handler are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->input_lock);
		np_jrb_t* jrb_in_node = jrb_find_str(mg->in_handlers, subject);
		/* don't allow duplicates */
		if (jrb_in_node == NULL ) {
			log_msg(LOG_DEBUG, "no inbound message handler found for %s, now looking up default handler", subject);
			jrb_in_node = jrb_find_str(mg->in_handlers, DEFAULT);
		}
		retVal = jrb_in_node->val.value.v;
		pthread_mutex_unlock(&mg->input_lock);
		break;

	case OUTBOUND: // outgoing message handlers are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->output_lock);
		np_jrb_t* jrb_out_node = jrb_find_str(mg->out_handlers, subject);
		/* don't allow duplicates */
		if (jrb_out_node == NULL ) {
			log_msg(LOG_DEBUG, "no outbound message handler found for %s, now looking up default handler", subject);
			jrb_out_node = jrb_find_str(mg->out_handlers, DEFAULT);
		}
		retVal = jrb_out_node->val.value.v;
		pthread_mutex_unlock(&mg->output_lock);
		break;

	case TRANSFORM:
		// outgoing message handlers are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->trans_lock);
		np_jrb_t* jrb_trans_node = jrb_find_str(mg->trans_handlers, subject);
		/* don't allow duplicates */
		if (jrb_trans_node == NULL ) {
			log_msg(LOG_DEBUG, "no transform message handler found for %s, now looking up default handler", subject);
			jrb_trans_node = jrb_find_str(mg->trans_handlers, DEFAULT);
		}
		retVal = jrb_trans_node->val.value.v;
		pthread_mutex_unlock(&mg->trans_lock);
		break;

	default:
		log_msg(LOG_DEBUG, "message mode not specified for %s, using default",
				subject);
	}
	return retVal;
}


int np_message_check_handler(np_messageglobal_t *mg, int msg_mode, const char* subject) {

	int retVal = 0;

	switch (msg_mode) {

	case INBOUND: // incoming message handler are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->input_lock);
		np_jrb_t* jrb_in_node = jrb_find_str(mg->in_handlers, subject);
		/* don't allow duplicates */
		if (jrb_in_node != NULL ) {
			retVal = 1;
		}
		pthread_mutex_unlock(&mg->input_lock);
		break;

	case OUTBOUND: // outgoing message handlers are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->output_lock);
		np_jrb_t* jrb_out_node = jrb_find_str(mg->out_handlers, subject);
		/* don't allow duplicates */
		if (jrb_out_node != NULL ) {
			retVal = 1;
		}
		pthread_mutex_unlock(&mg->output_lock);
		break;

	case TRANSFORM:
		// outgoing message handlers are required
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->trans_lock);
		np_jrb_t* jrb_trans_node = jrb_find_str(mg->trans_handlers, subject);
		/* don't allow duplicates */
		if (jrb_trans_node != NULL ) {
			retVal = 1;
		}
		pthread_mutex_unlock(&mg->trans_lock);
		break;

	default:
		log_msg(LOG_DEBUG, "message mode not specified for %s, default handling would be used", subject);
		break;
	}
	return retVal;
}


void np_message_register_handler(np_messageglobal_t *mg, np_msgproperty_t* msgprops) {

	switch (msgprops->msg_mode) {

	case INBOUND:
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->input_lock);

		np_jrb_t* jrb_in_node = jrb_find_str(mg->in_handlers, msgprops->msg_subject);
		/* don't allow duplicates */
		if (jrb_in_node == NULL ) {
			jrb_insert_str(mg->in_handlers, msgprops->msg_subject,
					new_jval_v(msgprops));
			log_msg(LOG_DEBUG, "inbound message handler registered for %s",
					msgprops->msg_subject);
			// jrb_node = jrb_find_str (mg->handlers, "");
		}
		pthread_mutex_unlock(&mg->input_lock);
		break;

	case OUTBOUND:
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->output_lock);
		np_jrb_t* jrb_out_node = jrb_find_str(mg->out_handlers, msgprops->msg_subject);
		/* don't allow duplicates */
		if (jrb_out_node == NULL ) {
			jrb_insert_str(mg->out_handlers, msgprops->msg_subject,
					new_jval_v(msgprops));
			log_msg(LOG_DEBUG, "outbound message handler registered for %s",
					msgprops->msg_subject);
			// jrb_node = jrb_find_str (mg->handlers, "");
		}
		pthread_mutex_unlock(&mg->output_lock);
		break;

	case TRANSFORM:
		/* add message handler function into the set of all handlers */
		pthread_mutex_lock(&mg->trans_lock);
		np_jrb_t* jrb_trans_node = jrb_find_str(mg->trans_handlers, msgprops->msg_subject);
		/* don't allow duplicates */
		if (jrb_trans_node == NULL ) {
			jrb_insert_str(mg->trans_handlers, msgprops->msg_subject,
					new_jval_v(msgprops));
			log_msg(LOG_DEBUG, "transform message handler registered for %s",
					msgprops->msg_subject);
		}
		pthread_mutex_unlock(&mg->trans_lock);
		break;

	default:
		log_msg(LOG_ERROR, "message mode not specified for %s, using default",
				msgprops->msg_subject);
	}
}

// np_msgproperty_t*
void np_message_create_property(np_messageglobal_t *mg, const char* subject, int msg_mode, int msg_type, int ack_mode, int priority, int retry, np_callback_t callback) {

	// log_msg(LOG_INFO, "message create property");
	np_msgproperty_t* prop = (np_msgproperty_t*) malloc(sizeof(np_msgproperty_t));

	prop->msg_subject = strndup(subject, 255);
	prop->msg_mode = msg_mode;
	prop->msg_type = msg_type;
	prop->priority = priority;
	prop->ack_mode = ack_mode;
	prop->retry = retry;
	prop->clb = callback;

	np_message_register_handler(mg, prop);
	return;
}

np_msginterest_t* np_message_create_interest(const np_state_t* state, const char* subject, int msg_type, unsigned long seqnum, int threshold) {

	np_msginterest_t* tmp = (np_msginterest_t*) malloc(sizeof(np_msginterest_t));
	tmp->msg_subject = strndup(subject, 255);
	tmp->key = state->neuropil->me->key;
	tmp->msg_type = msg_type;
	tmp->msg_seqnum = seqnum;
	tmp->msg_threshold = threshold;

	tmp->send_ack = 1;

	pthread_mutex_init (&tmp->lock, NULL);
    pthread_cond_init (&tmp->msg_received, &tmp->cond_attr);
    pthread_condattr_setpshared(&tmp->cond_attr, PTHREAD_PROCESS_PRIVATE);

    return tmp;
}

// update internal structure and return a interest if a matching pair has been found
np_msginterest_t* np_message_interest_update(np_messageglobal_t *mg, np_msginterest_t *interest) {

	np_msginterest_t* available = NULL;

	pthread_mutex_lock(&mg->interest_lock);

	// look up sources to see whether a sender already exists
	np_jrb_t* tmp_source = jrb_find_str(mg->interest_sources, interest->msg_subject);
	if (tmp_source != NULL) {
		np_msginterest_t* tmp = tmp_source->val.value.v;
		if (interest->msg_seqnum == 0) available = tmp;
		if ((tmp->msg_seqnum - tmp->msg_threshold) <= interest->msg_seqnum) available = tmp;
	} else {
		log_msg(LOG_DEBUG, "lookup of message source failed");
	}

	// look up target or create it
	np_jrb_t* tmp_target = jrb_find_str(mg->interest_targets, interest->msg_subject);
	if (tmp_target != NULL) {
		// update
		log_msg(LOG_DEBUG, "lookup of message target successful");
		np_msginterest_t* tmp = tmp_target->val.value.v;
		tmp->msg_type = interest->msg_type;
		tmp->msg_seqnum = interest->msg_seqnum;
		tmp->msg_threshold = interest->msg_threshold;

	} else {
		// create
		log_msg(LOG_DEBUG, "adding a new message target");
		jrb_insert_str(mg->interest_targets, interest->msg_subject, new_jval_v(interest));
	}
	pthread_mutex_unlock(&mg->interest_lock);

	return available;
}

np_msginterest_t* np_message_available_update(np_messageglobal_t *mg, np_msginterest_t *available) {

	np_msginterest_t* interest = NULL;

	pthread_mutex_lock(&mg->interest_lock);

	// look up targets to see whether a receiver already exists
	np_jrb_t* tmp_source = jrb_find_str(mg->interest_targets, available->msg_subject);
	if (tmp_source != NULL) {
		log_msg(LOG_DEBUG, "lookup of message target successful");
		np_msginterest_t* tmp = tmp_source->val.value.v;
		if (tmp->msg_seqnum == 0) interest = tmp;
		if ((tmp->msg_seqnum - tmp->msg_threshold) <= available->msg_seqnum) interest = tmp;
	}
	// else {
	// 	log_msg(LOG_DEBUG, "lookup of message target failed");
	// }

	// look up sources or create it
	np_jrb_t* tmp_target = jrb_find_str(mg->interest_sources, available->msg_subject);
	if (tmp_target != NULL) {
		log_msg(LOG_DEBUG, "lookup of message source successful");
		// update
		np_msginterest_t* tmp = tmp_target->val.value.v;
		tmp->msg_type = available->msg_type;
		tmp->msg_seqnum = available->msg_seqnum;
		tmp->msg_threshold = available->msg_threshold;
	} else {
		log_msg(LOG_DEBUG, "adding a new message source");
		// create
		jrb_insert_str(mg->interest_sources, available->msg_subject, new_jval_v(available));
	}
	pthread_mutex_unlock(&mg->interest_lock);

	return interest;
}

// check whether an interest is existing
np_msginterest_t* np_message_interest_match(np_messageglobal_t *mg, const char *subject) {

	// look up sources to see whether a sender already exists
	pthread_mutex_lock(&mg->interest_lock);
	np_jrb_t* tmp_source = jrb_find_str(mg->interest_targets, subject);
	pthread_mutex_unlock(&mg->interest_lock);

	if (tmp_source)
		return (np_msginterest_t*) tmp_source->val.value.v;
	else
		return NULL;
}

// check whether an interest is existing
np_msginterest_t* np_message_available_match(np_messageglobal_t *mg, const char *subject) {
	// look up sources to see whether a sender already exists
	pthread_mutex_lock(&mg->interest_lock);
	np_jrb_t* tmp_target = jrb_find_str(mg->interest_sources, subject);
	pthread_mutex_unlock(&mg->interest_lock);

	if (tmp_target)
		return (np_msginterest_t*) tmp_target->val.value.v;
	else
		return NULL;
}

np_msginterest_t* np_decode_msg_interest(np_messageglobal_t *mg, np_jrb_t* data ) {

	np_msginterest_t* interest = (np_msginterest_t*) malloc(sizeof(np_msginterest_t));
	interest->key = (np_key_t*) malloc(sizeof(np_key_t));

	np_jrb_t* key = jrb_find_str(data, "mi.key");
	str_to_key(interest->key, key->val.value.s);

	np_jrb_t* msg_subject = jrb_find_str(data, "mi.msg_subject");
	interest->msg_subject = strndup(msg_subject->val.value.s, 255);

	interest->msg_type = jrb_find_str(data, "mi.msg_type")->val.value.i;

	interest->msg_threshold = jrb_find_str(data, "mi.msg_threshold")->val.value.i;

	interest->msg_seqnum = jrb_find_str(data, "mi.msg_seqnum")->val.value.ul;


//	assert(pn_data_type(amqp_data) == PN_LIST);
//	int count = pn_data_get_list(amqp_data);
//	assert(count == 5);
//	pn_data_enter(amqp_data);
//
//	pn_data_next(amqp_data);
//    assert(pn_data_type(amqp_data) == PN_STRING);
//	pn_bytes_t bnp_key_t* 
//	char sHostkey[bnp_key_t* 
//	strncpy(sHostkey, bnp_key_t* 
//	// sHostkey[bnp_key_t* 
//	str_to_key(interest->key, sHostkey);
//
//	pn_data_next(amqp_data);
//    assert(pn_data_type(amqp_data) == PN_STRING);
//	pn_bytes_t bSubject = pn_data_get_string(amqp_data);
//	interest->msg_subject = (char*) malloc(bSubject.size);
//	strncpy(interest->msg_subject, bSubject.start, bSubject.size);
//	// interest->msg_subject[bSubject.size-1] = '\0';
//
//	pn_data_next(amqp_data);
//    assert(pn_data_type(amqp_data) == PN_INT);
//    interest->msg_type = pn_data_get_int(amqp_data);
//
//    pn_data_next(amqp_data);
//    assert(pn_data_type(amqp_data) == PN_ULONG);
//    interest->msg_seqnum = pn_data_get_ulong(amqp_data);
//
//    pn_data_next(amqp_data);
//    assert(pn_data_type(amqp_data) == PN_INT);
//    interest->msg_threshold = pn_data_get_int(amqp_data);
//
//	pn_data_exit(amqp_data);
//
//	if (pn_data_errno(amqp_data) < 0) {
//		log_msg(LOG_ERROR, "error decoding msg_interest from amqp data structure");
//	}

	return interest;
}

void np_message_encode_interest(np_jrb_t* data, np_msginterest_t *interest) {

	char* keystring = (char*) key_get_as_string (interest->key);

	jrb_insert_str(data, "mi.key", new_jval_s(keystring));
	jrb_insert_str(data, "mi.msg_subject", new_jval_s(interest->msg_subject));
	jrb_insert_str(data, "mi.msg_type", new_jval_i(interest->msg_type));
	jrb_insert_str(data, "mi.msg_seqnum", new_jval_ul(interest->msg_seqnum));
	jrb_insert_str(data, "mi.msg_threshold", new_jval_i(interest->msg_threshold));

//	pn_data_put_list(amqp_data);
//	pn_data_enter(amqp_data);
//	pn_data_put_string(amqp_data, pn_bytes(strlen(keystring), keystring));
//	pn_data_put_string(amqp_data, pn_bytes(strlen(interest->msg_subject), interest->msg_subject));
//	pn_data_put_int(amqp_data, interest->msg_type);
//	pn_data_put_ulong(amqp_data, interest->msg_seqnum);
//	pn_data_put_int(amqp_data, interest->msg_threshold);
//	pn_data_exit(amqp_data);
//	if (pn_data_errno(amqp_data) < 0) {
//		log_msg(LOG_ERROR, "error encoding msg_interest as amqp data structure");
//	}
}

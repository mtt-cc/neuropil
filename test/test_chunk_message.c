
#include <uuid/uuid.h>
#include <assert.h>
#include <stdlib.h>

#include "pthread.h"

#include "np_memory.h"
#include "np_message.h"
#include "np_jtree.h"
#include "np_key.h"
#include "jval.h"
#include "cmp.h"
#include "dtime.h"
#include "log.h"
#include "np_util.h"

#include "include.h"


int main(int argc, char **argv) {

	char log_file[256];
	sprintf(log_file, "%s_%s.log", "./test_chunk_message", "1000");
	// int level = LOG_ERROR | LOG_WARN | LOG_INFO | LOG_DEBUG | LOG_TRACE | LOG_ROUTING | LOG_NETWORKDEBUG | LOG_KEYDEBUG;
	// int level = LOG_ERROR | LOG_WARN | LOG_INFO | LOG_DEBUG | LOG_NETWORKDEBUG | LOG_KEYDEBUG;
	int level = LOG_ERROR | LOG_WARN | LOG_INFO | LOG_DEBUG;
	log_init(log_file, level);

	np_message_t* msg_out = NULL;
	np_new_obj(np_message_t, msg_out);
	char* msg_subject = "this.is.a.test";
	np_key_t* my_key = key_create_from_hostport("me", "two");

	uint16_t parts = 0;
	jrb_insert_str(msg_out->header, NP_MSG_HEADER_SUBJECT,  new_jval_s((char*) msg_subject));
	jrb_insert_str(msg_out->header, NP_MSG_HEADER_TO,  new_jval_s((char*) key_get_as_string(my_key)) );
	jrb_insert_str(msg_out->header, NP_MSG_HEADER_FROM, new_jval_s((char*) key_get_as_string(my_key)) );
	jrb_insert_str(msg_out->header, NP_MSG_HEADER_REPLY_TO, new_jval_s((char*) key_get_as_string(my_key)) );

	jrb_insert_str(msg_out->instructions, NP_MSG_INST_ACK, new_jval_ush(0));
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_ACK_TO, new_jval_s((char*) key_get_as_string(my_key)) );
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_SEQ, new_jval_ul(0));
	char* new_uuid = np_create_uuid(msg_subject, 1);
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_UUID, new_jval_s(new_uuid));
	free(new_uuid);

	double now = dtime();
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_TSTAMP, new_jval_d(now));
	now += 20;
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_TTL, new_jval_d(now));

	jrb_insert_str(msg_out->instructions, NP_MSG_INST_SEND_COUNTER, new_jval_ush(0));

	// TODO: message part split-up informations
	jrb_insert_str(msg_out->instructions, NP_MSG_INST_PARTS, new_jval_iarray(parts, parts));

	char prop_payload[30]; //  = (char*) malloc(25 * sizeof(char));
	memset (prop_payload, 'a', 29);
	prop_payload[29] = '\0';
	for (int16_t i = 0; i < 9; i++) {
		jrb_insert_int(msg_out->properties, i, new_jval_s(prop_payload));
	}
	char body_payload[51]; //  = (char*) malloc(50 * sizeof(char));
	memset (body_payload, 'b', 50);
	body_payload[50] = '\0';
	for (int16_t i = 0; i < 60; i++) {
		jrb_insert_int(msg_out->body, i, new_jval_s(body_payload));
	}

	uint16_t fixed_size = 1 + 40 + msg_out->header->byte_size + msg_out->instructions->byte_size + 15;
	uint16_t payload_size =
			msg_out->properties->byte_size +
			msg_out->body->byte_size +
			msg_out->footer->byte_size;

	//	n*1024 = n*f + (n*15) + (p-15) + x;
	//	n*1024 > n*f + (n*15) + (p-15);

	// 1024-f = pp;

	//	x = n*1024 - n*f - n*15 - (p-15);
	//	x = n*(1024 - f - 15) - (p-15);
	//	n*1024 = n*f + (p-15) + n*15;
	//	n*1024 - n*f - n*15 = p-15;
	//	n*(1024 - f - 15) = p-15;
	//	n = (p-15) / (1024 - f - 15);

	uint16_t chunks = ((int) (payload_size) / (1024 - fixed_size)) + 1;
	// uint16_t garbage_size_1 =  (payload_size-15) % (1024 - fixed_size - 15);
	// uint16_t chunks = ((int) ((payload_size - 15) / (1024 - fixed_size - 15))) + 1;
	uint16_t garbage_size_2 = (chunks*1024 - chunks*fixed_size) - payload_size - 10;

	if (garbage_size_2 < (strlen(NP_MSG_FOOTER_GARBAGE))) {
		log_msg(LOG_DEBUG, "recalculating garbage size");
		chunks++;
		garbage_size_2 = (chunks*1024 - chunks*fixed_size) - payload_size - 10;
	}

	// chunk_size = payload_size / chunks;
	// uint16_t garbage_size_1 = payload_size % 1024;
	// uint16_t garbage_size_2 = (chunks * 1024) - (fixed_size + payload_size);

	log_msg(LOG_DEBUG, "msg_size %llu chunks %d, garbage %d fixed %d payload_size %d",
			1 + 40 + msg_out->header->byte_size + msg_out->instructions->byte_size + msg_out->properties->byte_size + msg_out->body->byte_size + msg_out->footer->byte_size,
			chunks, garbage_size_2, fixed_size, payload_size);

	uint16_t footer_size = garbage_size_2 - strlen(NP_MSG_FOOTER_GARBAGE);
	char foot_payload[footer_size]; //  = malloc(footer_size);
	memset(foot_payload, 'c', footer_size-1);
	foot_payload[footer_size-1] = '\0';
	jrb_insert_str(msg_out->footer, NP_MSG_FOOTER_GARBAGE, new_jval_bin(foot_payload, footer_size));

	log_msg(LOG_DEBUG, "msg_size %llu chunks %d, garbage %d fixed %d payload_size %d",
			1 + 40 + msg_out->header->byte_size + msg_out->instructions->byte_size + msg_out->properties->byte_size + msg_out->body->byte_size + msg_out->footer->byte_size,
			chunks, msg_out->footer->byte_size, fixed_size, payload_size);

	/** message split up maths
	 ** message size = 1b (common header) + 40b (encryption) +
	 **                msg (header + instructions) + msg (properties + body) + msg (footer)
	 ** if (size > 1024)
	 **     fixed_size = 1b + 40b + msg (header + instructions)
	 **     payload_size = msg (properties) + msg(body) + msg(footer)
	 **     #_of_chunks = int(payload_size / (1024 - fixed_size)) + 1
	 **     chunk_size = payload_size / #_of_chunks
	 **     garbage_size = #_qof_chunks * (fixed_size + chunk_size) % 1024 // spezial behandlung garbage_size < 3
	 **     add garbage
	 ** else
	 ** 	add garbage
	 **/
	// char* buffer[1024];
	void** target_buffer = malloc(chunks * sizeof(void*));

	np_message_serialize_chunked(msg_out, target_buffer, chunks);

	np_message_t* msg_in = NULL;
	np_new_obj(np_message_t, msg_in);
	np_message_deserialize_chunked(msg_in, target_buffer, chunks);

	np_jtree_elem_t* properties_node = jrb_find_int(msg_out->properties, 1);
	np_jtree_elem_t* body_node = jrb_find_int(msg_out->body, 20);
	np_jtree_elem_t* footer_node = jrb_find_str(msg_out->footer, NP_MSG_FOOTER_GARBAGE);
	log_msg(LOG_DEBUG, "properties %s, body %s, garbage size %hd",
			properties_node->val.value.s,
			body_node->val.value.s,
			jrb_get_byte_size(footer_node));
}

/*
 * neuropil_demo_service.c
 *
 * This service is available via http://demo.neuropil.io
 *
 * It is composed out of the examples for
 *  - pingpong
 *  - echo server
 * */
//
// neuropil is copyright 2016-2017 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
/**
 *.. NOTE::
 *
 *   If you are not yet familiar with the neuropil initialization procedure please refer to the :ref:`tutorial`
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "event/ev.h"

#include "np_log.h"
#include "np_types.h"
#include "np_list.h"
#include "np_util.h"
#include "np_http.h"
#include "np_memory.h"
#include "np_message.h"
#include "np_msgproperty.h"
#include "np_keycache.h"
#include "np_tree.h"
#include "np_route.h"
#include "np_key.h"
#include "np_sysinfo.h"


#include "neuropil.h"
#include "example_helper.c"

NP_SLL_GENERATE_PROTOTYPES(int);
NP_SLL_GENERATE_IMPLEMENTATION(int);

uint32_t _ping_count = 0;
uint32_t _pong_count = 0;

np_bool receive_echo_message(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body);
np_bool receive_pong(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body);
np_bool receive_ping(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body);

int main(int argc, char **argv) {
	int no_threads = 8;
	char *j_key = NULL;
	char* proto = "udp4";
	char* port = NULL;
	char* publish_domain = NULL;
	int level = -2;
	char* logpath = ".";

	int opt;
	if (parse_program_args(
		__FILE__,
		argc,
		argv,
		&no_threads,
		&j_key,
		&proto,
		&port,
		&publish_domain,
		&level,
		&logpath,
		"",
		""
	) == FALSE) {
		exit(EXIT_FAILURE);
	}

	char log_file_host[256];
	sprintf(log_file_host, "%s%s_%s.log", logpath, "/neuropil_demo_service",port);

	np_log_init(log_file_host, level);
	np_init(proto, port, publish_domain);

 
	np_msgproperty_t* echo_props = NULL;
	np_add_receive_listener(receive_echo_message, "echo");
	echo_props = np_msgproperty_get(INBOUND, "echo");
	echo_props->ack_mode = ACK_NONE;
	echo_props->msg_ttl = 20.0;

	np_msgproperty_t* ping_props = NULL;
	np_add_receive_listener(receive_ping, "ping");
	ping_props = np_msgproperty_get(INBOUND, "ping");
	ping_props->ack_mode = ACK_NONE;
	ping_props->msg_ttl = 20.0;
	np_msgproperty_register(ping_props);

	np_msgproperty_t* pong_props = NULL;
	np_add_receive_listener(receive_ping, "pong");
	pong_props = np_msgproperty_get(INBOUND, "pong");
	pong_props->ack_mode = ACK_NONE;
	pong_props->msg_ttl = 20.0;

	np_start_job_queue(no_threads);

	double lastping = np_time_now();
	np_send_text("ping", "ping", _ping_count++, NULL);
	uint32_t last_count_of_routes = 0;
	uint32_t count_of_routes = 0;

	while (TRUE) {
		__np_example_helper_loop();
		np_time_sleep(0.1);

		double now = np_time_now();
			// invoke a ping message every 10 seconds
			if ((now - lastping) > 10.0)
		{
			lastping = np_time_now();
			np_send_text("ping", "ping", _ping_count++, NULL);
		}
	}
}

np_bool receive_echo_message(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body) {
	np_tree_t* header = msg->header;

	char* reply_to = NULL; // All
	np_tree_elem_t* repl_to = np_tree_find_str(header, _NP_MSG_HEADER_FROM);
	if (NULL != repl_to) {
		reply_to = np_treeval_to_str(repl_to->val, NULL);
		char* text;
		np_tree_elem_t* txt = np_tree_find_str(body, NP_MSG_BODY_TEXT);
		if (NULL != txt) {
			text = np_treeval_to_str(txt->val, NULL);

		} else {
			text = "<NON TEXT MSG>";
		}
		np_send_text("echo", text, 0, reply_to);
	}
	return TRUE;
}

np_bool receive_ping(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body)
{
	char* text = np_treeval_to_str(np_tree_find_str(body, NP_MSG_BODY_TEXT)->val, NULL);
	uint32_t seq = np_tree_find_str(properties, _NP_MSG_INST_SEQ)->val.value.ul;

	log_msg(LOG_INFO, "RECEIVED: %d -> %s", seq, text);
	log_msg(LOG_INFO, "SENDING: %d -> %s", _pong_count++, "pong");
	np_send_text("pong", "pong", _pong_count,NULL);

	return TRUE;
}

np_bool receive_pong(const np_message_t* const msg, np_tree_t* properties, np_tree_t* body)
{
	char* text = np_treeval_to_str(np_tree_find_str(body, NP_MSG_BODY_TEXT)->val, NULL);
	uint32_t seq = np_tree_find_str(properties, _NP_MSG_INST_SEQ)->val.value.ul;

	log_msg(LOG_INFO, "RECEIVED: %d -> %s", seq, text);
	log_msg(LOG_INFO, "SENDING: %d -> %s", _ping_count++, "ping");
	np_send_text("ping", "ping", _ping_count,NULL);

	return TRUE;
}
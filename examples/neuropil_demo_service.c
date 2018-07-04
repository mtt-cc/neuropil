//
// neuropil is copyright 2016-2018 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
/*
 * neuropil_demo_service.c
 *
 * This service is also available via *:udp4:demo.neuropil.io:31415
 *
 * It is composed out of the examples for
 *  - pingpong
 *  - echo server
 */

/**
 *.. NOTE::
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

bool receive_echo_message(np_context* context, np_message* message);
bool receive_pong(np_context* ac, np_message* message);
bool receive_ping(np_context* ac, np_message* message);

int main(int argc, char **argv) {
	int no_threads = 8;
	char *j_key = NULL;
	char* proto = "udp4";
	char* port = NULL;
	char* publish_domain = NULL;
	int level = -2;
	char* logpath = ".";

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
	) == false) {
		exit(EXIT_FAILURE);
	}

	struct np_settings *settings = np_new_settings(NULL);
	settings->n_threads = no_threads;

	sprintf(settings->log_file, "%s/%s_%s.log", logpath, "neuropil_demo_service", port);
	fprintf(stdout, "logpath: %s\n", settings->log_file);
	settings->log_level = level;

	np_context * context = np_new_context(settings);

	if (np_ok != np_listen(context, proto, publish_domain, atoi(port))) {
		np_example_print(context, stderr, "ERROR: Node could not listen");
		exit(EXIT_FAILURE);
	}
 
	np_msgproperty_t* echo_props = NULL;
	
	np_add_receive_cb(context, "echo", receive_echo_message);	
	np_set_mx_properties(context, "echo", { .ackmode = NP_MX_ACK_NONE });

	echo_props->msg_ttl = 20.0;

	np_msgproperty_t* ping_props = NULL;
	np_add_receive_cb(context, "ping", receive_ping);
	ping_props = np_msgproperty_get(context, INBOUND, "ping");
	ping_props->ack_mode = ACK_NONE;
	ping_props->msg_ttl = 20.0;
	np_msgproperty_register(ping_props);

	np_msgproperty_t* pong_props = NULL;
	np_add_receive_cb(context, "pong", receive_pong);
	pong_props = np_msgproperty_get(context, INBOUND, "pong");
	pong_props->ack_mode = ACK_NONE;
	pong_props->msg_ttl = 20.0;

	if (np_ok != np_run(context, 0)) {
		np_example_print(context, stderr, "ERROR: Node could not start");
		exit(EXIT_FAILURE);
	}

	while (true) {
		__np_example_helper_loop(context);		
	}
}

bool receive_echo_message(np_context* context, np_message* message) {
	np_example_print(context, stdout, "Echoing msg %s", message->uuid);
	np_send_to(context, "echo", message->data, message->data_length, &message->from);
	return true;
}

bool receive_ping(np_context* context, np_message* message)
{
	char tmp[255];
	_np_dhkey_to_str(&message->from, tmp);
	np_example_print(context, stdout, "Received ping from %s", tmp);
	np_send_text(context, "pong", "pong", _pong_count, &message->from);

	return true;
}

bool receive_pong(np_context* context, np_message* message)
{
	char tmp[255];
	_np_dhkey_to_str(&message->from, tmp);
	np_example_print(context, stdout, "Received pong from %s", tmp);
	np_send_text(context, "ping", "ping", _ping_count, &message->from);

	return true;
}

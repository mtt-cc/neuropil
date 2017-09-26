//
// neuropil is copyright 2016 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
#ifndef _NP_DENDRIT_H_
#define _NP_DENDRIT_H_

#include "np_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// input message handlers
NP_API_INTERN
void _np_in_handshake(np_jobargs_t* args);

NP_API_INTERN
void _np_in_received (np_jobargs_t* args);

NP_API_INTERN
void _np_in_ping(np_jobargs_t* args);
NP_API_INTERN
void _np_in_pingreply(np_jobargs_t* args);

NP_API_INTERN
void _np_in_piggy (np_jobargs_t* args);
NP_API_INTERN
void _np_in_update (np_jobargs_t* args);

NP_API_INTERN
void _np_in_join_req(np_jobargs_t* args);
NP_API_INTERN
void _np_in_join_ack (np_jobargs_t* args);
NP_API_INTERN
void _np_in_join_nack (np_jobargs_t* args);
NP_API_INTERN
void _np_in_join_wildcard_req(np_jobargs_t* args);

NP_API_INTERN
void _np_in_leave_req(np_jobargs_t* args);

NP_API_INTERN
void _np_in_discover_receiver(np_jobargs_t* args);
NP_API_INTERN
void _np_in_discover_sender(np_jobargs_t* args);
NP_API_INTERN
void _np_in_available_sender(np_jobargs_t* args);
NP_API_INTERN
void _np_in_available_receiver(np_jobargs_t* args);

NP_API_INTERN
void _np_in_authenticate(np_jobargs_t* args);
NP_API_INTERN
void _np_in_authenticate_reply(np_jobargs_t* args);

NP_API_INTERN
void _np_in_authorize(np_jobargs_t* args);
NP_API_INTERN
void _np_in_authorize_reply(np_jobargs_t* args);

NP_API_INTERN
void _np_in_account(np_jobargs_t* args);

NP_API_INTERN
void _np_in_signal_np_receive (np_jobargs_t* args);
NP_API_INTERN
void _np_in_callback_wrapper(np_jobargs_t* args);
NP_API_INTERN
void _np_in_upgrade_aaatoken(np_key_t* key_to_upgrade, np_aaatoken_t* new_token);
#ifdef __cplusplus
}
#endif

#endif // _NP_HANDLER_H_

//
// SPDX-FileCopyrightText: 2016-2024 by pi-lar GmbH
// SPDX-License-Identifier: OSL-3.0
//
// original version is based on the chimera project
#ifndef _NP_NETWORK_H_
#define _NP_NETWORK_H_

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include "event/ev.h"
#include "netdb.h"
#include "sys/socket.h"

#include "util/np_list.h"

#include "np_constants.h"
#include "np_memory.h"
#include "np_settings.h"
#include "np_types.h"
#include "np_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 ** NETWORK_PACK_SIZE is the maximum packet size that will be handled by
 *neuropil network layer
 **
 */
#define NETWORK_PACK_SIZE 65536

/**
 ** TIMEOUT is the number of seconds to wait for receiving ack from the
 *destination, if you want
 ** the sender to wait forever put 0 for TIMEOUT.
 **
 */
#define TIMEOUT 1.0

typedef uint16_t socket_type;
// enum socket_type {
static const socket_type UNKNOWN_PROTO  = 0x001;
static const socket_type IPv4           = 0x002;
static const socket_type IPv6           = 0x004;
static const socket_type MASK_IP        = 0x00F;
static const socket_type UDP            = 0x010; // UDP protocol - default
static const socket_type TCP            = 0x020; // TCP protocol
static const socket_type MASK_PROTOCOLL = 0x0FF;
static const socket_type PASSIVE        = 0x100;
static const socket_type MASK_OPTION    = 0xF00;
// static const socket_type // RAW         = 0x040, // pure IP protocol - no
// ports } NP_ENUM;

typedef enum np_network_type_e {
  np_network_type_none   = 0x00,
  np_network_type_server = 0x01,
  np_network_type_client = 0x02,
} np_network_type_e;

struct np_network_s {
  bool              initialized;
  bool              is_multiuse_socket;
  int               socket;
  ev_io             watcher_in;
  ev_io             watcher_out;
  uint8_t           is_running;
  np_network_type_e type;

  socket_type socket_type;

  struct sockaddr *remote_addr;
  socklen_t        remote_addr_len;

  uint32_t seqend;
  uint16_t max_messages_per_second;
  double   last_send_date;
  double   last_received_date;
  np_sll_t(void_ptr, out_events);

  char ip[CHAR_LENGTH_IP];
  char port[CHAR_LENGTH_PORT];

  np_mutex_t access_lock;
  TSP(bool, can_be_enabled);

} NP_API_INTERN;

_NP_GENERATE_MEMORY_PROTOTYPES(np_network_t);

NP_API_INTERN
bool _np_network_module_init(np_state_t *context);
NP_API_INTERN
void _np_network_module_destroy(np_state_t *context);

// parse protocol string of the form "tcp4://..." and return the correct @see
// socket_type
NP_API_INTERN
socket_type _np_network_parse_protocol_string(const char *protocol_str);

NP_API_INTERN
char *_np_network_get_protocol_string(np_state_t *context,
                                      socket_type protocol);

/** network_address:
 ** returns the ip address of the #hostname#
 **
 **/
NP_API_INTERN
bool _np_network_get_address(np_state_t       *context,
                             bool              create_socket,
                             struct addrinfo **ai,
                             socket_type       type,
                             char             *hostname,
                             char             *service);
// struct addrinfo _np_network_get_address (char *hostname);

NP_API_INTERN
enum np_return _np_network_get_remote_ip(np_context       *context,
                                         const char       *hostname,
                                         const socket_type protocol,
                                         char             *remote_ip);

NP_API_INTERN
void _np_network_stop(np_network_t *ng, bool force);
NP_API_INTERN
void _np_network_start(np_network_t *ng, bool force);

/** _np_network_init:
 ** initiates the networking layer by creating socket and bind it to #port#
 **
 **/
NP_API_INTERN
bool _np_network_init(np_network_t *network,
                      bool          create_socket,
                      socket_type   type,
                      char         *hostname,
                      char         *service,
                      uint16_t      max_messages_per_second,
                      int           prepared_socket_fd,
                      socket_type   passive_socket_type);

NP_API_INTERN
enum np_return _np_network_get_local_ip(NP_UNUSED const np_network_t *ng,
                                        const char                   *hostname,
                                        const socket_type socket_type,
                                        char             *local_ip);

NP_API_INTERN
enum np_return
_np_network_get_outgoing_ip(NP_UNUSED const np_network_t *ng,
                            const char                   *hostname,
                            const socket_type             passive_socket_type,
                            char                         *local_ip);

// checks whether a given IP is a loopback address
NP_API_INTERN
bool _np_network_is_loopback_address(NP_UNUSED const np_network_t *ng,
                                     const char                   *ip);

// checks whether a given IP falls into the range of private IP's or not
NP_API_INTERN
bool _np_network_is_private_address(NP_UNUSED np_network_t *ng, const char *ip);

// compares two ip strings and return the number of overlapping ip tuples
NP_API_INTERN
uint8_t _np_network_count_common_tuples(NP_UNUSED const np_network_t *ng,
                                        const char                   *remote_ip,
                                        const char                   *local_ip);

// NP_API_INTERN
// bool _np_network_send_data(np_state_t   *context,
//                            np_network_t *network,
//                            void         *data_to_send);
/**
 ** _np_network_append_msg_to_out_queue:
 ** Sends a message to host
 **
 **/
NP_API_INTERN
void _np_network_write(struct ev_loop *loop, ev_io *event, int revents);
NP_API_INTERN
void _np_network_read(struct ev_loop *loop, ev_io *event, int revents);
NP_API_INTERN
void _np_network_accept(struct ev_loop *loop, ev_io *event, int revents);
NP_API_INTERN
void _np_network_disable(np_network_t *self);
NP_API_INTERN
void _np_network_enable(np_network_t *self);
NP_API_INTERN
void _np_network_set_key(np_network_t *self, np_dhkey_t key);
#ifdef __cplusplus
}
#endif

#endif /* _CHIMERA_NETWORK_H_ */

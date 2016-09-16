/**
The structure np_aaatoken_t is used for authorization, authentication and accounting purposes
the data structure is the same for all purposes. Add-on information can be stored in a nested jtree structure.
Several analogies have been used as a baseline for this structure: json web token, kerberos and diameter.
Tokens do get integrity protected by adding an additional signature based on the issuers public key

The structure is described here to allow user the proper use of the :c:func:`np_set_identity` function.

*/

// copyright 2016 pi-lar GmbH

#ifndef _NP_AAATOKEN_H_
#define _NP_AAATOKEN_H_

#include <pthread.h>
#include <uuid/uuid.h>

#include "sodium.h"

#include "np_key.h"
#include "np_list.h"
#include "np_memory.h"
#include "np_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// sodium defines several length of its internal key size, but they always are 32U long
// crypto_scalarmult_BYTES, crypto_scalarmult_curve25519_BYTES, crypto_sign_ed25519_PUBLICKEYBYTES
// crypto_box_PUBLICKEYBYTES, crypto_box_SECRETKEYBYTES

typedef enum np_aaastate_e aaastate_type;

enum np_aaastate_e
{
	AAA_UNKNOWN       = 0x00,
	AAA_VALID         = 0x01,
	AAA_AUTHENTICATED = 0x02,
	AAA_AUTHORIZED    = 0x04,
	AAA_ACCOUNTING    = 0x08
} NP_ENUM NP_API_EXPORT;

#define AAA_INVALID (~AAA_VALID)

#define IS_VALID(x) (0 < (x & AAA_VALID))
#define IS_INVALID(x) (!IS_VALID(x))

#define IS_AUTHENTICATED(x) (0 < (AAA_AUTHENTICATED & x))
#define IS_NOT_AUTHENTICATED(x) (!IS_AUTHENTICATED(x))

#define IS_AUTHORIZED(x) (0 < (AAA_AUTHORIZED & x))
#define IS_NOT_AUTHORIZED(x) (!IS_AUTHORIZED(x))

#define IS_ACCOUNTING(x) (0 < (AAA_ACCOUNTING & x))
#define IS_NOT_ACCOUNTING(x) (!IS_ACCOUNTING(x))

/**
.. c:type:: np_aaatoken_t

   The np_aaatoken_t structure consists of the following data types:

.. c:member:: char[255] realm

   each token belongs to a realm which can be used to group several different tokens.
   (type should change to np_key_t in the future)

.. c:member:: char[255] issuer

   the sender or issuer of a token (type should change to np_key_t in the future)

.. c:member:: char[255] subject

   the subject which describes the contents of this token. can be a message subject (topic) or could be
   a node identity or ... (type should change to np_key_t in the future)

.. c:member:: char[255] audience

   the intended audience of a token. (type should change to np_key_t in the future)

.. c:member:: double issued_at

   date when the token was created

.. c:member:: double not_before

   date when the token will start to be valid

.. c:member:: double expiration

   expiration date of the token

.. c:member:: aaastate_type state

   internal state indicator whether this token is valid (remove ?)

.. c:member:: uuid_t uuid

   a uuid to identify this token (not sure if this is really required)

.. c:member:: unsigned char public_key[crypto_sign_BYTES]

   the public key of a identity

.. c:member:: unsigned char session_key[crypto_scalarmult_SCALARBYTES]

   the shared session key (used to store the node-2-node encryption)

.. c:member:: unsigned char private_key[crypto_sign_SECRETKEYBYTES]

   the private key of an identity

.. c:member:: np_tree_t* extensions

   a key-value jtree structure to add arbitrary informations to the token

   neuropil nodes can use the realm and issuer hash key informations to request authentication and authorization of a subject
   token can then be send to gather accounting information about message exchange
*/
struct np_aaatoken_s
{
	// link to memory management
	np_obj_t* obj;

	double version;

	char realm[255]; // owner or parent entity

	char issuer[255]; // from (can be self signed)
	char subject[255]; // about
	char audience[255]; // to

	double issued_at;
	double not_before;
	double expiration;

	aaastate_type state;

	char* uuid;

	unsigned char public_key[crypto_sign_PUBLICKEYBYTES];
	unsigned char session_key[crypto_scalarmult_SCALARBYTES];
	unsigned char private_key[crypto_sign_SECRETKEYBYTES];

	// key/value extension list
	np_tree_t* extensions;
} NP_API_EXPORT;

_NP_GENERATE_MEMORY_PROTOTYPES(np_aaatoken_t);


// serialization of the np_aaatoken_t structure
NP_API_INTERN
void np_encode_aaatoken(np_tree_t* data, np_aaatoken_t* token);
NP_API_INTERN
void np_decode_aaatoken(np_tree_t* data, np_aaatoken_t* token);

/**
.. c:function::np_bool token_is_valid(np_aaatoken_t* token)

   checks if a token is valid.
   performs a cryptographic integrity check with a checksum verification on the main data elements

   :param token: the token to check
   :return: a boolean indicating whether the token is valid
*/
NP_API_EXPORT
np_bool token_is_valid(np_aaatoken_t* token);

NP_API_INTERN
np_dhkey_t _np_create_dhkey_for_token(np_aaatoken_t* identity);

// neuropil internal aaatoken storage and exchange functions

NP_API_INTERN
void _np_add_sender_token(char* subject, np_aaatoken_t *token);
NP_API_INTERN
sll_return(np_aaatoken_t) _np_get_sender_token_all(char* subject);
NP_API_INTERN
np_aaatoken_t* _np_get_sender_token(char* subject, char* sender);

NP_API_INTERN
void _np_add_receiver_token(char* subject, np_aaatoken_t *token);
NP_API_INTERN
sll_return(np_aaatoken_t) _np_get_receiver_token_all(char* subject);
NP_API_INTERN
np_aaatoken_t* _np_get_receiver_token(char* subject);

NP_API_INTERN
void _np_aaatoken_add_signature(np_aaatoken_t* msg_token);

#ifdef __cplusplus
}
#endif

#endif // _NP_AAATOKEN_H_
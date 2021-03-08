//
// neuropil is copyright 2016-2019 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
// original version is based on the chimera project
#ifndef _NP_LOG_H_
#define _NP_LOG_H_

#include "neuropil.h"

#ifdef __cplusplus
extern "C" {
#endif
   enum np_log_e
   {
      LOG_NONE  		   = 0x00000000U, /* log nothing								*/
      LOG_NOMOD		   = 0x00000000U, /*											*/

      LOG_ERROR     	   = 0x00000001U, /* error messages							*/
      LOG_WARN       	= 0x00000002U, /* warning messages						    */
      LOG_INFO       	= 0x00000004U, /* info messages							    */
      LOG_DEBUG      	= 0x00000008U, /* debugging messages						*/
      LOG_TRACE         = 0x00000010U, /* tracing messages						    */
      LOG_VERBOSE       = 0x00000020U, /* verbose messages						    */

      LOG_SERIALIZATION	= 0x00000100U, /* debugging the serialization methods		*/
      LOG_MUTEX      	= 0x00000200U, /* debugging messages for mutex subsystem	*/
      LOG_KEY        	= 0x00000400U, /* debugging messages for key subsystem	    */
      LOG_NETWORK    	= 0x00000800U, /* debugging messages for network layer	    */
      LOG_ROUTING    	= 0x00001000U, /* debugging the routing table				*/
      LOG_MESSAGE    	= 0x00002000U, /* debugging the message subsystem			*/
      LOG_SECURE     	= 0x00004000U, /* debugging the security module			    */
      LOG_HTTP       	= 0x00008000U, /* debugging the http subsystem			    */
      LOG_AAATOKEN   	= 0x00010000U, /* debugging the aaatoken subsystem		    */
      LOG_MEMORY 		   = 0x00020000U, /* debugging the memory subsystem			*/
      LOG_SYSINFO		   = 0x00040000U, /* debugging the Sysinfo subsystem     	    */
      LOG_TREE			   = 0x00080000U, /* debugging the Tree subsystem     		    */
      LOG_THREADS		   = 0x00100000U, /* debugging the Threads subsystem     	    */
      LOG_MSGPROPERTY	= 0x00200000U, /* debugging the Messageproperties     	    */
      LOG_JOBS			   = 0x00400000U, /* debugging the Jobqueue subsystem     	    */
      LOG_EVENT			= 0x00800000U, /* debugging the undefined					*/
      LOG_MISC          = 0x01000000U, /* debugging the undefined					*/
      LOG_HANDSHAKE     = 0x02000000U, /* debugging the undefined					*/
      LOG_KEYCACHE      = 0x04000000U, /* debugging the undefined					*/

      LOG_GLOBAL     	= 0x80000000U, /* debugging the global system				*/

   } NP_ENUM NP_API_EXPORT;

   #define LOG_MODUL_MASK    0xFFFFFF00U /* filter the module mask */
   #define LOG_LEVEL_MASK    0x000000FFU /* filter the log level */

   #ifdef __cplusplus
}
#endif
#endif /* _NP_LOG_H_ */

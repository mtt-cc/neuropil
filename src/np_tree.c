//
// neuropil is copyright 2016 by pi-lar GmbH
// Licensed under the Open Software License (OSL 3.0), please see LICENSE file for details
//
// original version is based on the chimera project
/* Revision 1.2.  Jim Plank */

/* Original code by Jim Plank (plank@cs.utk.edu) */
/* modified for THINK C 6.0 for Macintosh by Chris Bartley */
/* modified for neuropil 2015 pi-lar GmbH Stephan Schwichtenberg */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <inttypes.h>

#include "np_tree.h"

#include "np_util.h"
#include "np_log.h"

RB_GENERATE(np_tree_s, np_tree_elem_s, link, _np_tree_elem_cmp);

//RB_GENERATE_STATIC(np_str_jtree, np_tree_elem_s, link, _np_tree_elem_cmp);
//RB_GENERATE_STATIC(np_int_jtree, np_tree_elem_s, link, _np_tree_elem_cmp);
//RB_GENERATE_STATIC(np_dbl_jtree, np_tree_elem_s, link, _np_tree_elem_cmp);
//RB_GENERATE_STATIC(np_ulong_jtree, np_tree_elem_s, link, _np_tree_elem_cmp);

np_tree_t* np_tree_create ()
{
	np_tree_t* new_tree = (np_tree_t*) malloc(sizeof(np_tree_t));
	CHECK_MALLOC(new_tree);

	new_tree->rbh_root = NULL;
	new_tree->size = 0;
	new_tree->byte_size = 5;

	return new_tree;
}

int16_t _np_tree_elem_cmp(const np_tree_elem_t* j1, const np_tree_elem_t* j2)
{
	assert(NULL != j1);
	assert(NULL != j2);

	np_treeval_t jv1 = j1->key;
	np_treeval_t jv2 = j2->key;

	if (jv1.type == jv2.type)
	{
		if (jv1.type == char_ptr_type)
			return strncmp(jv1.value.s, jv2.value.s, 64);

		if (jv1.type == double_type)
		{
			// log_msg(LOG_DEBUG, "comparing %f - %f = %d",
			// 		jv1.value.d, jv2.value.d, (int16_t) (jv1.value.d-jv2.value.d) );
			double res = jv1.value.d - jv2.value.d;
			if (res < 0) return -1;
			if (res > 0) return  1;
			return 0;
		}

		if (jv1.type == unsigned_long_type)
			return (int16_t) (jv1.value.ul - jv2.value.ul);

		if (jv1.type == int_type)
			return (int16_t) (jv1.value.i - jv2.value.i);
	}
	return (0);
};

np_tree_elem_t* np_tree_find_gte_str (np_tree_t* n, const char *key, uint8_t *fnd)
{
	assert(n   != NULL);
	assert(key != NULL);

	np_tree_elem_t* result = NULL;

	np_treeval_t search_key = { .type = char_ptr_type, .value.s = (char*) key };
	np_tree_elem_t search_elem = { .key = search_key };

	result = RB_NFIND(np_tree_s, n, &search_elem);
	if (NULL != result &&
		0    == strncmp(result->key.value.s, key, strlen(key)) )
	{
		*fnd = 1;
	}
	else
	{
		*fnd = 0;
	}
	return (result);
}

np_tree_elem_t* np_tree_find_str (np_tree_t* n, const char *key)
{
	assert(NULL != n);
	assert(NULL != key);

	np_treeval_t search_key = { .type = char_ptr_type, .value.s = (char*) key };
	np_tree_elem_t search_elem = { .key = search_key };
	return RB_FIND(np_tree_s, n, &search_elem);
}

np_tree_elem_t* np_tree_find_gte_int (np_tree_t* n, int16_t ikey, uint8_t *fnd)
{
	assert(n   != NULL);

	np_tree_elem_t* result = NULL;

	np_treeval_t search_key = { .type = int_type, .value.i = ikey };
	np_tree_elem_t search_elem = { .key = search_key };

	result = RB_NFIND(np_tree_s, n, &search_elem);
	if (NULL != result &&
		result->key.value.i == ikey )
	{
		*fnd = 1;
	}
	else
	{
		*fnd = 0;
	}

	return (result);
}

np_tree_elem_t* np_tree_find_int (np_tree_t* n, int16_t key)
{
	np_treeval_t search_key = { .type = int_type, .value.i = key };
	np_tree_elem_t search_elem = { .key = search_key };
	return (RB_FIND(np_tree_s, n, &search_elem));
}

np_tree_elem_t* np_tree_find_gte_ulong (np_tree_t* n, uint32_t ulkey, uint8_t *fnd)
{
	assert(n   != NULL);

	np_tree_elem_t* result = NULL;

	np_treeval_t search_key = { .type = unsigned_long_type, .value.ul = ulkey };
	np_tree_elem_t search_elem = { .key = search_key };

	result = RB_NFIND(np_tree_s, n, &search_elem);
	if (NULL != result &&
		result->key.value.ul == ulkey )
	{
		*fnd = 1;
	}
	else
	{
		*fnd = 0;
	}

	return (result);
}

np_tree_elem_t* np_tree_find_ulong (np_tree_t* n, uint32_t ulkey)
{
	np_treeval_t search_key = { .type = unsigned_long_type, .value.ul = ulkey };
	np_tree_elem_t search_elem = { .key = search_key };
	return (RB_FIND(np_tree_s, n, &search_elem));
}

np_tree_elem_t* np_tree_find_gte_dbl (np_tree_t* n, double dkey, uint8_t *fnd)
{
	assert(n   != NULL);

	np_tree_elem_t* result = NULL;

	np_treeval_t search_key = { .type = double_type, .value.d = dkey };
	np_tree_elem_t search_elem = { .key = search_key };

	result = RB_NFIND(np_tree_s, n, &search_elem);
	if (NULL != result &&
		result->key.value.d == dkey )
	{
		*fnd = 1;
	}
	else
	{
		*fnd = 0;
	}

	return (result);
}


np_tree_elem_t* np_tree_find_dbl (np_tree_t* n, double dkey)
{
	np_treeval_t search_key = { .type = double_type, .value.d = dkey };
	np_tree_elem_t search_elem = { .key = search_key };
	return (RB_FIND(np_tree_s, n, &search_elem));
}

void np_tree_del_str (np_tree_t* tree, const char *key)
{
	np_treeval_t search_key = { .type = char_ptr_type, .value.s = (char*) key };
	np_tree_elem_t search_elem = { .key = search_key };

	np_tree_elem_t* to_delete = RB_FIND(np_tree_s, tree, &search_elem);
	if (to_delete != NULL)
	{
		RB_REMOVE(np_tree_s, tree, to_delete);

		tree->byte_size -= np_tree_get_byte_size(to_delete);
		tree->size--;
		free(to_delete->key.value.s);

		if (to_delete->val.type == char_ptr_type) free(to_delete->val.value.s);
		if (to_delete->val.type == bin_type) free(to_delete->val.value.bin);
		if (to_delete->val.type == jrb_tree_type) np_tree_free(to_delete->val.value.tree);

		free (to_delete);
	}
}

void np_tree_del_int (np_tree_t* tree, const int16_t key)
{
	np_treeval_t search_key = { .type = int_type, .value.i = key };
	np_tree_elem_t search_elem = { .key = search_key };

	np_tree_elem_t* to_delete = RB_FIND(np_tree_s, tree, &search_elem);
	if (to_delete != NULL)
	{
		RB_REMOVE(np_tree_s, tree, to_delete);
		tree->byte_size -= np_tree_get_byte_size(to_delete);
		tree->size--;
		if (to_delete->val.type == char_ptr_type) free(to_delete->val.value.s);
		if (to_delete->val.type == bin_type) free(to_delete->val.value.bin);
		if (to_delete->val.type == jrb_tree_type) np_tree_free(to_delete->val.value.tree);

		free (to_delete);
	}
}

void np_tree_del_double (np_tree_t* tree, const double dkey)
{
	np_treeval_t search_key = { .type = double_type, .value.d = dkey };
	np_tree_elem_t search_elem = { .key = search_key };

	np_tree_elem_t* to_delete = RB_FIND(np_tree_s, tree, &search_elem);
	if (to_delete != NULL)
	{
		RB_REMOVE(np_tree_s, tree, to_delete);
		tree->byte_size -= np_tree_get_byte_size(to_delete);
		tree->size--;
		if (to_delete->val.type == char_ptr_type) free(to_delete->val.value.s);
		if (to_delete->val.type == bin_type) free(to_delete->val.value.bin);
		if (to_delete->val.type == jrb_tree_type) np_tree_free(to_delete->val.value.tree);
		free (to_delete);
	}
}

void np_tree_del_ulong (np_tree_t* tree, const uint32_t key)
{
	np_treeval_t search_key = { .type = unsigned_long_type, .value.ul = key };
	np_tree_elem_t search_elem = { .key = search_key };

	np_tree_elem_t* to_delete = RB_FIND(np_tree_s, tree, &search_elem);
	if (to_delete != NULL)
	{
		RB_REMOVE(np_tree_s, tree, to_delete);
		tree->byte_size -= np_tree_get_byte_size(to_delete);
		tree->size--;
		if (to_delete->val.type == char_ptr_type) free(to_delete->val.value.s);
		if (to_delete->val.type == bin_type) free(to_delete->val.value.bin);
		if (to_delete->val.type == jrb_tree_type) np_tree_free(to_delete->val.value.tree);

		free (to_delete);
	}
}

void np_tree_clear (np_tree_t* n)
{
	np_tree_elem_t* iter = RB_MIN(np_tree_s, n);
	np_tree_elem_t* tmp = NULL;

	if (NULL != iter)
	{
		do
		{
			tmp = iter;
			// log_msg(LOG_WARN, "jrb_free_tree: e->%p k->%p v->%p", tmp, tmp->key.value.s, &tmp->val);
			iter = RB_NEXT(np_tree_s, n, iter);
			// log_msg(LOG_WARN, "jrb_free_tree: t->%p i->%p", tmp, iter);

			switch (tmp->key.type)
			{
			case (char_ptr_type):
				np_tree_del_str(n, tmp->key.value.s);
				break;
			case (int_type):
				np_tree_del_int(n, tmp->key.value.i);
				break;
			case (double_type):
				np_tree_del_double(n, tmp->key.value.d);
				break;
			case (unsigned_long_type):
				np_tree_del_ulong(n, tmp->key.value.ul);
				break;
			default:
				break;
			}

		} while (NULL != iter);
	}
}

void np_tree_free (np_tree_t* n)
{
	if(NULL != n) {
		if(n->size > 0) {
			np_tree_clear(n);
		}
		free (n);
		n = NULL;
	}
}

void _np_print_tree (np_tree_t* n, uint8_t indent)
{
	np_tree_elem_t* tmp = NULL;

	RB_FOREACH(tmp, np_tree_s, n)
	{
		char s_indent[indent+1];
		memset(s_indent, ' ', indent);
		s_indent[indent] = '\0';

		if (tmp->key.type == char_ptr_type)      log_msg(LOG_DEBUG, "%s%s: %s", s_indent, np_treeval_to_str(tmp->key), np_treeval_to_str(tmp->val));
		if (tmp->key.type == int_type)           log_msg(LOG_DEBUG, "%s%s: %s", s_indent, np_treeval_to_str(tmp->key), np_treeval_to_str(tmp->val));
		if (tmp->key.type == double_type)        log_msg(LOG_DEBUG, "%s%s: %s", s_indent, np_treeval_to_str(tmp->key), np_treeval_to_str(tmp->val));
		if (tmp->key.type == unsigned_long_type) log_msg(LOG_DEBUG, "%s%s: %s", s_indent, np_treeval_to_str(tmp->key), np_treeval_to_str(tmp->val));

		if (tmp->val.type == jrb_tree_type) _np_print_tree(tmp->val.value.v, indent+1);
	}
}

void _np_tree_replace_all_with_str(np_tree_t* n, const char* key, np_treeval_t val)
{
	np_tree_clear(n);
    np_tree_insert_str(n, key, val);
}


uint64_t np_tree_get_byte_size(np_tree_elem_t* node)
{
	assert(node  != NULL);

	uint64_t byte_size = np_treeval_get_byte_size(node->key) + np_treeval_get_byte_size(node->val) ;


	return byte_size;
}

void np_tree_insert_str (np_tree_t* tree, const char *key, np_treeval_t val)
{
	assert(tree    != NULL);
	assert(key     != NULL);

	np_tree_elem_t* found = np_tree_find_str(tree, key);

	if (found == NULL)
	{
		// insert new value
		found = (np_tree_elem_t*) malloc(sizeof(np_tree_elem_t));
		CHECK_MALLOC(found);

		found->key.value.s = strndup(key, 255);
	    found->key.type = char_ptr_type;
	    found->key.size = strlen(key);

	    found->val = np_treeval_copy_of_val(val);
		// log_msg(LOG_WARN, "e->%p k->%p v->%p", found, found->key.value.s, &found->val);
		// log_msg(LOG_WARN, "e->%p k->%p v->%p", found, &found->key, &found->val);

		RB_INSERT(np_tree_s, tree, found);
	    tree->size++;
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_insert_int (np_tree_t* tree, int16_t ikey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_int(tree, ikey);

	if (found == NULL)
	{
		// insert new value
		found = (np_tree_elem_t*) malloc(sizeof(np_tree_elem_t));
		CHECK_MALLOC(found);

		// if (NULL == found) return;

	    found->key.value.i = ikey;
	    found->key.type = int_type;
	    found->key.size = sizeof(int16_t);
	    found->val = np_treeval_copy_of_val(val);

		RB_INSERT(np_tree_s, tree, found);
	    tree->size++;
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_insert_ulong (np_tree_t* tree, uint32_t ulkey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_ulong(tree, ulkey);

	if (found == NULL)
	{
		// insert new value
		found = (np_tree_elem_t*) malloc(sizeof(np_tree_elem_t));
		CHECK_MALLOC(found);

	    found->key.value.ul = ulkey;
	    found->key.type = unsigned_long_type;
	    found->key.size = sizeof(uint32_t);

	    found->val = np_treeval_copy_of_val(val);

		RB_INSERT(np_tree_s, tree, found);
	    tree->size++;
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_insert_dbl (np_tree_t* tree, double dkey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_dbl(tree, dkey);

	if (found == NULL)
	{
		// insert new value
		found = (np_tree_elem_t*) malloc(sizeof(np_tree_elem_t));
		CHECK_MALLOC(found);

		found->key.value.d = dkey;
	    found->key.type = double_type;
	    found->key.size = sizeof(double);

	    found->val = np_treeval_copy_of_val(val);

		RB_INSERT(np_tree_s, tree, found);
	    tree->size++;
		tree->byte_size += np_tree_get_byte_size(found);
	}
	else
	{
		// log_msg(LOG_WARN, "not inserting double key (%f) into jtree", dkey );
	}
}

void np_tree_replace_str (np_tree_t* tree, const char *key, np_treeval_t val)
{
	assert(tree    != NULL);
	assert(key     != NULL);

	np_tree_elem_t* found = np_tree_find_str(tree, key);

	if (found == NULL)
	{
		// insert new value
		np_tree_insert_str(tree, key, val);
	}
	else
	{
		// free up memory before replacing
		tree->byte_size -= np_tree_get_byte_size(found);

		if (found->val.type == char_ptr_type) free(found->val.value.s);
		if (found->val.type == bin_type)      free(found->val.value.bin);
		if (found->val.type == jrb_tree_type) np_tree_free(found->val.value.tree);

	    found->val = np_treeval_copy_of_val(val);
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_replace_int (np_tree_t* tree, int16_t ikey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_int(tree, ikey);

	if (found == NULL)
	{
		// insert new value
		np_tree_insert_int(tree, ikey, val);
	}
	else
	{
		tree->byte_size -= np_tree_get_byte_size(found);
		// free up memory before replacing
		if (found->val.type == char_ptr_type) free(found->val.value.s);
		if (found->val.type == bin_type)      free(found->val.value.bin);
		if (found->val.type == jrb_tree_type) np_tree_free(found->val.value.tree);

	    found->val = np_treeval_copy_of_val(val);

		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_replace_ulong (np_tree_t* tree, uint32_t ulkey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_ulong(tree, ulkey);

	if (found == NULL)
	{
		np_tree_insert_ulong(tree, ulkey, val);
	}
	else
	{
		tree->byte_size -= np_tree_get_byte_size(found);
		// free up memory before replacing
		if (found->val.type == char_ptr_type) free(found->val.value.s);
		if (found->val.type == bin_type)      free(found->val.value.bin);
		if (found->val.type == jrb_tree_type) np_tree_free(found->val.value.tree);

	    found->val = np_treeval_copy_of_val(val);
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

void np_tree_replace_dbl (np_tree_t* tree, double dkey, np_treeval_t val)
{
	assert(tree    != NULL);

	np_tree_elem_t* found = np_tree_find_dbl(tree, dkey);

	if (found == NULL)
	{
		// insert new value
		np_tree_insert_dbl(tree, dkey, val);
	}
	else
	{
		tree->byte_size -= np_tree_get_byte_size(found);
		// free up memory before replacing
		if (found->val.type == char_ptr_type) free(found->val.value.s);
		if (found->val.type == bin_type)      free(found->val.value.bin);
		if (found->val.type == jrb_tree_type) np_tree_free(found->val.value.tree);

	    found->val = np_treeval_copy_of_val(val);
		tree->byte_size += np_tree_get_byte_size(found);
	}
}

np_tree_t* np_tree_copy(np_tree_t* source) {

	np_tree_t* ret =	np_tree_create();
	np_tree_elem_t* tmp = NULL;
	RB_FOREACH(tmp, np_tree_s, source)
	{
		if (tmp->key.type == char_ptr_type)      np_tree_insert_str(ret, tmp->key.value.s, tmp->val);
		if (tmp->key.type == int_type)           np_tree_insert_int(ret, tmp->key.value.i, tmp->val);
		if (tmp->key.type == double_type)        np_tree_insert_dbl(ret, tmp->key.value.d, tmp->val);
		if (tmp->key.type == unsigned_long_type) np_tree_insert_ulong(ret, tmp->key.value.ul, tmp->val);
	}
	return ret;
}


void _np_tree_serialize(np_tree_t* jtree, cmp_ctx_t* cmp)
{
	uint16_t i = 0;
	// first assume a size based on jrb size
	// void* buf_ptr_map = cmp->buf;

	// if (!cmp_write_map(cmp, jrb->size*2 )) return;
	cmp_write_map32(cmp, jtree->size*2);

	// log_msg(LOG_DEBUG, "wrote jrb tree size %hd", jtree->size*2);
	// write jrb tree
	if (0 < jtree->size)
	{
		np_tree_elem_t* tmp = NULL;

		RB_FOREACH(tmp, np_tree_s, jtree)
		{
//			if (tmp)
//				log_msg(LOG_DEBUG, "%p: keytype: %hd, valuetype: %hd (size: %u)", tmp, tmp->key.type, tmp->val.type, tmp->val.size );

			if (int_type           == tmp->key.type ||
		 		unsigned_long_type == tmp->key.type ||
		 		double_type        == tmp->key.type ||
		 		char_ptr_type      == tmp->key.type)
		 	{
				// log_msg(LOG_DEBUG, "for (%p; %p!=%p; %p=%p) ", tmp->flink, tmp, msg->header, node, node->flink);
				__np_tree_serialize_write_type(tmp->key, cmp); i++;
				__np_tree_serialize_write_type(tmp->val, cmp); i++;
		 	}
			else
			{
				log_msg(LOG_DEBUG, "unknown key type for serialization" );
		 	}
		}
	}

	// void* buf_ptr = cmp->buf;
	// cmp->buf = buf_ptr_map;
	// cmp_write_map32(cmp, i );

	if (i != jtree->size*2)
		log_msg(LOG_ERROR, "serialized jrb size map size is %d, but should be %hd", jtree->size*2, i);

	// cmp->buf = buf_ptr;

//	switch (jrb->key.type) {
//	case int_type:
//		log_msg(LOG_DEBUG, "wrote int key (%hd)", jrb->key.value.i);
//		break;
//	case unsigned_long_type:
//		log_msg(LOG_DEBUG, "wrote uint key (%u)", jrb->key.value.ul);
//		break;
//	case double_type:
//		log_msg(LOG_DEBUG, "wrote double key (%f)", jrb->key.value.d);
//		break;
//	case char_ptr_type:
//		log_msg(LOG_DEBUG, "wrote str key (%s)", jrb->key.value.s);
//		break;
//	}
}


void _np_tree_deserialize(np_tree_t* jtree, cmp_ctx_t* cmp)
{
	cmp_object_t obj;

	uint32_t size = 0;

	cmp_read_map(cmp, &size);

	// if ( ((size%2) != 0) || (size <= 0) ) return;

	for (uint32_t i = 0; i < (size/2); i++)
	{
		// read key
		np_treeval_t tmp_key = { .type = none_type, .size = 0 };
		cmp_read_object(cmp, &obj);
		__np_tree_serialize_read_type(&obj, cmp, &tmp_key);
		if (none_type == tmp_key.type) {
			return;
		}

		// read value
		np_treeval_t tmp_val = { .type = none_type, .size = 0 };
		cmp_read_object(cmp, &obj);
		__np_tree_serialize_read_type(&obj, cmp, &tmp_val);
		if (none_type == tmp_val.type) {
			return;
		}

		switch (tmp_key.type)
		{
		case int_type:
			// log_msg(LOG_DEBUG, "read int key (%d)", tmp_key.value.i);
			np_tree_insert_int(jtree, tmp_key.value.i, tmp_val);
			break;
		case unsigned_long_type:
			// log_msg(LOG_DEBUG, "read uint key (%ul)", tmp_key.value.ul);
			np_tree_insert_ulong(jtree, tmp_key.value.ul, tmp_val);
			break;
		case double_type:
			// log_msg(LOG_DEBUG, "read double key (%f)", tmp_key.value.d);
			np_tree_insert_dbl(jtree, tmp_key.value.d, tmp_val);
			break;
		case char_ptr_type:
			// log_msg(LOG_DEBUG, "read str key (%s)", tmp_key.value.s);
			np_tree_insert_str(jtree, tmp_key.value.s, tmp_val);
			free (tmp_key.value.s);
			break;
		default:
			tmp_val.type = none_type;
			break;
		}

		if (tmp_val.type == char_ptr_type) free(tmp_val.value.s);
		if (tmp_val.type == bin_type)      free(tmp_val.value.bin);
		if (tmp_val.type == jrb_tree_type) np_tree_free(tmp_val.value.tree);
	}
	// log_msg(LOG_DEBUG, "read all key/value pairs from message part %p", jrb);
}

uint8_t __np_tree_serialize_read_type_key(void* buffer_ptr, np_treeval_t* target) {
	cmp_ctx_t cmp_key;
	cmp_init(&cmp_key, buffer_ptr, _np_buffer_reader, _np_buffer_writer);
	np_dhkey_t new_key;

	if (cmp_read_u64(&cmp_key, &(new_key.t[0])))
		if(cmp_read_u64(&cmp_key, &(new_key.t[1])))
			if(cmp_read_u64(&cmp_key, &(new_key.t[2])))
				cmp_read_u64(&cmp_key, &(new_key.t[3]));

	target->value.key = new_key;
	target->type = key_type;
	target->size = sizeof(np_dhkey_t);

	return cmp_key.error;
}

void __np_tree_serialize_write_type_key(np_dhkey_t source, cmp_ctx_t* target) {
	// source->size is not relevant here as the transport size includes marker sizes etc..
    //                        4 * size of uint64 marker + size of key element
	uint32_t transport_size = 4 * (sizeof(uint8_t) 		+ sizeof(uint64_t)); // 36 on x64

	cmp_ctx_t key_ctx;
	char buffer[transport_size];
	void* buf_ptr = buffer;
	cmp_init(&key_ctx, buf_ptr, _np_buffer_reader, _np_buffer_writer);

	cmp_write_u64(&key_ctx, source.t[0]);
	cmp_write_u64(&key_ctx, source.t[1]);
	cmp_write_u64(&key_ctx, source.t[2]);
	cmp_write_u64(&key_ctx, source.t[3]);

	cmp_write_ext32(target, key_type, transport_size, buf_ptr);
}

void __np_tree_serialize_write_type(np_treeval_t val, cmp_ctx_t* cmp)
{
	// void* count_buf_start = cmp->buf;
	// log_msg(LOG_DEBUG, "writing jrb (%p) value: %s", jrb, jrb->key.value.s);
	switch (val.type)
	{
	// signed numbers
	case short_type:
		cmp_write_s8(cmp, val.value.sh);
		break;
	case int_type:
		cmp_write_s16(cmp, val.value.i);
		break;
	case long_type:
		cmp_write_s32(cmp, val.value.l);
		break;
	case long_long_type:
		cmp_write_s64(cmp, val.value.ll);
		break;
		// characters
	case char_ptr_type:
		//log_msg(LOG_DEBUG, "string size %u/%lu -> %s", val.size, strlen(val.value.s), val.value.s);
		cmp_write_str32(cmp, val.value.s, val.size);
		break;

	case char_type:
		cmp_write_fixstr(cmp, (const char*) &val.value.c, sizeof(char));
		break;
//	case unsigned_char_type:
//	 	cmp_write_str(cmp, (const char*) &val.value.uc, sizeof(unsigned char));
//	 	break;

	// float and double precision
	case float_type:
		cmp_write_float(cmp, val.value.f);
		break;
	case double_type:
		cmp_write_double(cmp, val.value.d);
		break;

	// unsigned numbers
	case unsigned_short_type:
		cmp_write_u8(cmp, val.value.ush);
		break;
	case unsigned_int_type:
		cmp_write_u16(cmp, val.value.ui);
		break;
	case unsigned_long_type:
		cmp_write_u32(cmp, val.value.ul);
		break;
	case unsigned_long_long_type:
		cmp_write_u64(cmp, val.value.ull);
		break;

	case uint_array_2_type:
		cmp_write_fixarray(cmp, 2);
		cmp->write(cmp, &val.value.a2_ui[0], sizeof(uint16_t));
		cmp->write(cmp, &val.value.a2_ui[1], sizeof(uint16_t));
		break;

	case float_array_2_type:
	case char_array_8_type:
	case unsigned_char_array_8_type:
		log_msg(LOG_WARN, "please implement serialization for type %hhd", val.type);
		break;

	case void_type:
		log_msg(LOG_WARN, "please implement serialization for type %hhd", val.type);
		break;

	case bin_type:
		cmp_write_bin32(cmp, val.value.bin, val.size);
		//log_msg(LOG_DEBUG, "BIN size %"PRIu32, val.size);
		break;

	case key_type:
		{
			__np_tree_serialize_write_type_key(val.value.key, cmp);
			break;
		}

	case hash_type:
		// log_msg(LOG_DEBUG, "adding hash value %s to serialization", val.value.s);
		cmp_write_ext32(cmp, hash_type, val.size, val.value.bin);
		break;

	case jrb_tree_type:
		{
			cmp_ctx_t tree_cmp;
			char buffer[val.size];
			// log_msg(LOG_DEBUG, "buffer size for subtree %u (%hd %llu)", val.size, val.value.tree->size, val.value.tree->byte_size);
			// log_msg(LOG_DEBUG, "buffer size for subtree %u", val.size);
			void* buf_ptr = buffer;
			cmp_init(&tree_cmp, buf_ptr, _np_buffer_reader, _np_buffer_writer);
			_np_tree_serialize(val.value.tree, &tree_cmp);
			uint32_t buf_size = tree_cmp.buf - buf_ptr;

			// void* top_buf_ptr = cmp->buf;
			// write the serialized tree to the upper level buffer
			if (!cmp_write_ext32(cmp, jrb_tree_type, buf_size, buf_ptr))
			{
				log_msg(LOG_WARN, "couldn't write tree data -- ignoring for now");
			}
			// uint32_t top_buf_size = cmp->buf-top_buf_ptr;

//			else {
			// log_msg(LOG_DEBUG, "wrote tree structure size pre: %hu/%hu post: %hu %hu", val.size, val.value.tree->byte_size, buf_size, top_buf_size);
//			}
		}
		break;
	default:
		log_msg(LOG_WARN, "please implement serialization for type %hhd", val.type);
		break;
	}
}

void __np_tree_serialize_read_type(cmp_object_t* obj, cmp_ctx_t* cmp, np_treeval_t* value)
{
	switch (obj->type)
	{
	case CMP_TYPE_FIXMAP:
	case CMP_TYPE_MAP16:
	case CMP_TYPE_MAP32:
		log_msg(LOG_WARN,
				"error de-serializing message to normal form, found map type");
		break;

	case CMP_TYPE_FIXARRAY:
		if (2 == obj->as.array_size)
		{
			cmp->read(cmp, &value->value.a2_ui[0], sizeof(uint16_t));
			cmp->read(cmp, &value->value.a2_ui[1], sizeof(uint16_t));
			value->type = uint_array_2_type;
		}
		break;

	case CMP_TYPE_ARRAY16:
	case CMP_TYPE_ARRAY32:
		log_msg(LOG_WARN,
				"error de-serializing message to normal form, found array type");
		break;

	case CMP_TYPE_FIXSTR:
		cmp->read(cmp, &value->value.c, sizeof(char));
		value->type = char_type;
		value->size = 1;
		break;

	case CMP_TYPE_STR8:
	case CMP_TYPE_STR16:
	case CMP_TYPE_STR32:
		{
			value->type = char_ptr_type;
			value->size = obj->as.str_size;
			value->value.s = (char*) malloc(obj->as.str_size+1);
			CHECK_MALLOC(value->value.s);

			cmp->read(cmp, value->value.s, obj->as.str_size * sizeof(char));
			value->value.s[obj->as.str_size] = '\0';
			//log_msg(LOG_DEBUG, "string size %u/%lu -> %s", value->size, strlen(value->value.s), value->value.s);
			break;
		}
	case CMP_TYPE_BIN8:
	case CMP_TYPE_BIN16:
	case CMP_TYPE_BIN32:
		{
			value->type = bin_type;
			value->size = obj->as.bin_size;
			value->value.bin = malloc(obj->as.bin_size);
			// value->value.bin = calloc(1, value->size);
			CHECK_MALLOC(value->value.bin);
			// log_msg(LOG_DEBUG, "BIN size %"PRIu32, value->size);

			memset(value->value.bin, 0, value->size);
			cmp->read(cmp, value->value.bin, obj->as.bin_size);
			break;
		}
	case CMP_TYPE_NIL:
		log_msg(LOG_WARN, "unknown de-serialization for given type (cmp NIL) ");
		break;

	case CMP_TYPE_BOOLEAN:
		log_msg(LOG_WARN,
				"unknown de-serialization for given type (cmp boolean) ");
		break;

	case CMP_TYPE_EXT8:
	case CMP_TYPE_EXT16:
	case CMP_TYPE_EXT32:
	case CMP_TYPE_FIXEXT1:
	case CMP_TYPE_FIXEXT2:
	case CMP_TYPE_FIXEXT4:
	case CMP_TYPE_FIXEXT8:
	case CMP_TYPE_FIXEXT16:
		{
			// required for tree de-serialization
			// log_msg(LOG_DEBUG, "now reading cmp-extension type %hhd size %u", obj->as.ext.type, obj->as.ext.size);
			char buffer[obj->as.ext.size];
			void* buf_ptr = buffer;
			cmp->read(cmp, buf_ptr, obj->as.ext.size);

			// log_msg(LOG_DEBUG, "read %u bytes ", (cmp->buf - buf_ptr));
			if (obj->as.ext.type == jrb_tree_type)
			{
				// tree type
				np_tree_t* subtree = np_tree_create();
				cmp_ctx_t tree_cmp;
				cmp_init(&tree_cmp, buf_ptr, _np_buffer_reader, _np_buffer_writer);
				_np_tree_deserialize(subtree, &tree_cmp);
				// TODO: check if the complete buffer was read (byte count match)

				value->value.tree = subtree;
				value->type = jrb_tree_type;
				value->size = subtree->size;
				// log_msg(LOG_DEBUG, "read tree structure %u size %hu %lu", (tree_cmp.buf-buf_ptr), subtree->size, subtree->byte_size);
			}
			else if (obj->as.ext.type == key_type)
			{
				__np_tree_serialize_read_type_key(buf_ptr, value);
			}
			else if (obj->as.ext.type == hash_type)
			{
				value->type = hash_type;
				value->size = obj->as.ext.size;

				value->value.bin = (char*) malloc(obj->as.ext.size);
				CHECK_MALLOC(value->value.bin);

				memset(value->value.bin, 0, value->size);
				memcpy(value->value.bin, buffer, obj->as.ext.size);
				// value->value.s[obj->as.ext.size] = '\0';
				// log_msg(LOG_DEBUG, "reading hash value %s from serialization", value->value.s);
			}
			else
			{
				log_msg(LOG_WARN,
						"unknown de-serialization for given extension type %hhd", obj->as.ext.type);
			}
		}
		break;
	case CMP_TYPE_FLOAT:
		value->value.f = 0.0;
		value->value.f = obj->as.flt;
		value->type = float_type;
		break;

	case CMP_TYPE_DOUBLE:
		value->value.d = 0.0;
		value->value.d = obj->as.dbl;
		value->type = double_type;
		break;

	case CMP_TYPE_POSITIVE_FIXNUM:
	case CMP_TYPE_UINT8:
		value->value.ush = obj->as.u8;
		value->type = unsigned_short_type;
		break;
	case CMP_TYPE_UINT16:
		value->value.ui = 0;
		value->value.ui = obj->as.u16;
		value->type = unsigned_int_type;
		break;
	case CMP_TYPE_UINT32:
		value->value.ul = 0;
		value->value.ul = obj->as.u32;
		value->type = unsigned_long_type;
		break;

	case CMP_TYPE_UINT64:
		value->value.ull = 0;
		value->value.ull = obj->as.u64;
		value->type = unsigned_long_long_type;
		break;

	case CMP_TYPE_NEGATIVE_FIXNUM:
	case CMP_TYPE_SINT8:
		value->value.sh = obj->as.s8;
		value->type = short_type;
		break;

	case CMP_TYPE_SINT16:
		value->value.i = 0;
		value->value.i = obj->as.s16;
		value->type = int_type;
		break;

	case CMP_TYPE_SINT32:
		value->value.l = obj->as.s32;
		value->type = long_type;
		break;

	case CMP_TYPE_SINT64:
		value->value.ll = 0;
		value->value.ll = obj->as.s64;
		value->type = long_long_type;
		break;
	default:
		value->type = none_type;
		log_msg(LOG_WARN, "unknown deserialization for given type");
		break;
	}
}

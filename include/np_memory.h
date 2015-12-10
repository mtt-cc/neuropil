/**
 *  copyright 2015 pi-lar GmbH
 *  header only implementation to manage heap objects
 *  taking the generating approach using the c preprocessor
 *  Stephan Schwichtenberg
 **/

#ifndef _NP_MEMORY_H
#define _NP_MEMORY_H

#include "include.h"
#include "stdint.h"

// enum to identify the correct type of objects
typedef enum np_obj_type {
	np_none_t_e = 0,
	np_message_t_e,
	np_node_t_e,
	np_key_t_e, // not yet used
	np_aaatoken_t_e,
	np_msgproperty_t_e,
	test_struct_t_e = 99
} np_obj_enum;

typedef void (*np_dealloc_t) (void* data);
typedef void (*np_alloc_t) (void* data);

void del_callback(void* data);
void new_callback(void* data);

/** np_obj_t
 **
 ** void* like wrapper around structures to allow ref counting and null pointer checking
 ** each np_new_obj needs a corresponding np_free_obj
 ** if other methods would like to claim ownership, they should call np_ref_obj, np_unref_obj
 ** will release the object again (and possible delete it)
 **/
struct np_obj_s {

	np_obj_enum type;
	int16_t ref_count;
	void* ptr;

	np_dealloc_t del_callback;
	np_alloc_t   new_callback;

	// np_obj_t* prev;
	np_obj_t* next;

	pthread_mutex_t lock;
};


/** np_obj_pool_t
 **
 ** global object pool to store and handle all heap objects
 **/
struct np_obj_pool_s {
	np_obj_t* current;
	np_obj_t* first;
	// np_obj_t* last;

	np_obj_t* free_obj;
	// we need these two extensions
	uint32_t size;
	uint32_t available;

	pthread_mutex_t lock;
};

// macro definitions to generate header prototype definitions
#define _NP_GENERATE_MEMORY_PROTOTYPES(TYPE) \
void TYPE##_new(void*); \
void TYPE##_del(void*); \

// macro definitions to generate implementation of prototypes
#define _NP_GENERATE_MEMORY_IMPLEMENTATION(TYPE)

// convenience wrappers
#define np_ref_obj(TYPE, np_obj)              \
{                                             \
  pthread_mutex_lock(&(np_obj_pool->lock));   \
  assert (np_obj->obj != NULL);               \
  assert (np_obj->obj->type == TYPE##_e);     \
  np_mem_refobj(TYPE##_e, np_obj->obj);       \
  pthread_mutex_unlock(&(np_obj_pool->lock)); \
}

//printf("np_unref_obj: %20s:%d", __func__, __LINE__); \
//fflush(stdout); \
//printf(" %p", np_obj); \
//fflush(stdout); \
//printf(" obj:%p %d %d", np_obj->obj, np_obj->obj->ref_count, np_obj->obj->type); \
//fflush(stdout); \
//printf(" %p \n", np_obj->obj->ptr); \
//fflush(stdout);


#define np_unref_obj(TYPE, np_obj)            \
{                                             \
  pthread_mutex_lock(&(np_obj_pool->lock));   \
  assert (np_obj->obj != NULL);               \
  assert (np_obj->obj->type == TYPE##_e);     \
  assert (np_obj->obj->ptr != NULL);          \
  np_mem_unrefobj(TYPE##_e, np_obj->obj);     \
  if (NULL != np_obj->obj && np_obj->obj->ref_count <= 0 && np_obj->obj->ptr == np_obj) { \
    if (np_obj->obj->type != np_none_t_e)     \
    {                                         \
      np_obj->obj->del_callback(np_obj);      \
	  np_mem_freeobj(TYPE##_e, &np_obj->obj); \
	  np_obj->obj->ptr = NULL;                \
	  np_obj->obj = NULL;                     \
	  free(np_obj);                           \
	  np_obj = NULL;                          \
    }                                         \
  }                                           \
  pthread_mutex_unlock(&(np_obj_pool->lock)); \
}

//printf("np_new_obj  : %20s:%d", __func__, __LINE__); \
//fflush(stdout); \
//printf(" %p obj:%p %d %d %p \n", np_obj, np_obj->obj, np_obj->obj->ref_count, np_obj->obj->type, np_obj->obj->ptr); \
//fflush(stdout);


#define np_new_obj(TYPE, np_obj)              \
{                                             \
  pthread_mutex_lock(&(np_obj_pool->lock));   \
  np_obj = (TYPE*) malloc(sizeof(TYPE));      \
  np_mem_newobj(TYPE##_e, &np_obj->obj);      \
  np_obj->obj->new_callback = TYPE##_new;     \
  np_obj->obj->del_callback = TYPE##_del;     \
  np_obj->obj->new_callback(np_obj);          \
  np_obj->obj->ptr = np_obj;                  \
  np_mem_refobj(TYPE##_e, np_obj->obj);       \
  pthread_mutex_unlock(&(np_obj_pool->lock)); \
}


//printf("np_free_obj : %20s:%d", __func__, __LINE__); \
//fflush(stdout); \
//printf(" %p", np_obj); \
//fflush(stdout); \
//printf(" obj:%p %d %d", np_obj->obj, np_obj->obj->ref_count, np_obj->obj->type); \
//fflush(stdout); \
//printf(" %p \n", np_obj->obj->ptr); \
//fflush(stdout);


#define np_free_obj(TYPE, np_obj)             \
{                                             \
  pthread_mutex_lock(&(np_obj_pool->lock));   \
  np_mem_unrefobj(TYPE##_e, np_obj->obj);     \
  if (NULL != np_obj->obj && np_obj->obj->ref_count <= 0 && np_obj->obj->ptr == np_obj) { \
    if (np_obj->obj->type != np_none_t_e)     \
    {                                         \
      np_obj->obj->del_callback(np_obj);      \
	  np_mem_freeobj(TYPE##_e, &np_obj->obj); \
	  np_obj->obj->ptr = NULL;                \
	  np_obj->obj = NULL;                     \
	  free(np_obj);                           \
	  np_obj = NULL;                          \
    }                                         \
  }                                           \
  pthread_mutex_unlock(&(np_obj_pool->lock)); \
}


/**
 ** following this line: np_memory cache and object prototype definitions
 **/
void np_mem_init();

void np_mem_newobj(np_obj_enum obj_type, np_obj_t** obj);
// np_free - free resources (but not object wrapper) if ref_count is <= 0
// in case of doubt, call np_free. it will not harm ;-)
void np_mem_freeobj(np_obj_enum obj_type, np_obj_t** obj);
// increase ref count
void np_mem_refobj(np_obj_enum obj_type, np_obj_t* obj);
// decrease ref count
void np_mem_unrefobj(np_obj_enum obj_type, np_obj_t* obj);

// print the complete object list and statistics
void np_mem_printpool();


#endif // _NP_MEMORY_H

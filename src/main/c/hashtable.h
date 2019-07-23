#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "types.h"

#define HASHTABLE_ERROR    -1
#define HASHTABLE_NEW      0
#define HASHTABLE_EXISTING 1
#define MAX_SLOTS          100

typedef struct tag_LIST_NODE {
  char *key_cls_name;
  char *key_meth_name;
  char *key_meth_sig;
  void *value;
  struct tag_LIST_NODE *next;
} LIST_NODE;

typedef LIST_NODE* HASHTABLE[MAX_SLOTS];

#ifdef __cplusplus
extern "C" {
#endif
  HASHTABLE *hashtable_create();
  void hashtable_free(HASHTABLE *h, void (*freeing_visitor)(void *));
  void *hashtable_get(HASHTABLE *h, char *key_cls_name, char *key_meth_name, char *key_meth_sig);
  LIST_NODE *hashtable_insert(HASHTABLE *h, char *key_cls_name, char *key_meth_name, char *key_meth_sig, int *status);
  void hashtable_entry_set_key(LIST_NODE *entry, char *key_cls_name, char *key_meth_name, char *key_meth_sig);
  void hashtable_entry_set_value(LIST_NODE *entry, void *value);
  void *hashtable_entry_get_value(LIST_NODE *entry);
  void hashtable_iterate(HASHTABLE *h, void (*visitor)(const char *, const char *, const char *, void *));
#ifdef __cplusplus
}
#endif

#endif

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "types.h"

#define MAX_SLOTS 100

typedef struct tag_LIST_NODE {
  char *key;
  void *value;
  struct tag_LIST_NODE *next;
} LIST_NODE;

typedef LIST_NODE* HASHTABLE[MAX_SLOTS];

#ifdef __cplusplus
extern "C" {
#endif
  HASHTABLE *hashtable_create();
  void hashtable_free(HASHTABLE *h);
  void *hashtable_get(HASHTABLE *h, char *key);
  BOOL hashtable_insert(HASHTABLE *h, char *key, void *value);
  void hashtable_iterate(HASHTABLE *h, void (*visitor)(const char *, void *));
#ifdef __cplusplus
}
#endif

#endif

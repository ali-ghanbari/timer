#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static inline WORD jenkins_hash(const char* key) {
  WORD hash = 0;
  int i = 0;
  while (key[i]) {
    hash += key[i];
    hash += hash << 10;
    hash ^= hash >> 6;
    i++;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

HASHTABLE *hashtable_create() {
  HASHTABLE *h = (HASHTABLE *) malloc(sizeof(HASHTABLE));
  memset(h, 0, sizeof(HASHTABLE));
  return h;
}

void hashtable_free(HASHTABLE *h) {
  LIST_NODE *prev;
  LIST_NODE *head;
  int bucket_no;
  for (bucket_no = 0; bucket_no < MAX_SLOTS; bucket_no++) {
    head = (*h)[bucket_no];
    prev = head;
    while (head != NULL) {
      prev = head;
      head = head->next;
      free(prev);
    }
  }
  free(h);
}

void *hashtable_get(HASHTABLE *h, char *key)
{
  const int bucket_no = jenkins_hash(key) % (WORD) MAX_SLOTS;
  LIST_NODE *head = (*h)[bucket_no];
  while (head != NULL) {
    if (head->key == key || (head->key != NULL && key != NULL && !strcmp(head->key, key))) {
      break;
    }
    head = head->next;
  }
  if (head == NULL) {
    return NULL;
  }
  return head->value;
}

BOOL hashtable_insert(HASHTABLE *h, char *key, void *value) {
  const int bucket_no = jenkins_hash(key) % (WORD) MAX_SLOTS;
  LIST_NODE *head = (*h)[bucket_no];
  if (head == NULL) {
    LIST_NODE *node = (LIST_NODE *) malloc(sizeof(LIST_NODE));
    if (node == NULL) {
      return FALSE;
    }
    node->key = key;
    node->value = value;
    node->next=NULL;
    (*h)[bucket_no] = node;
  } else {
    LIST_NODE *prev = head;
    while (head != NULL) {
      if (head->key == key || (head->key != NULL && key != NULL && !strcmp(head->key, key))) {
	break;
      }
      prev = head;
      head = head->next;
    }
    if (head != NULL) {
      head->value = value; // exisitng key; update the value
    } else {
      // add a new entry
      LIST_NODE *node = (LIST_NODE *) malloc(sizeof(LIST_NODE));
      if (node == NULL) {
	return FALSE;
      }
      node->key = key;
      node->value = value;
      node->next = NULL;
      prev->next = node;
    }
  }
  return TRUE;
}

void hashtable_iterate(HASHTABLE *h, void (*visitor)(const char *, void *)) {
  LIST_NODE *head;
  int bucket_no;
  for (bucket_no = 0; bucket_no < MAX_SLOTS; bucket_no++) {
    head = (*h)[bucket_no];
    while (head != NULL) {
      visitor(head->key, head->value);
      head = head->next;
    }
  }
}

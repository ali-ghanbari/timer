#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static inline WORD jenkins_hash(const char* key)
{
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

HASHTABLE *hashtable_create()
{
  HASHTABLE *h = (HASHTABLE *) malloc(sizeof(HASHTABLE));
  memset(h, 0, sizeof(HASHTABLE));
  return h;
}

void hashtable_free(HASHTABLE *h, void (*freeing_visitor)(void *))
{
  LIST_NODE *prev;
  LIST_NODE *head;
  int bucket_no;
  for (bucket_no = 0; bucket_no < MAX_SLOTS; bucket_no++) {
    head = (*h)[bucket_no];
    prev = head;
    while (head != NULL) {
      prev = head;
      head = head->next;
      free(prev->key_cls_name);
      free(prev->key_meth_name);
      free(prev->key_meth_sig);
      freeing_visitor(prev->value);
      free(prev);
    }
  }
  free(h);
}

static inline BOOL key_equals(LIST_NODE *n, char *c_name, char *m_name, char *m_sig)
{
  return (n->key_cls_name == c_name || (n->key_cls_name != NULL && c_name != NULL && !strcmp(n->key_cls_name, c_name)))
    && (n->key_meth_name == m_name || (n->key_meth_name != NULL && m_name != NULL && !strcmp(n->key_meth_name, m_name)))
    && (n->key_meth_sig == m_sig || (n->key_meth_sig != NULL && m_sig != NULL && !strcmp(n->key_meth_sig, m_sig)));
}

void *hashtable_get(HASHTABLE *h, char *key_cls_name, char *key_meth_name, char *key_meth_sig)
{
  const int bucket_no = jenkins_hash(key_meth_name) % (WORD) MAX_SLOTS;
  LIST_NODE *head = (*h)[bucket_no];
  while (head != NULL) {
    if (key_equals(head, key_cls_name, key_meth_name, key_meth_sig)) {
      break;
    }
    head = head->next;
  }
  if (head == NULL) {
    return NULL;
  }
  return head->value;
}

LIST_NODE *hashtable_insert(HASHTABLE *h, char *key_cls_name, char *key_meth_name, char *key_meth_sig, int *status)
{
  const int bucket_no = jenkins_hash(key_meth_name) % (WORD) MAX_SLOTS;
  LIST_NODE *head = (*h)[bucket_no];
  if (head == NULL) {
    LIST_NODE *node = (LIST_NODE *) malloc(sizeof(LIST_NODE));
    if (node == NULL) {
      if (status != NULL) {
	*status = HASHTABLE_ERROR;
      }
      return NULL;
    }
    node->key_cls_name = key_cls_name;
    node->key_meth_name = key_meth_name;
    node->key_meth_sig = key_meth_sig;
    node->value = NULL;
    node->next = NULL;
    (*h)[bucket_no] = node;
    if (status != NULL) {
      *status = HASHTABLE_NEW;
    }
    return node;
  }
  LIST_NODE *prev = head;
  while (head != NULL) {
    if (key_equals(head, key_cls_name, key_meth_name, key_meth_sig)) {
      break;
    }
    prev = head;
    head = head->next;
  }
  if (head != NULL) { // entry is exisiting
    if (status != NULL) {
      *status = HASHTABLE_EXISTING;
    }
    return head;
  }
  // add a new entry
  LIST_NODE *node = (LIST_NODE *) malloc(sizeof(LIST_NODE));
  if (node == NULL) {
    if (status != NULL) {
      *status = HASHTABLE_ERROR;
    }
    return NULL;
  }
  node->key_cls_name = key_cls_name;
  node->key_meth_name = key_meth_name;
  node->key_meth_sig = key_meth_sig;
  node->value = NULL;
  node->next = NULL;
  prev->next = node;
  if (status != NULL) {
    *status = HASHTABLE_NEW;
  }
  return node;
}

void hashtable_entry_set_key(LIST_NODE *entry, char *key_cls_name, char *key_meth_name, char *key_meth_sig)
{
  entry->key_cls_name = key_cls_name;
  entry->key_meth_name = key_meth_name;
  entry->key_meth_sig = key_meth_sig;
}

void hashtable_entry_set_value(LIST_NODE *entry, void *value)
{
  entry->value = value;
}

void *hashtable_entry_get_value(LIST_NODE *entry)
{
  return entry->value;
}

void hashtable_iterate(HASHTABLE *h, void (*visitor)(const char *, const char *, const char *, void *))
{
  LIST_NODE *head;
  int bucket_no;
  for (bucket_no = 0; bucket_no < MAX_SLOTS; bucket_no++) {
    head = (*h)[bucket_no];
    while (head != NULL) {
      visitor(head->key_cls_name, head->key_meth_name, head->key_meth_sig, head->value);
      head = head->next;
    }
  }
}

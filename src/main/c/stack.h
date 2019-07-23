#ifndef __STACK_H__
#define __STACK_H__

#include <time.h>

typedef struct tag_STACK_NODE {
  struct timespec t;
  struct tag_STACK_NODE *next;
} STACK_NODE;

typedef STACK_NODE *STACK;

#ifdef __cplusplus
extern "C" {
#endif
  STACK stack_create();
  void stack_push(STACK *stack, struct timespec *t);
  STACK_NODE *stack_pop(STACK *stack);
  struct timespec *stack_node_get_value(STACK_NODE *node);
#ifdef __cplusplus
}
#endif
  
#endif

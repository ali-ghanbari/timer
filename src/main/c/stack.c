#include <string.h>
#include <stdlib.h>
#include "stack.h"

STACK stack_create()
{
  return NULL;
}

void stack_push(STACK *stack, struct timespec *t)
{
  STACK_NODE *node = (STACK_NODE *) malloc(sizeof(STACK_NODE));
  node->next = *stack;
  memcpy(&(node->t), t, sizeof(struct timespec));
  *stack = node;
}

STACK_NODE *stack_pop(STACK *stack)
{
  if (*stack == NULL) {
    return NULL;
  }
  STACK_NODE *top = *stack;
  *stack = (*stack)->next;
  return top;
}

struct timespec *stack_node_get_value(STACK_NODE *node)
{
  return &(node->t);
}

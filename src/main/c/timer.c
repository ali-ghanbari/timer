#include <jvmti.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "hashtable.h"
#include "stack.h"
#include "types.h"

typedef struct {
  STACK start_ts;
  DWORD max_us;
} METH_INFO;

/* Global agent data structure */
typedef struct {
  /* JVMTI Environment */ 
  jvmtiEnv *jvmti;
  jboolean vm_is_started;
  /* Data access Lock */
  jrawMonitorID lock;
  char *prefix;
  int prefix_len;
  HASHTABLE *methods_stat;
} GlobalAgentData;

static GlobalAgentData *gdata;

static inline BOOL filter(const char *cls_name)
{
  int lenpre = gdata->prefix_len;
  int lenstr = strlen(cls_name);
  return lenstr < lenpre ? FALSE : strncmp(gdata->prefix, cls_name, lenpre) == 0;
}

static inline DWORD max(DWORD a, DWORD b)
{
  return a >= b ? a : b;
}

static inline BOOL check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const char *str)
{
  if (errnum != JVMTI_ERROR_NONE) {
    char *errnum_str;
    errnum_str = NULL;
    (*jvmti)->GetErrorName(jvmti, errnum, &errnum_str);
    printf("ERROR: JVMTI: %d(%s): %s\n", errnum,
	   (errnum_str == NULL ? "Unknown" : errnum_str),
	   (str == NULL ? "" : str));
    return FALSE;
  }
  return TRUE;
}

/* static void enter_critical_section(jvmtiEnv *jvmti) */
/* { */
/*   jvmtiError error; */
/*   error = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock); */
/*   check_jvmti_error(jvmti, error, "Cannot enter with raw monitor"); */
/* }                   */

/* static void exit_critical_section(jvmtiEnv *jvmti) */
/* { */
/*   jvmtiError error; */
/*   error = (*jvmti)->RawMonitorExit(jvmti, gdata->lock); */
/*   check_jvmti_error(jvmti, error, "Cannot exit with raw monitor"); */
/* } */

/* static void JNICALL callbackVMInit(jvmtiEnv * jvmti, JNIEnv * env, jthread thread) */
/* { */
/* } */

/* static void JNICALL callbackVMDeath(jvmtiEnv *jvmti_env, JNIEnv* jni_env) */
/* { */
/* } */

/* static void JNICALL callbackException(jvmtiEnv *jvmti_env, */
/* 				      JNIEnv* jni_env, */
/* 				      jthread thread, */
/* 				      jmethodID method, */
/* 				      jlocation location, */
/* 				      jobject exception, */
/* 				      jmethodID catch_method, */
/* 				      jlocation catch_location) */
/* { */
/* //  jvmtiThreadGroupInfo groupInfo; */
/* //  jvmtiThreadInfo info; */
/* //  jvmtiError err; */
/* //  char* name; */
/* //  char* sig; */
/* //  err = (*jvmti_env)->GetMethodName(jvmti, method, &name, &sig, NULL); */
/* //  printf("Exception in Method:%s%s\n", name, sig); */
/* //  err = (*jvmti_env)->GetThreadInfo(jvmti_env, thread, &info); */
/* //  if (err == JVMTI_ERROR_NONE) { */
/* //    err = (*jvmti_env)->GetThreadGroupInfo(jvmti_env, info.thread_group, &groupInfo); */
/* //    if (err == JVMTI_ERROR_NONE ) { */
/* //      printf("Got Exception event, Current Thread is : %s and Thread Group is: %s\n", */
/* //	     ((info.name==NULL) ? "" : info.name), */
/* //	     groupInfo.name); */
/* //    } */
/* //  } */
/* } */

/* static void JNICALL callbackVMObjectAlloc(jvmtiEnv *jvmti_env, */
/* 					  JNIEnv* jni_env,  */
/* 					  jthread thread,  */
/* 					  jobject object,  */
/* 					  jclass object_klass,  */
/* 					  jlong size) */
/* { */
/* } */

void JNICALL method_entry_handler(jvmtiEnv *jvmti,
				  JNIEnv *jni_env,
				  jthread thread,
				  jmethodID method)
{
  jvmtiError err;
  jclass cls;
  char *cls_name;
  err = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &cls);
  check_jvmti_error(jvmti, err, "unable to retrieve declaring class");
  err = (*jvmti)->GetClassSignature(jvmti, cls, &cls_name, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve class signature");
  if (!filter(cls_name)) {
    free(cls_name);
    return;
  }
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  char *name;
  char *sig;
  err = (*jvmti)->GetMethodName(jvmti, method, &name, &sig, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve method name and/or signature");
  //const int len = strlen(cls_name) + strlen(name) + strlen(sig);
  //char *full_name = (char *) malloc(1 + len);
  //static char full_name[1024];
  //if (full_name == NULL) {
  //  return;
  //}
  //sprintf(full_name, "%s%s%s", cls_name, name, sig);
  //free(cls_name);
  //free(name);
  //free(sig);
  //free(full_name);
  int status;
  LIST_NODE *entry = hashtable_insert(gdata->methods_stat, cls_name, name, sig, &status);
  if (status == HASHTABLE_ERROR) {
    free(cls_name);
    free(name);
    free(sig);
    return;
  }
  if (status == HASHTABLE_EXISTING) {
    METH_INFO *mi = (METH_INFO *) hashtable_entry_get_value(entry);
    stack_push(&(mi->start_ts), &start);
    // we do not need these anymore
    free(cls_name);
    free(name);
    free(sig);
  } else {
    METH_INFO *mi = (METH_INFO *) malloc(sizeof(METH_INFO));
    if (mi == NULL) {
      free(cls_name);
      free(name);
      free(sig);
      return;
    }
    //memcpy(&(mi->start), &start, sizeof(struct timespec));
    mi->start_ts = stack_create();
    mi->max_us = 0;
    stack_push(&(mi->start_ts), &start);
    hashtable_entry_set_value(entry, mi); 
    // do not free cls_name, name, or sig as we are using them
  }
}

void JNICALL method_exit_handler(jvmtiEnv *jvmti,
				 JNIEnv *jni_env,
				 jthread thread,
				 jmethodID method,
				 jboolean was_popped_by_exception,
				 jvalue return_value)
{
  jvmtiError err;
  jclass cls;
  char *cls_name;
  err = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &cls);
  check_jvmti_error(jvmti, err, "unable to retrieve declaring class");
  err = (*jvmti)->GetClassSignature(jvmti, cls, &cls_name, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve class signature");
  if (!filter(cls_name)) {
    free(cls_name);
    return;
  }
  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  char *name;
  char *sig;
  err = (*jvmti)->GetMethodName(jvmti, method, &name, &sig, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve method name and/or signature");
  //static char full_name[1024];
  //const int len = strlen(cls_name) + strlen(name) + strlen(sig);
  //char *full_name = (char *) malloc(1 + len);
  //if (full_name == NULL) {
  //  return;
  //}
  //sprintf(full_name, "%s%s%s", cls_name, name, sig);
  //free(cls_name);
  //free(name);
  //free(sig);
  //free(full_name);
  //return;
  METH_INFO *mi = (METH_INFO *) hashtable_get(gdata->methods_stat, cls_name, name, sig);
  if (mi == NULL) {
    printf("fatal\n");
    free(cls_name);
    free(name);
    free(sig);
    return;
  }
  STACK_NODE *top = stack_pop(&(mi->start_ts));
  if (top != NULL) {
    struct timespec *start = stack_node_get_value(top);
    DWORD delta_us = (end.tv_sec - start->tv_sec) * 1000000 + (end.tv_nsec - start->tv_nsec) / 1000;
    mi->max_us = max(mi->max_us, delta_us);
    free(top);
  }
  free(cls_name);
  free(name);
  free(sig);
}

BOOL make_prefix(const char *raw_prefix)
{
  int len = raw_prefix == NULL ? 0 : strlen(raw_prefix);
  int i;
  gdata->prefix = malloc(2 + len);
  if (gdata->prefix == NULL) {
    return FALSE;
  }
  gdata->prefix_len = 1 + len;
  gdata->prefix[0] = 'L';
  if (len > 0) {
    for (i = 0; i < len; i++) {
      char ch = raw_prefix[i];
      if (ch == '.') {
	ch = '/';
      }
      gdata->prefix[1 + i] = ch;
    }
  }
  gdata->prefix[1 + len] = 0; // null character
  return TRUE;
}

void printer_visitor(const char *key_cls_name, const char *key_meth_name, const char *key_meth_sig, void *value)
{
  printf("%s%s%s ==> %ld us\n", key_cls_name, key_meth_name, key_meth_sig, ((METH_INFO *) value)->max_us);
}

void freeing_visitor(void *value)
{
  METH_INFO *mi = (METH_INFO *) value;
  STACK_NODE *top = NULL;
  do {
    top = stack_pop(&(mi->start_ts));
    if (top != NULL) {
      free(top);
    }
  } while (top != NULL);
  free(value);
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved)
{
  static GlobalAgentData data;
  jvmtiEnv *jvmti = NULL;
  jvmtiCapabilities capa;
  jvmtiEventCallbacks callbacks;
  jvmtiError error;
  jint res;

  printf("\nHello from JVMTI-Test!\noptions: %s\n", options);

  (void)memset(&data, 0, sizeof(data));

  gdata = &data;
  
  gdata->methods_stat = hashtable_create();

  if (!make_prefix(options)) {
    return JNI_ERR;
  }

  res = (*jvm)->GetEnv(jvm, (void **) &jvmti, JVMTI_VERSION_1_0);

  if (res != JNI_OK || jvmti == NULL) {
    printf("ERROR: Unable to access JVMTI Version 1 (0x%x),"
	   " is your J2SE a 1.5 or newer version?"
	   " JNIEnv's GetEnv() returned %d\n", JVMTI_VERSION_1, res);
    
  }

  gdata->jvmti = jvmti;

  (void)memset(&capa, 0, sizeof(jvmtiCapabilities));

  //capa.can_signal_thread = 1;
  //capa.can_get_owned_monitor_info = 1;
  capa.can_generate_method_entry_events = 1;
  capa.can_generate_method_exit_events = 1;
  //capa.can_generate_exception_events = 1;
  //capa.can_generate_vm_object_alloc_events = 1;
  //capa.can_tag_objects = 1;
  error = (*jvmti)->AddCapabilities(jvmti, &capa);
  check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");
  
  
  //-----
  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, (jthread) NULL);
  check_jvmti_error(jvmti, error, "Cannot set event notification");
  //-----
  
  //error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, (jthread)NULL);
  //check_jvmti_error(jvmti, error, "Cannot set event notification");
  //error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, (jthread)NULL);
  //check_jvmti_error(jvmti, error, "Cannot set event notification");
  /* error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL); */
  /* check_jvmti_error(jvmti, error, "Cannot set event notification"); */
  /* error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, (jthread)NULL); */
  /* check_jvmti_error(jvmti, error, "Cannot set event notification"); */

  //-----
  (void)memset(&callbacks, 0, sizeof(callbacks));
  //-----
                  
  //callbacks.VMInit = &callbackVMInit; /* JVMTI_EVENT_VM_INIT */
  //callbacks.VMDeath = &callbackVMDeath; /* JVMTI_EVENT_VM_DEATH */
  /* callbacks.Exception = &callbackException; /\* JVMTI_EVENT_EXCEPTION *\/ */
  /* callbacks.VMObjectAlloc = &callbackVMObjectAlloc; /\* JVMTI_EVENT_VM_OBJECT_ALLOC *\/ */

  //-----
  callbacks.MethodEntry = &method_entry_handler;
  callbacks.MethodExit = &method_exit_handler;
  error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint) sizeof(callbacks));
  check_jvmti_error(jvmti, error, "cannot set jvmti callbacks");
  //-----

  /* this creates a lock we should treat the code for our event handlers as critical sections */
  //error = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));
    
  return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm)
{
  hashtable_iterate(gdata->methods_stat, &printer_visitor);
  free(gdata->prefix);
  hashtable_free(gdata->methods_stat, &freeing_visitor);
}

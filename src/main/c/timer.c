#include <jvmti.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "hashtable.h"
#include "types.h"

typedef struct {
  struct timespec start;
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

BOOL filter(const char *cls_name)
{
  int lenpre = gdata->prefix_len;
  int lenstr = strlen(cls_name);
  return lenstr < lenpre ? FALSE : strncmp(gdata->prefix, cls_name, lenpre) == 0;
}

DWORD max(DWORD a, DWORD b) {
  return a >= b ? a : b;
}

BOOL check_jvmti_error(jvmtiEnv *jvmti, jvmtiError errnum, const char *str)
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
  char *name;
  char *sig;
  err = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &cls);
  check_jvmti_error(jvmti, err, "unable to retrieve declaring class");
  err = (*jvmti)->GetClassSignature(jvmti, cls, &cls_name, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve class signature");
  if (!filter(cls_name)) {
    return;
  }
  METH_INFO *mi = (METH_INFO *) malloc(sizeof(METH_INFO));
  if (mi == NULL) {
    return;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &(mi->start));
  err = (*jvmti)->GetMethodName(jvmti, method, &name, &sig, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve method name and/or signature");
  const int len = strlen(cls_name) + strlen(name) + strlen(sig);
  char *full_name = (char *) malloc(1 + len);
  if (full_name == NULL) {
    return;
  }
  sprintf(full_name, "%s%s%s", cls_name, name, sig);
  hashtable_insert(gdata->methods_stat, full_name, mi);
  // do not free full_name
}

void JNICALL method_exit_handler(jvmtiEnv *jvmti,
				 JNIEnv *jni_env,
				 jthread thread,
				 jmethodID method,
				 jboolean was_popped_by_exception,
				 jvalue return_value)
{
  struct timespec end;
  jvmtiError err;
  jclass cls;
  char *cls_name;
  char *name;
  char *sig;

  err = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &cls);
  check_jvmti_error(jvmti, err, "unable to retrieve declaring class");
  err = (*jvmti)->GetClassSignature(jvmti, cls, &cls_name, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve class signature");
  if (!filter(cls_name)) {
    return;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  err = (*jvmti)->GetMethodName(jvmti, method, &name, &sig, NULL);
  check_jvmti_error(jvmti, err, "unable to retrieve method name and/or signature");
  const int len = strlen(cls_name) + strlen(name) + strlen(sig);
  char *full_name = (char *) malloc(1 + len);
  if (full_name == NULL) {
    return;
  }
  sprintf(full_name, "%s%s%s", cls_name, name, sig);
  METH_INFO *mi = hashtable_get(gdata->methods_stat, full_name);
  if (mi == NULL) {
    free(full_name);
    return;
  }
  DWORD delta_us = (end.tv_sec - (mi->start).tv_sec) * 1000000 + (end.tv_nsec - (mi->start).tv_nsec) / 1000;
  mi->max_us = max(mi->max_us, delta_us);
  free(full_name);
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

void visitor(const char *key, void *value) {
  printf("%s ==> %ld us\n", key, ((METH_INFO *) value)->max_us);
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
  hashtable_iterate(gdata->methods_stat, &visitor);
  free(gdata->prefix);
  hashtable_free(gdata->methods_stat);
}

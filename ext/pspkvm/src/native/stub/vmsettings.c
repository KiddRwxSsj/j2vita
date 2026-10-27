/**
 * @file vmsettings.c
 *
 * PSPKVM VMSettings implementation--KNI backend
 */

#include <commonKNIMacros.h>
#include <midpEventUtil.h>
#include <midpString.h>
#include <midpMalloc.h>
#include <midpUtilKni.h>
#include <midpError.h>
#include <javacall_properties.h>

/**
 * IMPL_NOTE:(Doxy - Change this format) 
 *
 * FUNCTION:      get
 * CLASS:         com.pspkvm.system.VMSettings
 * TYPE:          virtual native function
 * OVERVIEW:      Get a configuration parameter
 * INTERFACE (operand stack manipulation):
 *   parameters:  k key for the param to retrn
 *   returns: the value of the the matching config param (or "" if null)
 */

KNIEXPORT KNI_RETURNTYPE_OBJECT
KNIDECL(com_pspkvm_system_VMSettings_get) {
  /* read-only on Vita: values come from the [internal] section of j2vita.ini */
  char key[128];
  char *value = NULL;
  jchar buf[128];
  int len, i;

  KNI_StartHandles(2);
  KNI_DeclareHandle(k);
  KNI_DeclareHandle(v);
  KNI_GetParameterAsObject(1, k);
  if (!KNI_IsNullHandle(k)) {
    len = KNI_GetStringLength(k);
    if (len > 127)
      len = 127;
    KNI_GetStringRegion(k, 0, len, buf);
    for (i = 0; i < len; i++)
      key[i] = (char)buf[i];
    key[len] = 0;
    if (javacall_get_property(key, JAVACALL_INTERNAL_PROPERTY, &value) == JAVACALL_OK && value)
      KNI_NewStringUTF(value, v);
  }
  KNI_EndHandlesAndReturnObject(v);
}
	
/**
 * IMPL_NOTE:(Doxy - Change this format) 
 *
 * FUNCTION:      commit
 * CLASS:         com.pspkvm.system.VMSettings
 * TYPE:          virtual native function
 * OVERVIEW:      Commit the configuration to storage
 */
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_VMSettings_commit) {
	KNI_ReturnVoid(); 
}

/**
 * IMPL_NOTE:(Doxy - Change this format) 
 *
 * FUNCTION:      set
 * CLASS:         com.pspkvm.system.VMSettings
 * TYPE:          virtual native function
 * OVERVIEW:      Set a configuration parameter
 * INTERFACE (operand stack manipulation):
 *   parameters:  k key for the param to get
 *   parameters:  v value for the param to get
 */
KNIEXPORT KNI_RETURNTYPE_VOID
KNIDECL(com_pspkvm_system_VMSettings_set) {
	KNI_ReturnVoid(); 
}


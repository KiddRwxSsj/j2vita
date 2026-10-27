/*
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "javacall_defs.h"
#include "javautil_string.h"
#include "javacall_memory.h"
#include "javacall_properties.h"
#include "javacall_logging.h"
#include "javacall_config_db.h"
#ifdef __vita__
#include <string.h>
#include "javacall_static_properties.h"
#endif

#ifdef __vita__
static unsigned short property_file_name[] = {
    'u','x','0',':','d','a','t','a','/','j','2','v','i','t','a','/',
    'j','2','v','i','t','a','.','i','n','i',0
};
#else
static unsigned short property_file_name[] = {'.','/','p','s','p','k','v','m','.','i','n','i',0};
#endif

static javacall_handle handle = NULL;
static int property_was_updated = 0;

static const char application_prefix[] = "application:";
static const char internal_prefix[] = "internal:";
static const char jsr75_prefix[] = "jsr75:";
static const char ams_prefix[] = "ams:";
static const char device_prefix[] = "device:";

#ifdef __vita__
/*
 * A writable j2vita.ini may come from an older PSP/Vita installation and
 * therefore contain no [application] section at all.  MIDP's Java-side
 * System.getProperty() ultimately reads this database, and the installer
 * requires at least microedition.profiles/configuration to be present.
 *
 * Seed the essential Java ME system properties into the in-memory database
 * after loading the INI. Existing user values always win.
 */
static void vita_seed_required_application_properties(void) {
    static const char *keys[] = {
        "microedition.configuration",
        "microedition.profiles",
        "microedition.platform",
        "microedition.locale",
        "javax.microedition.io.Connector.protocolpath",
        NULL
    };
    static const char *values[] = {
        "CLDC-1.1",
        "MIDP-2.1",
        "j2me",
        "en-US",
        "com.sun.midp.io",
        NULL
    };
    int i;

    for (i = 0; keys[i] != NULL; i++) {
        char *joined_key = javautil_string_strcat(application_prefix, keys[i]);
        char *current = NULL;

        if (joined_key == NULL) {
            continue;
        }

        if (JAVACALL_OK != javacall_configdb_getstring(handle, joined_key,
                                                        NULL, &current) ||
            current == NULL || current[0] == '\0') {
            if (JAVACALL_OK == javacall_configdb_setstr(handle, joined_key,
                                                        (char *)values[i])) {
                property_was_updated = 1;
            }
        }

        javacall_free(joined_key);
    }
}
#endif

/**
 * Initializes the configuration sub-system.
 *
 * @return <tt>JAVACALL_OK</tt> for success, JAVACALL_FAIL otherwise
 */
javacall_result javacall_initialize_configurations(void) {
    int file_name_len = sizeof(property_file_name)/sizeof(unsigned short);
    property_was_updated = 0;

    javacall_printf("javacall_configdb_load...\n");

    handle = javacall_configdb_load(property_file_name, file_name_len);
    if (handle == NULL) {
    	javacall_printf("load fail\n");
        return JAVACALL_FAIL;
    }
    javacall_printf("load ok\n");
#ifdef __vita__
    vita_seed_required_application_properties();
#endif
    return JAVACALL_OK;
}

/**
 * Finalize the configuration subsystem.
 */
void javacall_finalize_configurations(void) {
    int file_name_len = sizeof(property_file_name)/sizeof(unsigned short);
    if (property_was_updated != 0) {
#ifdef USE_PROPERTIES_FROM_FS
        javacall_configdb_dump_ini(handle, property_file_name, file_name_len);
#endif //USE_PROPERTIES_FROM_FS
    }
    javacall_configdb_free(handle);
    handle = NULL; 
}

/**
 * Gets the value of the specified property in the specified
 * property set.
 *
 * @param key The key to search for
 * @param type The property type 
 * @param result Where to put the result
 *
 * @return If found: <tt>JAVACALL_OK</tt>, otherwise
 *         <tt>JAVACALL_FAIL</tt>
 */
javacall_result javacall_get_property(const char* key, 
                                      javacall_property_type type,
                                      char** result){
    char* value = NULL;
    char* joined_key = NULL;

    if (JAVACALL_APPLICATION_PROPERTY == type) {
        joined_key = javautil_string_strcat(application_prefix, key);
    } else if (JAVACALL_INTERNAL_PROPERTY == type) {
        joined_key = javautil_string_strcat(internal_prefix, key);
    } else if (JAVACALL_JSR75_PROPERTY == type) {
        joined_key = javautil_string_strcat(jsr75_prefix, key);
    } else if (JAVACALL_AMS_PROPERTY == type) {
        joined_key = javautil_string_strcat(ams_prefix, key);
    } else if (JAVACALL_DEVICE_PROPERTY == type) {
        joined_key = javautil_string_strcat(device_prefix, key);
    }

    if (joined_key == NULL) {
        *result = NULL;
        javacall_print("fail1\n");
        return JAVACALL_FAIL;       
    }

    /*
     * javacall_configdb_getstring() returns JAVACALL_OK for a valid DB/key
     * even when the key is absent; in that case *result is the supplied
     * default (NULL here).  Treat NULL as a miss so Vita can fall back to
     * the compiled phoneME defaults below.
     */
    if (JAVACALL_OK == javacall_configdb_getstring(handle, joined_key, NULL, result)
        && *result != NULL) {
        javacall_free(joined_key);
        return JAVACALL_OK; 
    }

#ifdef __vita__
    /* Merge compiled phoneME defaults behind the writable Vita INI. */
    if (type == JAVACALL_APPLICATION_PROPERTY ||
        type == JAVACALL_INTERNAL_PROPERTY) {
        int section = (type == JAVACALL_APPLICATION_PROPERTY) ? 0 : 1;
        int i;

        for (i = 0; javacall_static_properties_keys[section][i] != NULL; i++) {
            if (strcmp(javacall_static_properties_keys[section][i], key) == 0) {
                *result = javacall_static_properties_values[section][i];
                javacall_free(joined_key);
                return JAVACALL_OK;
            }
        }
    }
#endif

    javacall_free(joined_key);
    *result = NULL;
    return JAVACALL_FAIL;
}


/**
 * Sets a property value matching the key in the specified
 * property set.
 *
 * @param key The key to set
 * @param value The value to set <tt>key</tt> to
 * @param replace_if_exist The value to decide if it's needed to replace
 * the existing value corresponding to the key if already defined
 * @param type The property type 
 * 
 * @return Upon success <tt>JAVACALL_OK</tt>, otherwise
 *         <tt>JAVACALL_FAIL</tt>
 */
javacall_result javacall_set_property(const char* key, 
                                      const char* value, 
                                      int replace_if_exist,
                                      javacall_property_type type) {
    char* joined_key = NULL;

    if (JAVACALL_APPLICATION_PROPERTY == type) {
        joined_key = javautil_string_strcat(application_prefix, key);
    } else if (JAVACALL_INTERNAL_PROPERTY == type) {
        joined_key = javautil_string_strcat(internal_prefix, key);
    } else if (JAVACALL_JSR75_PROPERTY == type) {
        joined_key = javautil_string_strcat(jsr75_prefix, key);
    } else if (JAVACALL_AMS_PROPERTY == type) {
        joined_key = javautil_string_strcat(ams_prefix, key);
    } else if (JAVACALL_DEVICE_PROPERTY == type) {
        joined_key = javautil_string_strcat(device_prefix, key);
    }

    if (joined_key == NULL) {
        return JAVACALL_FAIL;       
    }

    if (replace_if_exist == 0) { /* don't replace existing value */
        if (JAVACALL_OK == javacall_configdb_find_key(handle,joined_key)) {
            /* key exist, don't set */
            javacall_free(joined_key);
        } else {/* key doesn't exist, set it */
            javacall_configdb_setstr(handle, joined_key, (char *)value);
            property_was_updated=1;
            javacall_free(joined_key);
        }
    } else { /* replace existing value */
        javacall_configdb_setstr(handle, joined_key, (char *)value);
        javacall_free(joined_key);
        property_was_updated=1;
    }
    return JAVACALL_OK;
}



#ifdef __cplusplus
}
#endif


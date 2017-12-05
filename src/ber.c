#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "dllist.h"
#include "mibtree.h"
#include "ber.h"
#include "utils.h"

#define BER_ERRBUF_SIZE 1024
static char _ber_errbuf[BER_ERRBUF_SIZE];

#define BER_THROW_ERROR(errval, fmt, ...) \
    snprintf(_ber_errbuf, BER_ERRBUF_SIZE, fmt, ##__VA_ARGS__); \
    *errorptr = _ber_errbuf; \
    return errval;


static struct oid_type_data *fill_oid_type_data(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    struct oid_type_data *data = xmalloc(sizeof(struct oid_type_data));
    memset(data, 0, sizeof(struct oid_type_data));
    data->components = dllist_create();

    struct oid_type_data *component = NULL;

    if (syntax->is_explicit && syntax->parent) {
        component = fill_oid_type_data(syntax->parent, obj, errorptr);
        if (! component) {
            return NULL;
        }
        dllist_append(data->components, component);
    } else if (syntax->base_type == MIB_TYPE_SEQUENCE_OF) {
        if (! json_object_is_type(obj, json_type_array)) {
            BER_THROW_ERROR(NULL, "Expected JSON array, found: %s", json_object_to_json_string(obj));
        }
        array_list *list = json_object_get_array(obj);
        int len = array_list_length(list);
        for (int i = 0; i < len; i++) {
            json_object *component_obj = array_list_get_idx(list, i);
            component = fill_oid_type_data(syntax->u.seq_type, component_obj, errorptr);
            if (! component) {
                return NULL;
            }
            dllist_append(data->components, component);
        }
    } else if (syntax->base_type == MIB_TYPE_SEQUENCE) {
        if (! json_object_is_type(obj, json_type_object)) {
            BER_THROW_ERROR(NULL, "Expected JSON object, found: %s", json_object_to_json_string(obj));
        }
        struct object_type_syntax **itemptr;
        dllist_foreach(itemptr, syntax->u.components) {
            json_object *component_obj;
            if (json_object_object_get_ex(obj, (*itemptr)->name, &component_obj)) {
                component = fill_oid_type_data(*itemptr, component_obj, errorptr);
                if (! component) {
                    return NULL;
                }
                dllist_append(data->components, component);
            } else {
                BER_THROW_ERROR(NULL, "JSON key %s was not found", (*itemptr)->name);
            }
        }
    } else if (syntax->base_type == MIB_TYPE_CHOICE) {
        if (! json_object_is_type(obj, json_type_object)) {
            BER_THROW_ERROR(NULL, "Expected JSON object, found: %s", json_object_to_json_string(obj));
        }
        if (json_object_object_length(obj) != 1) {
            BER_THROW_ERROR(NULL, "JSON object for CHOICE should have exactly one key");
        }
        struct object_type_syntax **itemptr;
        dllist_foreach(itemptr, syntax->u.components) {
            json_object *component_obj;
            if (json_object_object_get_ex(obj, (*itemptr)->name, &component_obj)) {
                component = fill_oid_type_data(*itemptr, component_obj, errorptr);
                break;
            }
        }
        if (! component) {
            BER_THROW_ERROR(NULL, "None of the CHOICE elements were specified in JSON");
        }
        dllist_append(data->components, component);
    } else if (syntax->base_type == MIB_TYPE_NULL) {
        
    }
    return data;
}

unsigned char *ber_encode(struct oid *oid, char *value, char **errorptr)
{
    if (! oid->type) {
        *errorptr = "Given OID does not have a type";
        return NULL;
    }
    json_object *obj = json_tokener_parse(value);
    if (! obj) {
        *errorptr = "Invalid JSON";
        return NULL;
    }
    struct oid_type_data *data = fill_oid_type_data(oid->type->syntax, obj, errorptr);
    if (! data) {
        return NULL;
    }
    return NULL;
    
}

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
    return errval


static struct oid_type_data *encode_tag(struct oid_type_data *data);
struct oid_type_data *encode_length(struct oid_type_data *data);

static struct oid_type_data *encode_null(struct object_type_syntax *syntax, json_object *obj, char **errorptr);
struct oid_type_data *encode_integer(struct object_type_syntax *syntax, json_object *obj, char **errorptr);
struct oid_type_data *encode_obj_id(struct object_type_syntax *syntax, json_object *obj, char **errorptr);
struct oid_type_data *encode_octet_string(struct object_type_syntax *syntax, json_object *obj, char **errorptr);

static struct dllist *encode_choice(struct object_type_syntax *syntax, json_object *obj, char **errorptr) {return NULL;}
static struct dllist *encode_sequence(struct object_type_syntax *syntax, json_object *obj, char **errorptr) {return NULL;}
static struct dllist *encode_seq_of(struct object_type_syntax *syntax, json_object *obj, char **errorptr) {return NULL;}


static struct oid_type_data *encode_oid_type(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    struct oid_type_data *data;

    if (! syntax->parent) {
        switch (syntax->base_type) {
            case MIB_TYPE_NULL:
                data = encode_null(syntax, obj, errorptr);
            case MIB_TYPE_INTEGER:
                data = encode_integer(syntax, obj, errorptr);
            case MIB_TYPE_OBJECT_IDENTIFIER:
                data = encode_obj_id(syntax, obj, errorptr);
            case MIB_TYPE_OCTET_STRING:
                data = encode_octet_string(syntax, obj, errorptr);
            default:
                BER_THROW_ERROR(NULL, "Type %s is incomplete", syntax->name);
        }
        return encode_tag(encode_length(data));
    }

    struct dllist *components = NULL;
    struct oid_type_data *component;
    int is_constructed = 0;

    if (! syntax->parent->parent && ! IS_PRIMITIVE(syntax)) {
        is_constructed = 1;
        switch (syntax->base_type) {
            case MIB_TYPE_CHOICE:
                components = encode_choice(syntax, obj, errorptr);
                break;
            case MIB_TYPE_SEQUENCE:
                components = encode_sequence(syntax, obj, errorptr);
                break;
            case MIB_TYPE_SEQUENCE_OF:
                components = encode_seq_of(syntax, obj, errorptr);
                break;
            default:
                snprintf(_ber_errbuf, BER_ERRBUF_SIZE, "Unexpected type: %d", syntax->base_type);
        }
        if (! components) {
            return NULL;
        }
    } else {
        component = encode_oid_type(syntax->parent, obj, errorptr);
        if (! component) {
            return NULL;
        }
        components = dllist_create();
        dllist_append(components, component);
        free(component);
    }

    is_constructed = is_constructed || syntax->is_explicit;
    if (is_constructed) {
        unsigned long long length = 0;
        dllist_foreach(component, components) {
            length += component->taglen + component->lenlen + component->datalen;
        }
        data = xmalloc(sizeof(struct oid_type_data));
        memset(data, 0, sizeof(struct oid_type_data));
        data->syntax = syntax;
        data->datalen = length;
        data->databuf = (unsigned char *)xmalloc(length);
        unsigned long long pos = 0;
        dllist_foreach(component, components) {
            memcpy(data->databuf + pos, component->tagbuf, component->taglen);
            pos += component->taglen;
            memcpy(data->databuf + pos, component->lenbuf, component->lenlen);
            pos += component->lenlen;
            if (component->databuf) {
                memcpy(data->databuf + pos, component->databuf, component->datalen);
                pos += component->datalen;
                free(component->databuf);
            }
            free(component->tagbuf);
            free(component->lenbuf);
        }
    } else {
        dllist_foreach(component, components) {
            data = xmalloc(sizeof(struct oid_type_data));
            memcpy(data, component, sizeof(struct oid_type_data));
            data->syntax = syntax;
            break;
        }
    }
    dllist_destroy(components);
    return encode_tag(encode_length(data));
}

static struct oid_type_data *encode_tag(struct oid_type_data *data)
{
    if (data->tagbuf) {
        free(data->tagbuf);
    }
    data->taglen = 1;
    if (data->syntax->tag >= 31) {
        unsigned long long tag = data->syntax->tag;
        while (tag) {
            data->taglen++;
            tag >>= 7;
        }
    }
    data->tagbuf = (unsigned char *)xmalloc(data->taglen);
    int is_constructed = data->syntax->is_explicit || (! IS_PRIMITIVE(data->syntax));
    data->tagbuf[0] = (data->syntax->visibility << 6) | (is_constructed << 5) | (data->syntax->tag & 0x1f);
    if (data->syntax->tag >= 31) {
        data->tagbuf[0] |= 0x1f;
        unsigned long long tag = data->syntax->tag;
        for (int i = data->taglen - 1; i > 0; i--) {
            data->tagbuf[i] = (tag & 0x7f) | 0x80;
            tag >>= 7;
        }
        data->tagbuf[data->taglen - 1] &= ~0x80;
    }
    return data;
}

struct oid_type_data *encode_length(struct oid_type_data *data)
{
    if (data->lenbuf) {
        free(data->lenbuf);
    }
    data->lenlen = 1;
    unsigned long long datalen = data->datalen;
    if (datalen > 127) {
        while (datalen) {
            data->lenlen++;
            datalen >>= 8;
        }
    }
    data->lenbuf = (unsigned char *)xmalloc(data->lenlen);
    datalen = data->datalen;
    if (datalen <= 127) {
        data->lenbuf[0] = (unsigned char)datalen;
    } else {
        data->lenbuf[0] = 0x80 | ((data->lenlen - 1) & 0x7f);
        for (int i = data->lenlen - 1; i > 0; i--) {
            data->lenbuf[i] = datalen & 0xff;
            datalen >>= 8;
        }
    }
    return data;
}

static struct oid_type_data *encode_null(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    struct oid_type_data *data = (struct oid_type_data *)xmalloc(sizeof(struct oid_type_data));
    memset(data, 0, sizeof(struct oid_type_data));
    return data;
}

struct oid_type_data *encode_integer(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    if (! json_object_is_type(obj, json_type_int)) {
        BER_THROW_ERROR(NULL, "JSON for type %s must be an integer, not %s", syntax->name, json_object_to_json_string(obj));
    }
    long long value = json_object_get_int64(obj);
    long long v = value;
    struct oid_type_data *data = (struct oid_type_data *)xmalloc(sizeof(struct oid_type_data));
    memset(data, 0, sizeof(struct oid_type_data));
    if (v >= 0) {
        while (v) {
            data->datalen++;
            if ((v >> 8 == 0) && (v & 0x80)) {
                data->datalen++;
            }
            v >>= 8;
        }
    } else {
        if (v == -1) {
            data->datalen = 1;
        } else {
            while (v < -1) {
                data->datalen++;
                if ((v >> 8) == -1 && !(v & 0x80)) {
                    data->datalen++;
                }
                v >>= 8;
            }
        }
    }
    data->databuf = (unsigned char *)xmalloc(data->datalen);
    for (int i = data->datalen - 1; i >= 0; i--) {
        data->databuf[i] = (unsigned char)(value & 0xff);
        value >>= 8;
    }
    return data;
}

struct oid_type_data *encode_obj_id(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    struct dllist *values;
    int value;
    const char *string = json_object_get_string(obj);
    if (json_object_is_type(obj, json_type_string)) {
        char *dot = strchr(string, '.');
        if (! dot) {
            BER_THROW_ERROR(NULL, "Invalid OBJECT IDENTIFIER: %s", string);
        }
        value = atoi(string);
        if (value > 2 || value < 0) {
            BER_THROW_ERROR(NULL, "Invalid OBJECT IDENTIFIER: %s", string);
        }
        value *= 40;
        value += atoi(dot + 1);
        values = dllist_create();
        dllist_append(values, &value);
        while ((dot = strchr(dot + 1, '.'))) {
            value = atoi(dot + 1);
            dllist_append(values, &value);
        }
    } else if (json_object_is_type(obj, json_type_array)) {
        array_list *list = json_object_get_array(obj);
        int len = array_list_length(list);
        if (len < 2) {
            BER_THROW_ERROR(NULL, "Invalid OBJECT IDENTIFIER: %s", string);
        }
        json_object *component = array_list_get_idx(list, 0);
        value = json_object_get_int(component);
        if (value > 2 || value < 0) {
            BER_THROW_ERROR(NULL, "Invalid OBJECT IDENTIFIER: %s", string);
        }
        value *= 40;
        component = array_list_get_idx(list, 1);
        value += json_object_get_int(component);
        values = dllist_create();
        dllist_append(values, &value);
        for (int i = 2; i < len; i++) {
            component = array_list_get_idx(list, i);
            if (json_object_is_type(component, json_type_int)) {
                value = json_object_get_int(component);
                dllist_append(values, &value);
            }
        }
    } else {
        BER_THROW_ERROR(NULL, "Invalid OBJECT IDENTIFIER: %s", string);
    }
    struct oid_type_data *data = (struct oid_type_data *)xmalloc(sizeof(struct oid_type_data));
    memset(data, 0, sizeof(struct oid_type_data));
    int *valptr;
    dllist_foreach(valptr, values) {
        value = *valptr;
        while (value) {
            data->datalen++;
            value >>= 7;
        }
    }
    data->databuf = (unsigned char *)xmalloc(data->datalen);
    int pos = 0;
    dllist_foreach(valptr, values) {
        value = *valptr;
        int vallen = 0;
        while (value) {
            vallen++;
            value >>= 7;
        }
        value = *valptr;
        for (int i = vallen - 1; i >= 0; i--) {
            data->databuf[pos + i] = (value & 0x7f) | 0x80;
            value >>= 7;
        }
        data->databuf[pos + vallen - 1] &= ~0x80;
        pos += vallen;
    }
    return data;
}

struct oid_type_data *encode_octet_string(struct object_type_syntax *syntax, json_object *obj, char **errorptr)
{
    const char *string = json_object_get_string(obj);
    struct oid_type_data *data = (struct oid_type_data *)xmalloc(sizeof(struct oid_type_data));
    memset(data, 0, sizeof(struct oid_type_data));
    if (json_object_is_type(obj, json_type_string)) {
        data->databuf = (unsigned char *)xstrdup((char *)string);
        data->datalen = strlen((char *)string);
    } else if (json_object_is_type(obj, json_type_array)) {
        array_list *list = json_object_get_array(obj);
        int len = array_list_length(list);
        data->datalen = len;
        data->databuf = (unsigned char *)xmalloc(len);
        for (int i = 0; i < len; i++) {
            json_object *component = array_list_get_idx(list, i);
            if (! json_object_is_type(component, json_type_int)) {
                free(data->databuf);
                free(data);
                BER_THROW_ERROR(NULL, "Invalid JSON object for OCTET STRING: unexpected array element %s", json_object_get_string(component));
            }
            long long value = json_object_get_int64(component);
            if (value > 255 || value < 0) {
                fprintf(stderr, "Warning: value %lld is out of range (0.255), it will be truncated to %d\n", value, (value & 0xff));
            }
            data->databuf[i] = value & 0xff;
        }
    } else {
        free(data);
        BER_THROW_ERROR(NULL, "Invalid JSON object for OCTET STRING, string or array of integers expected, found: %s", string);
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
    struct oid_type_data *data = encode_oid_type(oid->type->syntax, obj, errorptr);
    if (! data) {
        return NULL;
    }
    return NULL;

}

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SPDX-License-Identifier:  LGPL-2.0
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <ctype.h>
#include <string.h>

#include <glib.h>

#include <ModemManager.h>
#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>
#include <gdbm.h>
#include "json-glib/json-glib.h"
#include "mm-common-helpers.h"
#include "mm-sms-storage.h"

static gchar *dbname = "/etc/ModemManager/smsDb.json";

/** Store message in local storage in "pdu,state" format*/
gboolean
mm_sms_storage_store_message(gchar *pdu, MMSmsState state, guint32 *idx, GError **error)
{
    JsonParser *parser = json_parser_new ();
    GError *err = NULL;
    guint msg_ref = 0;
    JsonNode *root;
    JsonObject *root_object;
    JsonReader *reader = NULL;
    gboolean no_data = TRUE;
    if (!json_parser_load_from_file(parser, dbname, &err)) {
        if (err->code != G_FILE_ERROR_NOENT) {
            g_object_unref (parser);
            g_set_error (err,
                         MM_CORE_ERROR,
                         MM_CORE_ERROR_FAILED,
                         "Unable to open JSON file");
            *error = err;
            return FALSE;
        }
        g_error_free(err);
        err = NULL;
    } else {
        root = json_parser_get_root (parser);
        if( JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
            no_data = FALSE;
            reader = json_reader_new (root);
            root_object = json_node_get_object(root);
            if(json_reader_read_member (reader, "msg_ref")) {
              msg_ref = json_reader_get_int_value (reader);
            }
            json_reader_end_member (reader);
        }
    }

    if (no_data) {
        root = json_node_new(JSON_NODE_OBJECT);
        root_object = json_object_new();
    }
    msg_ref++;

    json_object_set_int_member(root_object, "msg_ref", msg_ref);
    JsonObject *object = json_object_new();
    json_object_set_string_member(object, "pdu", pdu);
    json_object_set_int_member(object, "state", state);
    json_object_set_object_member (root_object, g_strdup_printf ("%u", msg_ref), object);
    json_node_take_object(root, root_object);

    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, root);

    // Write the data to JSON
    if (!json_generator_to_file(generator, dbname, err)) {
        g_set_error (err,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "DB write operation failed");
        *error = err;
        g_object_unref (parser);
        if (reader)
            g_object_unref (reader);
        if (no_data)
            json_node_unref (root);
        return FALSE;
    }

    g_object_unref (parser);
    if (reader)
        g_object_unref (reader);
    if (no_data)
        json_node_unref (root);
    *idx = msg_ref;
    return TRUE;
}

/* Delete a message of given index from the storage */
gboolean
mm_sms_storage_delete_message (guint index, GError **error)
{

    JsonParser *parser = json_parser_new ();
    JsonNode *root;
    JsonObject *root_object;
    JsonReader *reader;

    if (!json_parser_load_from_file(parser, dbname, error)) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Unable to open JSON file");
        g_object_unref (parser);
        return FALSE;
    }
    root = json_parser_get_root (parser);
    if( JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
        reader = json_reader_new (root);
        root_object = json_node_get_object(root);
    } else {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "No messages present in the DB");
        g_object_unref (parser);
        return FALSE;
    }

    //Delete message
    json_object_remove_member (root_object, g_strdup_printf ("%u", index));
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, root);

    // Write the data to JSON
    if (!json_generator_to_file(generator, dbname, error)) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "DB write operation failed");
        return FALSE;
    }

    g_object_unref (parser);
    g_object_unref (reader);
    return TRUE;

}

/* Read a message of given idex from stoarge */
gboolean
mm_sms_storage_read_message (guint index, gchar **pdu, MMSmsState *state, GError **error)
{
    JsonParser *parser = json_parser_new ();
    JsonNode *root;
    JsonObject *root_object;
    JsonReader *reader;

    if (!json_parser_load_from_file(parser, dbname, error)) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Unable to open JSON file");
        g_object_unref (parser);
        return FALSE;
    }
    root = json_parser_get_root (parser);
    if( JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
        reader = json_reader_new (root);
        root_object = json_node_get_object(root);
    } else {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "No messages present in the DB");
        g_object_unref (parser);
        return FALSE;
    }
    JsonNode *node = json_object_get_member (root_object, g_strdup_printf ("%u", index));
    if (node) {
        JsonObject *obj = json_node_get_object (node);
        *pdu = g_strdup(json_object_get_string_member (obj, "pdu"));
        *state = json_object_get_int_member (obj, "state");
    }
    g_object_unref (parser);
    g_object_unref (reader);
    return TRUE;
}

/* Return all valid indexces */
GList *
mm_sms_storage_read_all_indexces (GError **error)
{
    JsonParser *parser = json_parser_new ();
    JsonNode *root;
    JsonObject *root_object;
    JsonReader *reader;
    GList *list = NULL;
    gulong msg_ref;
    if (!json_parser_load_from_file(parser, dbname, error)) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "Unable to open JSON file");
        g_object_unref (parser);
        return NULL;
    }
    root = json_parser_get_root (parser);
    if( JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
        reader = json_reader_new (root);
        root_object = json_node_get_object(root);
    } else {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_FAILED,
                     "No messages present in the DB");
        g_object_unref (parser);
        return NULL;
    }
    GList *memberNames = json_object_get_members(root_object);
    GList *iter;
    for (iter = memberNames; iter != NULL; iter = iter->next) {
        const gchar *memberName = (const gchar *)iter->data;
        msg_ref = g_ascii_strtoll (memberName, NULL, 10);
        list = g_list_append (list, msg_ref);
    }
    g_object_unref (parser);
    g_object_unref (reader);
    return list;
}

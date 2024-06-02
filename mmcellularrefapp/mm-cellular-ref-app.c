/*
* SPDX-License-Identifier:  LGPL-2.0
* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
*/

#include <glib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <gio/gio.h>
#include <libmm-glib.h>
#include <glib.h>
#include <glib-object.h>
#include <locale.h>
#define MM_TEST_SUCCESS (0)
#define MM_TEST_FAILURE (-1)

/* Global Definitions */
GDBusConnection *connection;


GMutex manager_mutex;
typedef enum {
    BEARER_IP_FAMILY_NONE    = 0,
    BEARER_IP_FAMILY_IPV4    = 1 << 0,
    BEARER_IP_FAMILY_IPV6    = 1 << 1,
    BEARER_IP_FAMILY_IPV4V6  = 1 << 2,
    BEARER_IP_FAMILY_ANY     = 0xFFFFFFFF
} BearerIpFamily;

typedef struct {
    MMObject *object;
    MMModem *modem;
    MMModem3gpp *modem_3gpp;
    MMModemSignal *modem_signal;
    MMBearer *bearer;
} Context;

static int
connect_process_reply (gboolean      result,
                       const GError *error)
{
    int res = MM_TEST_FAILURE;
    if (!result) {
        g_printerr ("error: couldn't connect the bearer: '%s'\n",
                    error ? error->message : "unknown error");
    return res;
    }
    res = MM_TEST_SUCCESS;
    g_print ("successfully connected the bearer\n");
    return res;
}
static int
disconnect_process_reply (gboolean      result,
                       const GError *error)
{
    int res = MM_TEST_FAILURE;
    if (!result) {
        g_printerr ("error: couldn't disconnect the bearer: '%s'\n",
                    error ? error->message : "unknown error");
        return res;
    }
    res = MM_TEST_SUCCESS;
    g_print ("successfully disconnected the bearer\n");
    return res;
}
static void
context_free (Context *ctx)
{
    if (!ctx)
        return;

    if (ctx->modem)
        g_object_unref(ctx->modem);

    if (ctx->modem_3gpp)
        g_object_unref(ctx->modem_3gpp);

    if (ctx->bearer)
        g_object_unref(ctx->bearer);

    if (ctx->modem_signal)
        g_object_unref(ctx->modem_signal);

    if (ctx->object)
        g_object_unref(ctx->object);

    g_free (ctx);
}

static MMBearer *
search_bearer (GList       *bearer_list,
                const gchar *bearer_path)
{
    GList *list = NULL;
    MMBearer *bearer = NULL;

    for (list = bearer_list; list; list = g_list_next (list)) {
        bearer = MM_BEARER (list->data);
        if (!g_strcmp0 (mm_bearer_get_path (bearer), bearer_path)) {
            return g_object_ref (bearer);
        }
        if(!bearer_path)
        {
            if(mm_bearer_get_profile_id(bearer) == 1) {
                g_print("found bearer at path %s profile ID %d\n", mm_bearer_get_path(bearer),
                        mm_bearer_get_profile_id(bearer));
                return g_object_ref(bearer);
            }
        }
    }
    return NULL;
}


static MMBearer* get_bearer(MMManager *manager, char *bearer_path)
{
    GList *list, *modems = NULL, *bearers = NULL;
    MMBearer *found = NULL;
    GError *error = NULL;
    MMObject *object;
    MMModem *modem = NULL;
    g_mutex_lock (&manager_mutex);
    modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (manager));
    g_mutex_unlock (&manager_mutex);
    if (!modems) {
        g_print ("%s- error: couldn't find modem\n", __FUNCTION__);
        return NULL;
    }
    for (list = modems; !found && list; list = g_list_next (list)) {
        object = MM_OBJECT (list->data);
        modem = mm_object_get_modem (object);
        bearers = mm_modem_list_bearers_sync (modem, NULL, &error);
        if (!bearers) {
            g_print ("%s - error: couldn't list bearer '%s'\n",
                        __FUNCTION__, error ? error->message : "unknown error");
            if(error)
                g_error_free (error);
        }
        else
        {
            found = search_bearer (bearers, bearer_path);
            g_list_free_full (bearers, g_object_unref);
        }
        g_clear_object (&modem);
    }
    g_list_free_full (modems, g_object_unref);

    if (!found) {
        g_print ("%s - error: couldn't find bearer\n", __FUNCTION__);
        return NULL;
    }

    return found;
}

MMManager *check_ModemManager (GDBusConnection *connection)
{
    MMManager *manager = NULL;
    gchar *name_owner;
    GError *error = NULL;

    manager = mm_manager_new_sync (connection,
                               G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
                               NULL,
                               &error);

    if (!manager) {
       g_print ("error: couldn't create manager: %s\n",
                    error ? error->message : "unknown error");
       return NULL;
    }

    return manager;
}
static MMObject *
get_object (MMManager *manager)
{
    GList *list, *modems = NULL;
    MMObject *found = NULL, *object = NULL;

    if( !manager )
        return NULL;

    modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (manager));

    if (!modems)
    {
        g_print ("ModemManger is not ready\n");
    }
    else
    {
        for (list = modems; list; list = g_list_next (list)) {
            object = MM_OBJECT (list->data);
            if( mm_object_get_path(object) != NULL )
            {
                found = g_object_ref (object);
                break;
            }
        }
        g_list_free_full (modems, g_object_unref);
    }
    return found;
}

char* mm_test_bearer_init(MMManager *manager, bool create)
{
    Context *ctx;
    GError *error = NULL;
    MMBearerProperties *properties = NULL;
    MMBearer *bearer = NULL;
    char *bearPath = NULL;
    int res;
    BearerIpFamily ipFamily = BEARER_IP_FAMILY_IPV4V6;
    ctx = g_new0 (Context, 1);
    ctx->object = get_object(manager);
    ctx->modem = mm_object_get_modem (ctx->object);
    if(create == TRUE)
    {
        properties = mm_bearer_properties_new ();
        if( properties )
        {
            mm_bearer_properties_set_profile_id (properties, 1);
            ipFamily = BEARER_IP_FAMILY_IPV4;
            mm_bearer_properties_set_ip_type(properties, ipFamily);
            bearer = mm_modem_create_bearer_sync (ctx->modem, properties, NULL, &error);
             if( bearer == NULL )
            {
                g_print ("Couldn't create new bearer: '%s'\n",
                          error ? error->message : "unknown error");
            }
            else
            {
                bearPath = mm_bearer_get_path (bearer);
            }
            g_clear_object (&properties);
        }
    }
    else
    {
        bearer = get_bearer(manager, NULL);
        if(bearer)
        {
            bearPath = mm_bearer_get_path (bearer);
            res = mm_modem_delete_bearer_sync (ctx->modem, bearPath, NULL, &error);
            bearPath = NULL;
        }
    }

    context_free(ctx);
    return bearPath;

}
static int mm_invoke_connect_script(MMBearer *bearer)
{
  pid_t pid, wait_pid;
  int argc = 0;
  int cmd_rc, i;
  const char* argv[20];
  MMBearerProperties *properties = NULL;
  g_autoptr(MMBearerIpConfig)    ipv4_config = NULL;
  properties       = mm_bearer_get_properties (bearer);
  ipv4_config      = mm_bearer_get_ipv4_config (bearer);
  const gchar  *address = NULL;
  char* interface = NULL;
  guint mtu;
  gchar* prefix = NULL;
  address = mm_bearer_ip_config_get_address (ipv4_config);
  prefix  = g_strdup_printf ("%u", mm_bearer_ip_config_get_prefix (ipv4_config));
  interface = mm_bearer_get_interface (bearer);
  g_print("Bring_up_call.sh arguments %s %s %s\n", address, prefix, interface);
  if(!interface)
  {
     g_print("Invalid Interface name\n");
     return MM_TEST_FAILURE;
  }
  pid = fork();
  if (pid < 0) {
    /* Failed to fork */
    g_print("Fork Failed\n");
    return MM_TEST_FAILURE;
  }

  else if (pid == 0)
  {
    /* We are the child process*/
    argc = 0;
    g_print("Bring_up_call.sh arguments %s %s %s\n", address, prefix, interface);
    /* NULL terminate argument vector */
    argv[0] = "/usr/bin/bring_up_call.sh";
    argv[1] = address;
    argv[2] = prefix;
    argv[3] = interface;
    argv[4] = '\0';
    for(i = 0;i < 4; i++)
    {
    g_print("arg[%d] = %s\n", i, argv[i]);
    }
    if (execvp(argv[0], argv) < 0)
    {
      g_print("exec failed\n");
      _exit(-1);
    }
  }
  else
  {
     if ((wait_pid = waitpid(pid, &cmd_rc, 0) == pid) && WIFEXITED(cmd_rc)) {
        cmd_rc = WEXITSTATUS(cmd_rc);
        g_print("%s(): [cpid:%d] Process exited normally with rc=%d\n",
            __func__, pid, cmd_rc);
     }
  }
  return MM_TEST_SUCCESS;
}
MMManager *mm_initialize(void)
{

    GOptionContext *context;
    GError *error = NULL;
    MMManager *manager = NULL;
    GList *l;
    setlocale (LC_ALL, "");
    /* Setup dbus connection to use */
    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

    if(connection)
        manager = check_ModemManager(connection);
    return manager;
}

int mm_test_bearer_action(char* bearer_path, bool connect)
{

    Context *ctx;
    int res = MM_TEST_SUCCESS;
    MMManager *manager = NULL;
    GError *error = NULL;
    char* bearPath = NULL;
    manager = mm_initialize();
    ctx = g_new0 (Context, 1);
    ctx->object = get_object(manager);
    ctx->modem = mm_object_get_modem (ctx->object);
    if(connect == TRUE)
    {
        ctx->bearer = get_bearer (manager, bearer_path);
        if( ctx->bearer )
        {
            res = mm_bearer_connect_sync(ctx->bearer, NULL, &error);
            res = connect_process_reply(res, error);
        }
    }
    else
    {
        ctx->bearer = get_bearer(manager, bearer_path);
        res = mm_bearer_disconnect_sync(ctx->bearer, NULL, &error);
        res = disconnect_process_reply(res, error);
        if(res == 0) {
            bearPath = mm_bearer_get_path(ctx->bearer);
            res = mm_modem_delete_bearer_sync (ctx->modem, bearPath, NULL, &error);
        }
    }

    context_free(ctx);
    return res;

}
void process_input(char input)
{
    MMManager *manager;
    char *bearPath = NULL;
    char bearPathStr[100];
    MMBearer *bearer = NULL;
    int res = -1;
    switch(input)
    {
        case 'c':
            manager = mm_initialize();
            bearPath = mm_test_bearer_init(manager, TRUE);
            res = mm_test_bearer_action(bearPath, TRUE);
            if(res)
            {
                break;
            }
            manager = mm_initialize();
            bearer = get_bearer (manager, bearPath);
<<<<<<< HEAD
            res = mm_invoke_connect_script(bearer);
            if(!res) {
                g_print("Bearer connected on bearPath %s\n", bearPath);
=======
            if (bearer) {
                res = mm_invoke_connect_script(bearer);
                if(!res) {
                    g_print("Bearer connected on bearPath %s\n", bearPath);
                }
>>>>>>> 3a6d486cec179ec91c21863ea9c535a2089d56aa
            }
            break;
        case 'd':
            res = mm_test_bearer_action(NULL, FALSE);
            break;
        default:
            break;
    }
    return;
}
int main(int argc, char* argv[])
{
    if(argc < 1 || !argv[1])
    {
        g_print("Please input either 'c' or 'd' for connect/disconnect");
        exit(1);
    }
    process_input(argv[1][0]);
    return 0;
}

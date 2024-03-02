/*
 * SPDX-License-Identifier: LGPL-2.0
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <gtk/gtk.h>
#include <libmm-glib.h>

typedef struct {
  GDBusConnection *connection;
  MMManager *manager;
  MMModem *modem;
  MMModemMessaging *modem_messaging;
  MMModemMode allowed_modes;
} Context;

static Context *ctx;

struct smsEditor {
  GtkWidget *window;
  GtkWidget *number_view;
  GtkWidget *text_view;
  GtkWidget *grid;
  gboolean shift;
};

// To keep track of focused text view
static gboolean is_number_view_active;

// Display size
static int screen_width;
static int screen_height;

void display_virtual_key_board(struct smsEditor *editor);

// Callback for virtual keyboard key clicks
void button_clicked(GtkWidget *widget, gpointer data) {
  const gchar *label = gtk_button_get_label(GTK_BUTTON(widget));
  struct smsEditor *editor = data;

  GtkTextView *text_view;

  if (is_number_view_active) {
    text_view = GTK_TEXT_VIEW (editor->number_view);
  } else {
    text_view = GTK_TEXT_VIEW (editor->text_view);
  }

  // Get the text buffer from the text view
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

  // Get the current text in the buffer
  GtkTextIter iter;
  gtk_text_buffer_get_end_iter(buffer, &iter);

  if (g_strcmp0(label, "Shift") == 0) {
    editor->shift = !editor->shift;
    display_virtual_key_board(editor);
    return;
  } else if (g_strcmp0(label, "<--") == 0) {
    // Delete last character in the buffer
    gtk_text_buffer_backspace(buffer, &iter, 0, 1);
    return;
  } else if (g_strcmp0(label, "Enter") == 0) {
    // Insert \n into the buffer
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
    return;
  } else if (g_strcmp0(label, "Space") == 0) {
    // Insert space into the buffer
    gtk_text_buffer_insert(buffer, &iter, " ", -1);
    return;
  }

  // Insert the key into the buffer
  gtk_text_buffer_insert(buffer, &iter, label, -1);
}

MMSmsProperties *build_sms_properties_from_input(const gchar *number, const gchar *text) {
  MMSmsProperties *properties = mm_sms_properties_new();

  if (!properties) {
     return NULL;
  }

  if (number) {
    mm_sms_properties_set_number(properties, number);
  }

  if (text) {
    mm_sms_properties_set_text(properties, text);
  }

  return properties;
}

// Callback for send sms button click
void send_sms_button_clicked(GtkWidget *button, gpointer data) {
  struct smsEditor *editor = data;
  // Get text buffer
  GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor->text_view));

  // Get start and end of the iterator
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer, &start, &end);

  // Get text from buffer
  gchar *text = gtk_text_buffer_get_text(text_buffer, &start, &end, FALSE);
  printf("text view %s\n", text);

  // Get number from buffer
  GtkTextBuffer *number_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor->number_view));
  GtkTextIter start1, end1;
  gtk_text_buffer_get_bounds(number_buffer, &start1, &end1);
  gchar *number = gtk_text_buffer_get_text(number_buffer, &start1, &end1, FALSE);
  printf("number view %s\n", number);

  GError *error = NULL;
  MMSmsProperties *properties;
  properties = build_sms_properties_from_input(number, text);

  if (!properties) {
    g_printerr ("error: SmsProperties are invalid\n");
    return;
  }

  // Create SMS
  MMSms *sms = mm_modem_messaging_create_sync(ctx->modem_messaging, properties, NULL, &error);
  if (!sms) {
    g_printerr("Not able to create SMS : %s\n", error ? error->message : "unknown error");
    return;
  }

  // Send SMS
  if (!mm_sms_send_sync(sms, NULL, &error)) {
    g_printerr("send_sms_button_clicked: could't send sms: %s\n",
        error ? error->message : "unknown error");
  } else {
    printf("send_sms_button_clicked: SMS send success\n");
  }

  // Store SMS in data base
  if (!mm_sms_store_sync(sms, MM_SMS_STORAGE_TA, NULL, &error)) {
    g_printerr("Not able to store MO SMS to data base : %s\n",
        error ? error->message : "unknown error");
    return;
  }
}

// Callback for sms list button click
void list_sms_button_clicked(GtkWidget *button, gpointer data) {
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "List of Messages");
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), screen_width-100, screen_height/3);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  GtkWidget *list = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(window), list);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(list), vbox);

  GError *error = NULL;
  GList *smsList = mm_modem_messaging_list_sync(ctx->modem_messaging, NULL, &error);
  if (smsList == NULL) {
    g_printerr("list_sms_button_clicked: could't get sms list: %s\n",
        error ? error->message : "unknown error");
    return;
  }

  MMSms *sms;
  GList *l;
  for (l = smsList; l; l = g_list_next (l)) {
    sms = MM_SMS (l->data);
    GtkWidget *text_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), text_hbox, FALSE, FALSE, 0);

    // Create a text view with state
    MMSmsState state = mm_sms_get_state(sms);
    GtkWidget *text_lable = gtk_label_new("unknown");

    gchar *lable;
    printf("list_sms_button_clicked: state of the SMS %d\n", state);
    if (state == MM_SMS_STATE_RECEIVED || state == MM_SMS_STATE_RECEIVING) {
      lable = "Inbox";
    } else if (state == MM_SMS_STATE_SENDING) {
      lable = "Outbox";
    } else if (state == MM_SMS_STATE_SENT) {
      lable = "Sent";
    } else if (state == MM_SMS_STATE_STORED || state == MM_SMS_STATE_UNKNOWN) {
      lable = "Draft";
    }
    gtk_label_set_label(text_lable, lable);
    gtk_box_pack_start(GTK_BOX(text_hbox), GTK_WIDGET(text_lable), FALSE, FALSE, 5);

    gchar *sms_str = g_strconcat(mm_sms_get_number(sms), " ", mm_sms_get_text(sms), " ",
        mm_sms_get_timestamp(sms), NULL);
    GtkWidget *text_view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer((GtkTextView*)text_view);
    gtk_text_buffer_set_text(buffer, sms_str, strlen(sms_str));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE); // Not editable
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE); // Remove cursor
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_box_pack_start(GTK_BOX(text_hbox), GTK_WIDGET(text_view), TRUE, TRUE, 5);
  }

  gtk_widget_show_all(window);
  gtk_main();
}

gboolean number_view_clicked(GtkWidget *text_view, GdkEventFocus event, gpointer data) {
  is_number_view_active = TRUE;
  return TRUE;
}

gboolean text_view_clicked(GtkWidget *text_view, GdkEventFocus event, gpointer data) {
  is_number_view_active = FALSE;
  return TRUE;
}

// Function to create a button with a given label
GtkWidget *create_key_button(const gchar *label, struct smsEditor *editor) {
  GtkWidget *button = gtk_button_new_with_label(label);
  g_signal_connect(button, "clicked", G_CALLBACK(button_clicked), editor);
  return button;
}

void display_virtual_key_board(struct smsEditor *editor) {
  // Define caps lock keyboard layout
  const gchar *cap_layout[] = {
      "!", "@", "#", "$", "%", "&", "*", "(", ")", "_", "<--",
      "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "Enter",
      "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "Shift",
      "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "Space",
  };

  // Define the normal keyboard layout
  const gchar *small_layout[] = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "<--",
      "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "Enter",
      "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "Shift",
      "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Space",
  };

  // Remove existing grid view to redraw based on shift key
  int i;
  for (i = 0; i< 4; i++) { // 4 is number of rows in layout
    gtk_grid_remove_row(GTK_GRID(editor->grid), 0);
  }

  const gchar **layout;
  if (editor->shift) {
    layout = cap_layout;
  } else {
    layout = small_layout;
  }

  int row = 0, col = 0;
  // Create buttons for each key and add them to the grid
  for (i = 0; i < sizeof(cap_layout) / sizeof(cap_layout[0]); ++i) {
    GtkWidget *button = create_key_button(layout[i], editor);
    gtk_grid_attach(GTK_GRID(editor->grid), button, col, row, 1, 1);

    // Update row and column for the next button
    col++;
    if (col > 10) {
      col = 0;
      row++;
    }
  }

  gtk_widget_show_all(editor->window);
}

// Callback for network type changed
void network_type_changed(GtkComboBox *combo, gpointer data) {
  GError *error = NULL;
  GtkComboBoxText *combo_text = (GtkComboBoxText *)combo;
  printf("network_type_changed: %s \n", gtk_combo_box_text_get_active_text(combo_text));
  MMModemMode preferred = mm_common_get_modes_from_string(
      gtk_combo_box_text_get_active_text(combo_text));
  if (!mm_modem_set_current_modes_sync(ctx->modem, ctx->allowed_modes, preferred, NULL, &error)) {
    g_printerr("network_type_changed error: couldn't set preferred network mode: %s\n",
        error ? error->message : "unknown error");
  } else {
    printf("network_type_changed, set preferred network mode is SUCCESS\n");
  }
}

// Callback for compose sms button click
void compose_sms_button_clicked(GtkWidget *button, gpointer data) {
  struct smsEditor editor;
  memset(&editor, 0, sizeof editor);

  editor.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(editor.window), "Compose Message");
  gtk_window_set_position(GTK_WINDOW(editor.window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(editor.window), screen_width-100, screen_height/3);
  g_signal_connect(G_OBJECT(editor.window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // Create a vertical box to hold the text/number view and the virtual keyboard
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(editor.window), vbox);

  // Create a number view with lable
  GtkWidget *number_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(number_hbox), TRUE, TRUE, 0);

  GtkWidget *number_lable = gtk_label_new("Number");
  gtk_box_pack_start(GTK_BOX(number_hbox), GTK_WIDGET(number_lable), FALSE, FALSE, 5);

  editor.number_view = gtk_text_view_new();
  g_signal_connect(editor.number_view, "focus-in-event", G_CALLBACK(number_view_clicked), NULL);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editor.number_view), GTK_WRAP_WORD);
  gtk_box_pack_start(GTK_BOX(number_hbox), GTK_WIDGET(editor.number_view), TRUE, TRUE, 0);

  // Create a text view with lable
  GtkWidget *text_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(text_hbox), TRUE, TRUE, 0);

  GtkWidget *text_lable = gtk_label_new("Text");
  gtk_box_pack_start(GTK_BOX(text_hbox), GTK_WIDGET(text_lable), FALSE, FALSE, 5);

  editor.text_view = gtk_text_view_new();
  g_signal_connect(editor.text_view, "focus-in-event", G_CALLBACK(text_view_clicked), NULL);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editor.text_view), GTK_WRAP_WORD);
  gtk_box_pack_start(GTK_BOX(text_hbox), GTK_WIDGET(editor.text_view), TRUE, TRUE, 0);

  // Create a button to send SMS
  GtkWidget *sendSms = gtk_button_new_with_label("sendSMS");
  g_signal_connect(sendSms, "clicked", G_CALLBACK(send_sms_button_clicked), &editor);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(sendSms), FALSE, FALSE, 10);

  // Create a grid for the virtual keyboard layout
  editor.grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(editor.grid), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(editor.grid), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), editor.grid, TRUE, TRUE, 5);

  display_virtual_key_board(&editor);
  gtk_main();
}

// Initialize modem inteface context
gboolean initializeContext() {
  GError *error = NULL;

  /* Initialize context */
  ctx = g_new0(Context, 1);

  /* Setup dbus connection to use */
  ctx->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

  if (!ctx->connection) {
    g_printerr("error: couldn't get bus: %s\n", error ? error->message : "unknown error");
    return FALSE;
  }

  ctx->manager = mm_manager_new_sync(ctx->connection,
      G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START, NULL, &error);

  if (!ctx->manager) {
    g_printerr("error: couldn't create manager: %s\n", error ? error->message : "unknown error");
    return FALSE;
  }

  GList *modems = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (ctx->manager));
  GList *l;
  MMModem  *modem;
  MMModemMessaging *modem_messaging;

  // Taking first found modem
  MMObject *obj = MM_OBJECT (modems[0].data);
  modem = mm_object_get_modem (obj);
  modem_messaging = mm_object_get_modem_messaging(obj);

  g_list_free_full(modems, g_object_unref);

  if (!modem) {
    g_printerr("error: couldn't get modem \n");
    g_object_unref(ctx->manager);
    return FALSE;
  }

  if (!modem_messaging) {
    g_printerr("error: couldn't get modem messaging \n");
    g_object_unref(ctx->manager);
    return FALSE;
  }

  ctx->modem = modem;
  ctx->modem_messaging = modem_messaging;
  return TRUE;
}

// Listen for SMS state change
static void sms_state_updated(MMSms *sms) {
  gint state = mm_sms_get_state(sms);

  if (state == MM_SMS_STATE_RECEIVED) {
    g_print ("[%s] new sms : %s\n", mm_sms_get_path (sms), mm_sms_state_get_string (state));
  }
}

// Call back for MT SMS received
static gboolean sms_added(MMModemMessaging *modem_messaging, const gchar *sms_path,
    gboolean received) {
  GList *sms_list;
  GList *l;
  MMSms *new_sms = NULL;
  GError *error = NULL;

  sms_list = mm_modem_messaging_list_sync (modem_messaging, NULL, NULL);
  for (l = sms_list; l && !new_sms; l = g_list_next (l)) {
    MMSms *l_sms = MM_SMS  (l->data);

    if (g_strcmp0 (mm_sms_get_path (l_sms), sms_path) == 0)
        new_sms = l_sms;
  }
  g_assert (new_sms);

  gint state = mm_sms_get_state (new_sms);

  // MT SMS not received completely
  if (state == MM_SMS_STATE_RECEIVING) {
    g_signal_connect (new_sms, "notify::state", G_CALLBACK (sms_state_updated), NULL);
  } else if (state == MM_SMS_STATE_RECEIVED) {
    g_print ("[%s] new sms: %s\n", mm_sms_get_path (new_sms), mm_sms_state_get_string (state));
  } else {
    g_print("Received signal is not for MT SMS\n");
  }

  g_list_free_full (sms_list, g_object_unref);
  return TRUE;
}

int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  // Get the default display size and set the font as per resolution.
  GdkDisplay *display = gdk_display_get_default();

  if (display) {
    GdkScreen *screen = gdk_screen_get_default();
    double resolution = gdk_screen_get_resolution(screen);
    printf("default resolution %f\n", resolution);

    // Calculate font based on resolution, font choosen as 12 and can be changed
    int font_size = (int)(16.0 * (resolution / 96.0));
    GtkSettings *settings = gtk_settings_get_default();
    gchar *font_name = g_strdup_printf("Sans %d", font_size);
    g_object_set(settings,"gtk-font-name", font_name, NULL);
    printf("Updating font size %d\n", font_size);

    screen_width = gdk_screen_width();
    screen_height = gdk_screen_height();
    printf("Display Width: %d height: %d\n", screen_width, screen_height);
  } else {
    g_printerr("Failed to get the default display.\n");
    // default values
    screen_width = 500;
    screen_height = 800;
  }

  if (!initializeContext()) {
    g_printerr("error: couldn't initialize modem manager context\n");
    return 1;
  }

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Telephony App");
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), screen_width-100, screen_height/3);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // Create a vertical box to hold the telephony app options.
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  GtkWidget *sms = gtk_button_new_with_label("Compose Message");
  g_signal_connect(sms, "clicked", G_CALLBACK(compose_sms_button_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(sms), FALSE, FALSE, 5);

  GtkWidget *list_sms = gtk_button_new_with_label("List of Messages");
  g_signal_connect(list_sms, "clicked", G_CALLBACK(list_sms_button_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(list_sms), FALSE, FALSE, 0);

  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, FALSE, 5);

  GtkWidget *lable = gtk_label_new("Network Type");
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(lable), FALSE, FALSE, 10);

  GtkWidget *combo = gtk_combo_box_text_new();
  ctx->allowed_modes = MM_MODEM_MODE_NONE;
  MMModemMode preferred_mode = MM_MODEM_MODE_NONE;
  gchar *allowed_modes_string = NULL;
  int preferred_index = 0;
  if (mm_modem_get_current_modes(ctx->modem, &(ctx->allowed_modes), &preferred_mode)) {
    allowed_modes_string = mm_modem_mode_build_string_from_mask(ctx->allowed_modes);
    g_auto(GStrv) mode_strings = g_strsplit(allowed_modes_string, ", ", -1);
    g_free(allowed_modes_string);
    int i;
    gchar *preferred_string = mm_modem_mode_build_string_from_mask(preferred_mode);
    for (i = 0; mode_strings[i]; i++) {
      if (!g_strcmp0(mode_strings[i], preferred_string)) {
        preferred_index = i;
      }
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), NULL, mode_strings[i]);
    }
  }

  printf("get allowed network mode %d preferred mode %d index %d\n", ctx->allowed_modes,
      preferred_mode, preferred_index);
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), preferred_index);
  g_signal_connect(combo, "changed", G_CALLBACK(network_type_changed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(combo), FALSE, FALSE, 0);

  // Register for incoming messages
  g_signal_connect (ctx->modem_messaging, "added", G_CALLBACK (sms_added), NULL);
  gtk_widget_show_all(window);
  gtk_main();

  return 0;
}

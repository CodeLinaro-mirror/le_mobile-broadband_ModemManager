/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * SPDX-License-Identifier:  LGPL-2.0
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef MM_SMS_STORAGE_H
#define MM_SMS_STORAGE_H

#include <glib.h>
#include <ModemManager.h>

gboolean
mm_sms_storage_store_message(gchar *pdu,
                             MMSmsState state, guint32 *idx, GError **error);
gboolean
mm_sms_storage_delete_message (guint index, GError **error);
gboolean
mm_sms_storage_read_message (guint index,
                             gchar **pdu, MMSmsState *state, GError **error);
GList *
mm_sms_storage_read_all_indexces (GError **error);



#endif /* MM_SMS_STORAGE_H */

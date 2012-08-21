/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <pmapi.h>
#include <appcore-efl.h>
#include <Ecore_X.h>
#include <utilX.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <syspopup.h>
#include <E_DBus.h>
#include <aul.h>

#include "bt-syspopup.h"

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

static int __bluetooth_term(bundle *b, void *data)
{
	bt_log_print(BT_POPUP, "\nSystem-popup: terminate");
	return 0;
}

static int __bluetooth_timeout(bundle *b, void *data)
{
	bt_log_print(BT_POPUP, "\nSystem-popup: timeout");
	return 0;
}

syspopup_handler handler = {
	.def_term_fn = __bluetooth_term,
	.def_timeout_fn = __bluetooth_timeout
};

/* Cleanup objects to avoid mem-leak */
static void __bluetooth_cleanup(struct bt_popup_appdata *ad)
{
	if (ad == NULL)
		return;

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;
}

static void __bluetooth_parse_event(struct bt_popup_appdata *ad, const char *event_type)
{
	if (!strcasecmp(event_type, "pin-request"))
		ad->event_type = BT_EVENT_PIN_REQUEST;
	else if (!strcasecmp(event_type, "passkey-confirm-request"))
		ad->event_type = BT_EVENT_PASSKEY_CONFIRM_REQUEST;
	else if (!strcasecmp(event_type, "passkey-request"))
		ad->event_type = BT_EVENT_PASSKEY_REQUEST;
	else if (!strcasecmp(event_type, "authorize-request"))
		ad->event_type = BT_EVENT_AUTHORIZE_REQUEST;
	else if (!strcasecmp(event_type, "app-confirm-request"))
		ad->event_type = BT_EVENT_APP_CONFIRM_REQUEST;
	else if (!strcasecmp(event_type, "push-authorize-request"))
		ad->event_type = BT_EVENT_PUSH_AUTHORIZE_REQUEST;
	else if (!strcasecmp(event_type, "confirm-overwrite-request"))
		ad->event_type = BT_EVENT_CONFIRM_OVERWRITE_REQUEST;
	else if (!strcasecmp(event_type, "keyboard-passkey-request"))
		ad->event_type = BT_EVENT_KEYBOARD_PASSKEY_REQUEST;
	else if (!strcasecmp(event_type, "bt-information"))
		ad->event_type = BT_EVENT_INFORMATION;
	else if (!strcasecmp(event_type, "exchange-request"))
		ad->event_type = BT_EVENT_EXCHANGE_REQUEST;
	else
		ad->event_type = 0x0000;
		return;
}

static void __bluetooth_request_to_cancel(void)
{
	/* To be implement it */
}

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad)
{
	switch (ad->event_type) {
	case BT_EVENT_PIN_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_STRING, "", G_TYPE_INVALID,
					   G_TYPE_INVALID);

		break;


	case BT_EVENT_KEYBOARD_PASSKEY_REQUEST:

		__bluetooth_request_to_cancel();

		break;

	case BT_EVENT_PASSKEY_CONFIRM_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_PASSKEY_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPassKey",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_STRING, "", G_TYPE_INVALID,
					   G_TYPE_INVALID);

		break;

	case BT_EVENT_PASSKEY_DISPLAY_REQUEST:
		/* Nothing to do */
		break;

	case BT_EVENT_AUTHORIZE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_APP_CONFIRM_REQUEST:
		{
			DBusMessage *msg = NULL;
			int response = 2;

			msg = dbus_message_new_signal(
					BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
					BT_SYS_POPUP_INTERFACE,
					BT_SYS_POPUP_METHOD_RESPONSE);

			/* For timeout rejection is sent to  be handled in
			   application */
			response = 1;

			dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response,
				 DBUS_TYPE_INVALID);

			e_dbus_message_send(ad->EDBusHandle,
				msg, NULL, -1, NULL);

			dbus_message_unref(msg);
		}
		break;

	case BT_EVENT_PUSH_AUTHORIZE_REQUEST:
	case BT_EVENT_EXCHANGE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->obex_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_CONFIRM_OVERWRITE_REQUEST:

		dbus_g_proxy_call_no_reply(ad->obex_proxy,
					   "ReplyOverwrite",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	default:
		break;
	}

	__bluetooth_win_del(ad);
}

static int __bluetooth_keydown_cb(void *data, int type, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	Ecore_Event_Key *ev = event_info;

	if (!strcmp(ev->keyname, KEY_END) || !strcmp(ev->keyname, KEY_SELECT)) {
		bt_log_print(BT_POPUP, "Key [%s]", ev->keyname);
		/* remove_all_event(); */

		if (!strcmp(ev->keyname, KEY_END)) {
			__bluetooth_remove_all_event(ad);
		}
	}

	return 0;
}

static int __bluetooth_request_timeout_cb(void *data)
{
	struct bt_popup_appdata *ad;

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;

	bt_log_print(BT_POPUP, "Request time out, Canceling reqeust");

	/* Destory UI and timer */
	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	__bluetooth_remove_all_event(ad);
	return 0;
}

static void __bluetooth_input_request_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);
	int response = 0;
	char *input_text = NULL;
	char *convert_input_text = NULL;

	if (ad == NULL)
		return;

	/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */

	input_text = (char *)elm_entry_entry_get(ad->entry);

	if (input_text) {
		convert_input_text =
		    elm_entry_markup_to_utf8(input_text);
	}

	if (!strcmp(event, BT_STR_OK))
		response = 1;
	else
		response = 0;

	if (convert_input_text == NULL)
		return;

	bt_log_print(BT_POPUP, "PIN/Passkey[%s] event[%d] response[%d]",
		     convert_input_text, ad->event_type, response);

	if (response == 1) {
		bt_log_print(BT_POPUP, "Done case");
		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPinCode",
						   G_TYPE_UINT, BT_AGENT_ACCEPT,
						   G_TYPE_STRING,
						   convert_input_text,
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPassKey",
						   G_TYPE_UINT, BT_AGENT_ACCEPT,
						   G_TYPE_STRING,
						   convert_input_text,
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		}
	} else {
		bt_log_print(BT_POPUP, "Cancel case");
		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPinCode",
						   G_TYPE_UINT, BT_AGENT_CANCEL,
						   G_TYPE_STRING, "",
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
						   "ReplyPassKey",
						   G_TYPE_UINT, BT_AGENT_CANCEL,
						   G_TYPE_STRING, "",
						   G_TYPE_INVALID,
						   G_TYPE_INVALID);
		}
	}

	__bluetooth_delete_input_view(ad);

	free(convert_input_text);

	__bluetooth_win_del(ad);
}

static void __bluetooth_input_cancel_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_request_to_cancel();

	__bluetooth_delete_input_view(ad);

	__bluetooth_win_del(ad);
}

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (ad == NULL)
		return;

	if (!strcmp(event, BT_STR_OK)) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	}

	evas_object_del(obj);

	__bluetooth_win_del(ad);
}

static int __bluetooth_init_app_signal(struct bt_popup_appdata *ad)
{
	if (NULL == ad)
		return FALSE;

	e_dbus_init();
	ad->EDBusHandle = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (!ad->EDBusHandle) {
		bt_log_print(BT_POPUP, "e_dbus_bus_get failed  \n ");
		return FALSE;
	} else {
		bt_log_print(BT_POPUP, "e_dbus_bus_get success \n ");
		e_dbus_request_name(ad->EDBusHandle,
				    BT_SYS_POPUP_IPC_NAME, 0, NULL, NULL);
	}
	return TRUE;
}

static void __bluetooth_app_confirm_cb(void *data,
				     Evas_Object *obj, void *event_info)
{
	bt_log_print(BT_POPUP, "__bluetooth_app_confirm_cb ");
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	DBusMessage *msg = NULL;
	int response = 0;

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

	if (!strcmp(event, BT_STR_YES) || !strcmp(event, BT_STR_OK))
		response = 0;
	else
		response = 1;

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	evas_object_del(obj);

	__bluetooth_win_del(ad);
}

static void __bluetooth_authorization_request_cb(void *data,
					       Evas_Object *obj,
					       void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!strcmp(event, BT_STR_YES)) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	}

	__bluetooth_win_del(ad);
}

static void __bluetooth_push_authorization_request_cb(void *data,
						    Evas_Object *obj,
						    void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!strcmp(event, BT_STR_YES))
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	else
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

	__bluetooth_win_del(ad);
}

static void __bluetooth_confirm_overwrite_request_cb(void *data,
						   Evas_Object *obj,
						   void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!strcmp(event, BT_STR_YES))
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyOverwrite",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	else
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyOverwrite",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

	__bluetooth_win_del(ad);
}

static void __bluetooth_ime_hide(void)
{
	Ecore_IMF_Context *imf_context = NULL;
	imf_context = ecore_imf_context_add(ecore_imf_context_default_id_get());
	if (imf_context)
		ecore_imf_context_input_panel_hide(imf_context);
}

static void __bluetooth_entry_change_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	char *input_text = NULL;
	char *convert_input_text = NULL;
	char *output_text = NULL;
	int text_length = 0;

	input_text = (char *)elm_entry_entry_get(obj);

	if (input_text) {
		convert_input_text = elm_entry_markup_to_utf8(input_text);
		if (convert_input_text) {
			text_length = strlen(convert_input_text);

			if (text_length == 0) {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_TRUE);
				elm_object_signal_emit(ad->editfield,
							"elm,state,eraser,hide",
							"elm");
			} else {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_FALSE);
				elm_object_signal_emit(ad->editfield,
							"elm,state,eraser,show",
							"elm");
			}

			if (ad->event_type == BT_EVENT_PASSKEY_REQUEST) {
				if (text_length > BT_PK_MLEN) {
					text_length = BT_PK_MLEN;
					convert_input_text[BT_PK_MLEN] = '\0';
					output_text =
					    elm_entry_utf8_to_markup
					    (convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			} else {
				if (text_length > BT_PIN_MLEN) {
					text_length = BT_PIN_MLEN;
					convert_input_text[BT_PIN_MLEN] = '\0';
					output_text =
					    elm_entry_utf8_to_markup
						(convert_input_text);

					elm_entry_entry_set(obj, output_text);
					elm_entry_cursor_end_set(obj);
					free(output_text);
				}
			}
			free(convert_input_text);
		}
	}
}

static void __bluetooth_entry_focused_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	if (!elm_entry_is_empty(obj))
		elm_object_signal_emit(data, "elm,state,eraser,show", "elm");

	elm_object_signal_emit(data, "elm,state,guidetext,hide", "elm");
}

static void __bluetooth_entry_unfocused_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	if (elm_entry_is_empty(obj))
		elm_object_signal_emit(data, "elm,state,guidetext,show", "elm");

	elm_object_signal_emit(data, "elm,state,eraser,hide", "elm");
}

static void __bluetooth_eraser_clicked_cb(void* data, Evas_Object* obj,
				const char* emission, const char* source)
{
	elm_entry_entry_set(data, "");
}

static void __bluetooth_check_chagned_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	Eina_Bool state = EINA_FALSE;

        if (obj == NULL)
                return;

        state = elm_check_state_get(obj);
        elm_entry_password_set(data, !state);
}

static void __bluetooth_draw_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN] = { 0 };
	Ecore_X_Window xwin;
	Evas_Object *btn1;
	Evas_Object *btn2;

	bt_log_print(BT_POPUP, "__bluetooth_draw_popup");

	ad->popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND,
					 EVAS_HINT_EXPAND);

	if (title != NULL) {
		snprintf(temp_str, BT_TITLE_STR_MAX_LEN+BT_TEXT_EXTRA_LEN,
			"<align=center>%s</align>", title);
		elm_object_text_set(ad->popup, temp_str);
	}

	if ((btn1_text != NULL) && (btn2_text != NULL)) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup_button/default");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);

		btn2 = elm_button_add(ad->popup);
		elm_object_style_set(btn2, "popup_button/default");
		elm_object_text_set(btn2, btn2_text);
		elm_object_part_content_set(ad->popup, "button2", btn2);
		evas_object_smart_callback_add(btn2, "clicked", func, ad);
	} else if (btn1_text != NULL) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup_button/default");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);
	}

	xwin = elm_win_xwindow_get(ad->popup);
	ecore_x_netwm_window_type_set(xwin, ECORE_X_WINDOW_TYPE_NOTIFICATION);
	utilx_set_system_notification_level(ecore_x_display_get(), xwin,
				UTILX_NOTIFICATION_LEVEL_NORMAL);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);

	bt_log_print(BT_POPUP, "__bluetooth_draw_popup END");
}

static void __bluetooth_draw_input_view(struct bt_popup_appdata *ad,
			const char *title, const char *text,
			void (*func)
			(void *data, Evas_Object *obj, void *event_info))
{
	Ecore_X_Window xwin;
	Evas_Object *conformant = NULL;
	Evas_Object *content = NULL;
	Evas_Object *passpopup = NULL;
	Evas_Object *box = NULL;
	Evas_Object *label = NULL;
	Evas_Object *editfield = NULL;
	Evas_Object *entry = NULL;
	Evas_Object *check = NULL;
	Evas_Object *l_button = NULL;
	Evas_Object *r_button = NULL;

	if (ad == NULL || ad->win_main == NULL) {
		bt_log_print(BT_POPUP, "Invalid parameter");
		return;
	}

	conformant = elm_conformant_add(ad->win_main);
	if (conformant == NULL) {
		bt_log_print(BT_POPUP, "conformant is NULL");
		return;
	}
	ad->popup = conformant;

	elm_win_conformant_set(ad->win_main, EINA_TRUE);
	elm_win_resize_object_add(ad->win_main, conformant);
	evas_object_size_hint_weight_set(conformant, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(conformant, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(conformant);

	content = elm_layout_add(conformant);
	elm_object_content_set(conformant, content);

	passpopup = elm_popup_add(content);
	elm_object_part_text_set(passpopup, "title,text", title);

	box = elm_box_add(passpopup);
	evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(box);

	label = elm_label_add(box);
	elm_object_style_set(label, "popup/default");
	elm_object_text_set(label, text);
	elm_label_line_wrap_set(label, ELM_WRAP_CHAR);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(label);
	elm_box_pack_end(box, label);

	editfield = elm_layout_add(box);
	elm_layout_theme_set(editfield, "layout", "editfield", "default");
	evas_object_size_hint_weight_set(editfield, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(editfield, EVAS_HINT_FILL, EVAS_HINT_FILL);
	ad->editfield = editfield;

	entry = elm_entry_add(box);
	elm_object_part_content_set(editfield, "elm.swallow.content", entry);
	elm_object_part_text_set(editfield, "elm.text", text);
	elm_entry_single_line_set(entry, EINA_TRUE);
	elm_entry_scrollable_set(entry, EINA_TRUE);
	ad->entry = entry;

	evas_object_smart_callback_add(entry, "changed",
				__bluetooth_entry_change_cb,
				ad);

	evas_object_smart_callback_add(entry, "focused",
				__bluetooth_entry_focused_cb,
				editfield);

	evas_object_smart_callback_add(entry, "unfocused",
				__bluetooth_entry_unfocused_cb,
				editfield);

	elm_object_signal_callback_add(editfield, "elm,eraser,clicked", "elm",
				(Edje_Signal_Cb)__bluetooth_eraser_clicked_cb,
				entry);

	elm_entry_password_set(entry, TRUE);
	elm_entry_input_panel_layout_set(entry,
				ELM_INPUT_PANEL_LAYOUT_NUMBERONLY);

	evas_object_show(entry);
	evas_object_show(editfield);
	elm_object_focus_set(entry, EINA_TRUE);
	elm_box_pack_end(box, editfield);

	check = elm_check_add(box);
	elm_object_text_set(check, BT_STR_SHOW_PASSWORD);
	elm_object_focus_allow_set(check, EINA_FALSE);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND,
					EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(check, EVAS_HINT_FILL,
					EVAS_HINT_FILL);
	evas_object_smart_callback_add(check, "changed",
				__bluetooth_check_chagned_cb, entry);
	evas_object_show(check);
	elm_box_pack_end(box, check);

	elm_object_content_set(passpopup, box);

	l_button = elm_button_add(ad->win_main);
	elm_object_style_set(l_button, "popup_button/default");
	elm_object_text_set(l_button, BT_STR_OK);
	elm_object_part_content_set(passpopup, "button1", l_button);
	evas_object_smart_callback_add(l_button, "clicked", func, ad);
	elm_object_disabled_set(l_button, EINA_TRUE);

	ad->edit_field_save_btn = l_button;

	r_button = elm_button_add(ad->win_main);
	elm_object_style_set(r_button, "popup_button/default");
	elm_object_text_set(r_button, BT_STR_CANCEL);
	elm_object_part_content_set(passpopup, "button2", r_button);
	evas_object_smart_callback_add(r_button, "clicked", func, ad);
	evas_object_show(passpopup);

	xwin = elm_win_xwindow_get(conformant);
	ecore_x_netwm_window_type_set(xwin, ECORE_X_WINDOW_TYPE_NOTIFICATION);
	utilx_set_system_notification_level(ecore_x_display_get(), xwin,
				UTILX_NOTIFICATION_LEVEL_NORMAL);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();
}

/* AUL bundle handler */
static int __bluetooth_launch_handler(struct bt_popup_appdata *ad,
			     void *reset_data, const char *event_type)
{
	bundle *kb = (bundle *) reset_data;
	char view_title[BT_TITLE_STR_MAX_LEN] = { 0 };
	char text[BT_GLOBALIZATION_STR_LENGTH] = { 0 };
	int timeout = 0;
	const char *device_name = NULL;
	const char *passkey = NULL;
	const char *file = NULL;
	char *conv_str = NULL;

	bt_log_print(BT_POPUP, "__bluetooth_launch_handler");

	if (!reset_data || !event_type)
		return -1;

	if (!strcasecmp(event_type, "pin-request")) {
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_BLUETOOTH_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);
	} else if (!strcasecmp(event_type, "passkey-confirm-request")) {
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS,
			     conv_str, passkey);

			bt_log_print(BT_POPUP, "title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
					BT_STR_OK, BT_STR_CANCEL,
					__bluetooth_passkey_confirm_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "passkey-request")) {
		const char *device_name = NULL;

		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_BLUETOOTH_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);

	} else if (!strcasecmp(event_type, "passkey-display-request")) {
		/* Nothing to do */
	} else if (!strcasecmp(event_type, "authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_ALLOW_PS_TO_CONNECT_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_YES, BT_STR_NO,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "app-confirm-request")) {
		bt_log_print(BT_POPUP, "app-confirm-request");
		timeout = BT_AUTHORIZATION_TIMEOUT;

		const char *title = NULL;
		const char *type = NULL;

		title = bundle_get_val(kb, "title");
		type = bundle_get_val(kb, "type");

		if (!title)
			return -1;

		if (strcasecmp(type, "twobtn") == 0) {
			__bluetooth_draw_popup(ad, title, BT_STR_YES, BT_STR_NO,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "onebtn") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, title, BT_STR_YES, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "push-authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		file = bundle_get_val(kb, "file");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_RECEIVE_PS_FROM_PS_Q, file, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_YES, BT_STR_NO,
				__bluetooth_push_authorization_request_cb);
	} else if (!strcasecmp(event_type, "confirm-overwrite-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		file = bundle_get_val(kb, "file");

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_OVERWRITE_FILE_Q, file);

		__bluetooth_draw_popup(ad, view_title, BT_STR_YES, BT_STR_NO,
				__bluetooth_confirm_overwrite_request_cb);
	} else if (!strcasecmp(event_type, "keyboard-passkey-request")) {
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			bt_log_print(BT_POPUP, "title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
						BT_STR_CANCEL, NULL,
						__bluetooth_input_cancel_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "bt-information")) {
		bt_log_print(BT_POPUP, "bt-information");
		timeout = BT_NOTIFICATION_TIMEOUT;

		const char *title = NULL;
		const char *type = NULL;

		title = bundle_get_val(kb, "title");
		type = bundle_get_val(kb, "type");

		if (title != NULL) {
			if (strlen(title) > 255)
				return -1;
		} else
			return -1;

		if (strcasecmp(type, "onebtn") == 0) {
			__bluetooth_draw_popup(ad, title, BT_STR_OK, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			__bluetooth_draw_popup(ad, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "exchange-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_EXCHANGE_OBJECT_WITH_PS_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_popup(ad, view_title, BT_STR_YES, BT_STR_NO,
				     __bluetooth_authorization_request_cb);
	} else {

		return -1;
	}

	if (ad->event_type != BT_EVENT_FILE_RECIEVED)
		ad->timer = ecore_timer_add(timeout, (Ecore_Task_Cb)
					__bluetooth_request_timeout_cb,
					ad);

	return 0;
}

static void __bluetooth_win_del(void *data)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_cleanup(ad);

	elm_exit();
}

static Evas_Object *__bluetooth_create_win(const char *name)
{
	Evas_Object *eo;
	int w;
	int h;

	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
		ecore_x_window_size_get(ecore_x_window_root_first_get(),
					&w, &h);
		evas_object_resize(eo, w, h);
	}

	return eo;
}

static void __bluetooth_session_init(struct bt_popup_appdata *ad)
{
	DBusGConnection *conn = NULL;
	GError *err = NULL;

	g_type_init();

	conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err);

	if (!conn) {
		bt_log_print(BT_POPUP,
			     "ERROR: Can't get on system bus [%s]",
			     err->message);
		g_error_free(err);
		return;
	}

	ad->agent_proxy = dbus_g_proxy_new_for_name(conn,
						    "org.bluez.frwk_agent",
						    "/org/bluez/agent/frwk_agent",
						    "org.bluez.Agent");
	if (!ad->agent_proxy)
		bt_log_print(BT_POPUP, "Could not create a agent dbus proxy");

	ad->obex_proxy = dbus_g_proxy_new_for_name(conn,
						   "org.bluez.frwk_agent",
						   "/org/obex/ops_agent",
						   "org.openobex.Agent");
	if (!ad->obex_proxy)
		bt_log_print(BT_POPUP, "Could not create obex dbus proxy");

	if (!__bluetooth_init_app_signal(ad))
		bt_log_print(BT_POPUP, "__bt_syspopup_init_app_signal failed");
}

static int __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;

	bt_log_print(BT_POPUP, "__bluetooth_create() start.\n");

	/* create window */
	win = __bluetooth_create_win(PACKAGE);
	if (win == NULL)
		return -1;
	ad->win_main = win;

	/* init internationalization */
	if (appcore_set_i18n(BT_COMMON_PKG, BT_COMMON_RES) < 0)
		return -1;

	ecore_imf_init();
	ad->event_handle = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
						   (Ecore_Event_Handler_Cb)
						   __bluetooth_keydown_cb,
						   ad);

	__bluetooth_session_init(ad);

	return 0;
}

static int __bluetooth_terminate(void *data)
{
	struct bt_popup_appdata *ad = data;

	if (ad->event_handle)
		ecore_event_handler_del(ad->event_handle);

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;

	return 0;
}

static int __bluetooth_pause(void *data)
{

	return 0;
}

static int __bluetooth_resume(void *data)
{

	return 0;
}

static int __bluetooth_reset(bundle *b, void *data)
{
	struct bt_popup_appdata *ad = data;
	const char *event_type = NULL;
	int ret = 0;

	bt_log_print(BT_POPUP, "__bluetooth_reset()\n");

	if (ad == NULL) {
		bt_log_print(BT_POPUP, "App data is NULL\n");
		return -1;
	}

	/* Start Main UI */
	event_type = bundle_get_val(b, "event-type");

	if (event_type != NULL) {
		if (!strcasecmp(event_type, "terminate")) {
			__bluetooth_win_del(ad);
			return 0;
		}

		if (syspopup_has_popup(b)) {
			/* Destroy the existing popup*/
			__bluetooth_cleanup(ad);
			/* create window */
			ad->win_main = __bluetooth_create_win(PACKAGE);
			if (ad->win_main == NULL)
				return -1;
		}

		__bluetooth_parse_event(ad, event_type);

		elm_win_alpha_set(ad->win_main, EINA_TRUE);

		ret = syspopup_create(b, &handler, ad->win_main, ad);
		if (ret == -1) {
			bt_log_print(BT_POPUP, "syspopup_create err");
			__bluetooth_remove_all_event(ad);
		} else {
			ret = __bluetooth_launch_handler(ad,
						       b, event_type);

			if (ret != 0)
				__bluetooth_remove_all_event(ad);

			/* Change LCD brightness */
			ret = pm_change_state(LCD_NORMAL);
			if (ret != 0)
				bt_log_print(BT_POPUP, "Fail to change LCD");
		}
	} else {
		bt_log_print(BT_POPUP, "event type is NULL \n");
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct bt_popup_appdata ad;
	struct appcore_ops ops = {
		.create = __bluetooth_create,
		.terminate = __bluetooth_terminate,
		.pause = __bluetooth_pause,
		.resume = __bluetooth_resume,
		.reset = __bluetooth_reset,
	};

	memset(&ad, 0x0, sizeof(struct bt_popup_appdata));
	ops.data = &ad;

	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}


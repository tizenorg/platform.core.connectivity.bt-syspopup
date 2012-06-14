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

	if (ad->layout_main)
		evas_object_del(ad->layout_main);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->layout_main = NULL;
	ad->win_main = NULL;
}

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad)
{
	switch (ad->event_type) {
	case BT_EVENT_PIN_REQUEST:
	case BT_EVENT_KEYBOARD_PASSKEY_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_STRING, "", G_TYPE_INVALID,
					   G_TYPE_INVALID);

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


	if (ad->entry == NULL) {
		/* BT_EVENT_KEYBOARD_PASSKEY_REQUEST */
		convert_input_text = strdup(ad->passkey);

		if (!strcmp(event, BT_STR_YES))
			response = 1;
		else
			response = 0;
	} else {
		/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */

			input_text = (char *)elm_entry_entry_get(ad->entry);

		if (input_text) {
			convert_input_text =
			    elm_entry_markup_to_utf8(input_text);
		}

		if (!strcmp(event, BT_STR_DONE))
			response = 1;
		else
			response = 0;
	}

	if (convert_input_text == NULL)
		return;

	bt_log_print(BT_POPUP, "PIN/Passkey[%s] event[%d] response[%d]",
		     convert_input_text, ad->event_type, response);

	if (response == 1) {
		bt_log_print(BT_POPUP, "Done case");
		if (ad->event_type == BT_EVENT_PIN_REQUEST ||
		    ad->event_type == BT_EVENT_KEYBOARD_PASSKEY_REQUEST) {
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
		if (ad->event_type == BT_EVENT_PIN_REQUEST ||
		    ad->event_type == BT_EVENT_KEYBOARD_PASSKEY_REQUEST) {
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

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (ad == NULL)
		return;

	if (!strcmp(event, BT_STR_YES)) {
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

static Evas_Object *__bluetooth_create_bg(Evas_Object *parent, char *style)
{
	Evas_Object *bg = NULL;

	bg = elm_bg_add(parent);

	evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	if (style)
		elm_object_style_set(bg, style);

	elm_win_resize_object_add(parent, bg);

	evas_object_show(bg);

	return bg;
}

static Evas_Object *__bluetooth_create_layout(Evas_Object *parent)
{
	Evas_Object *layout = NULL;

	layout = elm_layout_add(parent);

	elm_layout_theme_set(layout, "layout", "application", "default");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND,
					 EVAS_HINT_EXPAND);

	elm_win_resize_object_add(parent, layout);

	evas_object_show(layout);

	return layout;
}

static Evas_Object *__bluetooth_add_navigationbar(Evas_Object *parent)
{
	Evas_Object *naviframe = NULL;

	naviframe = elm_naviframe_add(parent);
	elm_object_part_content_set(parent, "elm.swallow.content", naviframe);
	evas_object_show(naviframe);

	return naviframe;
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

			if (text_length == 0)
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_TRUE);
			else
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_FALSE);

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

static Evas_Object *__bluetooth_gl_icon_get(void *data,
					  Evas_Object *obj, const char *part)
{
	Evas_Object *entry = NULL;
	Evas_Object *layout = NULL;
	struct bt_popup_appdata *ad = NULL;

	bt_log_print(BT_POPUP, "__bluetooth_gl_icon_get");

	ad = (struct bt_popup_appdata *)data;

	if (data == NULL) {
		bt_log_print(BT_POPUP, "data is NULL");
		return NULL;
	}

	if (!strcmp(part, "elm.icon")) {

		layout = elm_layout_add(obj);
		elm_layout_theme_set(layout, "layout", "editfield", "default");

		entry = elm_entry_add(obj);
		ad->entry = entry;

		elm_entry_single_line_set(entry, EINA_TRUE);
		elm_entry_scrollable_set(entry, EINA_TRUE);
		elm_entry_password_set(entry, EINA_TRUE);

		elm_entry_input_panel_layout_set(entry,
			 ELM_INPUT_PANEL_LAYOUT_PHONENUMBER);

		elm_entry_input_panel_enabled_set(entry, EINA_TRUE);

		elm_object_part_content_set(layout, "elm.swallow.content", entry);

		evas_object_show(entry);
		elm_object_focus_set(entry, EINA_TRUE);

		evas_object_smart_callback_add(entry, "changed",
			__bluetooth_entry_change_cb, ad);

		return layout;
	}

	return NULL;
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
			"<align=center>%s</font>", title);
		elm_object_text_set(ad->popup, temp_str);
	}

	if ((btn1_text != NULL) && (btn2_text != NULL)) {
		btn1 = elm_button_add(ad->popup);
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);

		btn2 = elm_button_add(ad->popup);
		elm_object_text_set(btn2, btn2_text);
		elm_object_part_content_set(ad->popup, "button2", btn2);
		evas_object_smart_callback_add(btn2, "clicked", func, ad);
	} else if (btn1_text != NULL) {
		btn1 = elm_button_add(ad->popup);
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
			const char *title,
			void (*func)
			(void *data, Evas_Object *obj, void *event_info))
{
	Evas_Object *bg = NULL;
	Evas_Object *genlist = NULL;
	Evas_Object *l_button = NULL;
	Evas_Object *r_button = NULL;
	Elm_Object_Item *git = NULL;
	Elm_Object_Item *navi_it;
	Ecore_X_Window xwin;

	evas_object_show(ad->win_main);

	bg = __bluetooth_create_bg(ad->win_main, "group_list");

	ad->layout_main = __bluetooth_create_layout(ad->win_main);

	/* Show Indicator */
	elm_win_indicator_mode_set(ad->win_main, ELM_WIN_INDICATOR_SHOW);

	ad->navi_fr = __bluetooth_add_navigationbar(ad->layout_main);

	/* Create genlist */
	genlist = elm_genlist_add(ad->navi_fr);
	evas_object_show(genlist);

	/* Set item class for dialogue seperator */
	ad->sp_itc.item_style = "dialogue/separator/11/with_line";
	ad->sp_itc.func.text_get = NULL;
	ad->sp_itc.func.content_get = NULL;
	ad->sp_itc.func.state_get = NULL;
	ad->sp_itc.func.del = NULL;

	ad->itc.item_style = "dialogue/1icon";
	ad->itc.func.text_get = NULL;
	ad->itc.func.content_get = __bluetooth_gl_icon_get;
	ad->itc.func.state_get = NULL;
	ad->itc.func.del = NULL;

	/* Seperator */
	git = elm_genlist_item_append(genlist, &ad->sp_itc, NULL, NULL,
				    ELM_GENLIST_ITEM_NONE, NULL, NULL);

	elm_genlist_item_select_mode_set(git, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

	/*editfield for dialogue item (dialogue item) */
	elm_genlist_item_append(genlist, &ad->itc, ad, NULL,
				ELM_GENLIST_ITEM_NONE, NULL, ad);

	navi_it = elm_naviframe_item_push(ad->navi_fr, BT_STR_ENTER_PIN,
		       NULL, NULL, genlist, NULL);

	l_button = elm_button_add(ad->navi_fr);
	elm_object_style_set(l_button, "naviframe/title/default");
	elm_object_text_set(l_button, BT_STR_CANCEL);
	evas_object_show(l_button);
	evas_object_smart_callback_add(l_button, "clicked", func, ad);
	elm_object_item_part_content_set(navi_it, "title_left_btn", l_button);

	r_button = elm_button_add(ad->navi_fr);
	elm_object_style_set(r_button, "naviframe/title/default");
	elm_object_text_set(r_button, BT_STR_DONE);
	evas_object_show(r_button);
	evas_object_smart_callback_add(r_button, "clicked", func, ad);
	elm_object_disabled_set(r_button, EINA_TRUE);

	elm_object_item_part_content_set(navi_it, "title_right_btn", r_button);

	ad->edit_field_save_btn = r_button;

	xwin = elm_win_xwindow_get(ad->layout_main);
	ecore_x_netwm_window_type_set(xwin, ECORE_X_WINDOW_TYPE_NOTIFICATION);
	utilx_set_system_notification_level(ecore_x_display_get(), xwin,
				UTILX_NOTIFICATION_LEVEL_NORMAL);
}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();

	if (ad->navi_fr)
		elm_naviframe_item_pop(ad->navi_fr);

	if (ad->layout_main)
		evas_object_del(ad->layout_main);

	ad->navi_fr = NULL;
	ad->layout_main = NULL;
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
		ad->event_type = BT_EVENT_PIN_REQUEST;
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s (%s)", BT_STR_ENTER_PIN, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title,
					  __bluetooth_input_request_cb);
	} else if (!strcasecmp(event_type, "passkey-confirm-request")) {
		ad->event_type = BT_EVENT_PASSKEY_CONFIRM_REQUEST;
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			     BT_STR_PASSKEY_MATCH_Q, conv_str);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     "%s<br>%s", text, passkey);

			bt_log_print(BT_POPUP, "title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
					BT_STR_YES, BT_STR_NO,
					__bluetooth_passkey_confirm_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "passkey-request")) {
		const char *device_name = NULL;

		ad->event_type = BT_EVENT_PASSKEY_REQUEST;
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s (%s)", BT_STR_ENTER_PIN, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title,
					  __bluetooth_input_request_cb);

	} else if (!strcasecmp(event_type, "passkey-display-request")) {
		/* Nothing to do */
	} else if (!strcasecmp(event_type, "authorize-request")) {
		ad->event_type = BT_EVENT_AUTHORIZE_REQUEST;
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
		ad->event_type = BT_EVENT_APP_CONFIRM_REQUEST;
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
		ad->event_type = BT_EVENT_PUSH_AUTHORIZE_REQUEST;
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
		ad->event_type = BT_EVENT_CONFIRM_OVERWRITE_REQUEST;
		timeout = BT_AUTHORIZATION_TIMEOUT;

		file = bundle_get_val(kb, "file");

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 BT_STR_OVERWRITE_FILE_Q, file);

		__bluetooth_draw_popup(ad, view_title, BT_STR_YES, BT_STR_NO,
				__bluetooth_confirm_overwrite_request_cb);
	} else if (!strcasecmp(event_type, "keyboard-passkey-request")) {
		ad->event_type = BT_EVENT_KEYBOARD_PASSKEY_REQUEST;
		timeout = BT_AUTHENTICATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			snprintf(ad->passkey, sizeof(ad->passkey), "%s", passkey);

			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			     BT_STR_PASSKEY_MATCH_Q, conv_str);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     "%s<br>%s", text, passkey);

			bt_log_print(BT_POPUP, "title: %s", view_title);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_popup(ad, view_title,
						BT_STR_YES, BT_STR_NO,
						__bluetooth_input_request_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "bt-information")) {
		bt_log_print(BT_POPUP, "bt-information");
		ad->event_type = BT_EVENT_INFORMATION;
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
	} else if (!strcasecmp(event_type, "bt-security")) {
		const char *type = NULL;

		bt_log_print(BT_POPUP, "bt-security");
		ad->event_type = BT_EVENT_INFORMATION;
		timeout = BT_NOTIFICATION_TIMEOUT;

		type = bundle_get_val(kb, "type");

		if (!strcasecmp(type, "disabled")) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				 BT_STR_DISABLED_RESTRICTS);
		} else if (!strcasecmp(type, "handsfree")) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				 BT_STR_HANDS_FREE_RESTRICTS);
		}

		__bluetooth_draw_popup(ad, view_title, BT_STR_OK, NULL,
					__bluetooth_app_confirm_cb);
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
	if (!ad->agent_proxy) {
		bt_log_print(BT_POPUP, "Could not create a agent dbus proxy");
	}
	ad->obex_proxy = dbus_g_proxy_new_for_name(conn,
						   "org.bluez.frwk_agent",
						   "/org/obex/ops_agent",
						   "org.openobex.Agent");
	if (!ad->agent_proxy) {
		bt_log_print(BT_POPUP, "Could not create a agent dbus proxy");
	}

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

	if (ad->layout_main)
		evas_object_del(ad->layout_main);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->layout_main = NULL;
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
		if (syspopup_has_popup(b)) {
			if (!strcasecmp(event_type, "terminate")) {
				__bluetooth_win_del(ad);
				return 0;
			} else {
				/* Destroy the existing popup*/
				__bluetooth_cleanup(ad);
				/* create window */
				ad->win_main = __bluetooth_create_win(PACKAGE);
				if (ad->win_main == NULL)
					return -1;
			}
		}
		if (strcasecmp(event_type, "pin-request") != 0 &&
		      strcasecmp(event_type, "passkey-request") != 0)
			elm_win_alpha_set(ad->win_main, EINA_TRUE);

		ret = syspopup_create(b, &handler, ad->win_main, ad);
		if (ret == -1) {
			bt_log_print(BT_POPUP, "syspopup_create err");
		}
		else {
			ret = __bluetooth_launch_handler(ad,
						       b, event_type);

			if (ret != 0)
				return -1;

			/* Change LCD brightness */
			ret = pm_change_state(LCD_NORMAL);
			if (ret != 0)
				return -1;
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


/*
* bt-syspopup
*
* Copyright 2013 Samsung Electronics Co., Ltd
*
* Licensed under the Flora License, Version 1.1 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.tizenopensource.org/license
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include <stdio.h>
#include <app.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <syspopup.h>
#include <E_DBus.h>
#include <aul.h>
#include <bluetooth.h>
#include <feedback.h>
#include <device/display.h>
#include <efl_extension.h>
#include "bt-syspopup-w.h"
#include <dbus/dbus-glib-lowlevel.h>
#include <syspopup_caller.h>
#include <dd-display.h>

#define COLOR_TABLE "/usr/apps/org.tizen.bt-syspopup/shared/res/tables/com.samsung.bt-syspopup_ChangeableColorTable.xml"
#define FONT_TABLE "/usr/apps/org.tizen.bt-syspopup/shared/res/tables/com.samsung.bt-syspopup_FontInfoTable.xml"

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

static void __bluetooth_set_win_level(Evas_Object *parent);

static void __bluetooth_terminate(void *data);

static int __bluetooth_error_toast_timeout_cb(void *data);

static void __bluetooth_draw_error_toast_popup(struct bt_popup_appdata *ad, char *toast_text);

static void __bluetooth_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text);

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad);

static int __bluetooth_term(bundle *b, void *data)
{
	BT_DBG("System-popup: terminate");
	__bluetooth_remove_all_event((struct bt_popup_appdata *)data);
	__bluetooth_terminate(data);
	return 0;
}

static int __bluetooth_timeout(bundle *b, void *data)
{
	BT_DBG("System-popup: timeout");
	return 0;
}

syspopup_handler handler = {
	.def_term_fn = __bluetooth_term,
	.def_timeout_fn = __bluetooth_timeout
};

/* Cleanup objects to avoid mem-leak */
static void __bluetooth_cleanup(struct bt_popup_appdata *ad)
{
	BT_DBG("+");

	if (ad == NULL)
		return;

	if (ad->po) {
		bt_pincode_input_object *po = ad->po;
		if (po->circle_surface) {
			eext_circle_surface_del(po->circle_surface);
			po->circle_surface = NULL;
		}

		if (po->bg) {
			evas_object_del(po->bg);
			po->bg = NULL;
		}

		if (po->naviframe) {
			evas_object_del(po->naviframe);
			po->naviframe = NULL;
		}

		if (po->conformant) {
			evas_object_del(po->conformant);
			po->conformant = NULL;
		}

		if (po->input_guide_text) {
			g_free(po->input_guide_text);
			po->input_guide_text = NULL;
		}

		if (po->input_text) {
			g_free(po->input_text);
			po->input_text = NULL;
		}

		if (po->pincode) {
			g_free(po->pincode);
			po->pincode = NULL;
		}
	}

	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	if (ad->popup) {
		elm_access_object_unregister(ad->popup);
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}

	if (ad->ly_pass) {
		evas_object_del(ad->ly_pass);
		ad->ly_pass = NULL;
	}

	if (ad->ly_keypad) {
		evas_object_del(ad->ly_keypad);
		ad->ly_keypad = NULL;
	}

	if (ad->agent_proxy) {
		g_object_unref(ad->agent_proxy);
		ad->agent_proxy = NULL;
	}

#if 0
	if (ad->color_table) {
		ea_theme_color_table_free(ad->color_table);
		ad->color_table = NULL;
	}
	if (ad->font_table) {
		ea_theme_font_table_free(ad->font_table);
		ad->font_table = NULL;
	}
#endif

	if (ad->win_main) {
		evas_object_del(ad->win_main);
		ad->win_main = NULL;
	}

	BT_DBG("-");
}

static void __bluetooth_set_win_level(Evas_Object *parent)
{
#if 0
	Ecore_X_Window xwin;
	xwin = elm_win_xwindow_get(parent);
	if (xwin == 0) {
		BT_ERR("elm_win_xwindow_get is failed");
	} else {
		BT_DBG("Setting window type");
		ecore_x_netwm_window_type_set(xwin,
				ECORE_X_WINDOW_TYPE_NOTIFICATION);
		utilx_set_system_notification_level(ecore_x_display_get(),
				xwin, UTILX_NOTIFICATION_LEVEL_HIGH);
	}
#endif
}

static void __bluetooth_popup_hide_cb(void *data, Evas_Object *obj, void *event_info)
{
	elm_popup_dismiss(obj);
}

static void __bluetooth_popup_hide_finished_cb(void *data, Evas_Object *obj, void *event_info)
{
	ret_if(!obj);
	evas_object_del(obj);
}

static void __bluetooth_popup_block_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
	ret_if(!obj);
	elm_popup_dismiss(obj);
}

static void __lock_display()
{
#if 0
	int ret = device_power_request_lock(POWER_LOCK_DISPLAY, 0);
	if (ret < 0)
		BT_ERR("LCD Lock failed");
#endif
}

static void __unlock_display()
{
#if 0
	int ret = device_power_release_lock(POWER_LOCK_DISPLAY);
	if (ret < 0)
		BT_ERR("LCD Unlock failed");
#endif
}

static void __bluetooth_notify_event(feedback_pattern_e feedback)
{
	int result;

	BT_DBG("Notify event");

	result = feedback_initialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_ERR("feedback_initialize error : %d", result);
		return;
	}

	result = feedback_play(feedback);
	BT_INFO("feedback [%d], ret value [%d]", feedback, result);

	result = feedback_deinitialize();
	if (result != FEEDBACK_ERROR_NONE) {
		BT_INFO("feedback_initialize error : %d", result);
		return;
	}
}

static void __bluetooth_parse_event(struct bt_popup_appdata *ad, const char *event_type)
{
	BT_DBG("+");

	if (!strcasecmp(event_type, "pin-request"))
		ad->event_type = BT_EVENT_PIN_REQUEST;
	else if (!strcasecmp(event_type, "passkey-confirm-request"))
		ad->event_type = BT_EVENT_PASSKEY_CONFIRM_REQUEST;
	else if (!strcasecmp(event_type, "passkey-auto-accepted"))
		ad->event_type = BT_EVENT_PASSKEY_AUTO_ACCEPTED;
	else if (!strcasecmp(event_type, "passkey-request"))
		ad->event_type = BT_EVENT_PASSKEY_REQUEST;
	else if (!strcasecmp(event_type, "passkey-display-request"))
		ad->event_type = BT_EVENT_PASSKEY_DISPLAY_REQUEST;
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
	else if (!strcasecmp(event_type, "phonebook-request"))
		ad->event_type = BT_EVENT_PHONEBOOK_REQUEST;
	else if (!strcasecmp(event_type, "message-request"))
		ad->event_type = BT_EVENT_MESSAGE_REQUEST;
	else if (!strcasecmp(event_type, "unable-to-pairing"))
		ad->event_type = BT_EVENT_UNABLE_TO_PAIRING;
	else if (!strcasecmp(event_type, "music-auto-connect-request"))
		ad->event_type = BT_EVENT_HANDSFREE_AUTO_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "system-reset-request"))
		ad->event_type = BT_EVENT_SYSTEM_RESET_REQUEST;
	else
		ad->event_type = 0x0000;

	BT_DBG("-");
	return;

}

static void __bluetooth_request_to_cancel(void)
{
	bt_device_cancel_bonding();
}

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad)
{
	BT_INFO("Remove event 0X%X", ad->event_type);
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
		__unlock_display();

		break;

	case BT_EVENT_PASSKEY_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey",
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
			DBusMessage *msg;
			int response;

			msg = dbus_message_new_signal(
					BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
					BT_SYS_POPUP_INTERFACE,
					BT_SYS_POPUP_METHOD_RESPONSE);

			/* For timeout rejection is sent to  be handled in
			   application */
			response = BT_AGENT_REJECT;

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

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

		break;

	case BT_EVENT_CONFIRM_OVERWRITE_REQUEST: {
		DBusMessage *msg;
		int response = BT_AGENT_REJECT;

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
						BT_SYS_POPUP_INTERFACE,
						BT_SYS_POPUP_METHOD_RESPONSE);
		if (msg == NULL) {
			BT_ERR("msg == NULL, Allocation failed");
			break;
		}

		dbus_message_append_args(msg, DBUS_TYPE_INT32,
						&response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		break;
	}

	case BT_EVENT_SYSTEM_RESET_REQUEST:

		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		__unlock_display();

		break;

	default:
		break;
	}

	__bluetooth_win_del(ad);
}

static int __bluetooth_request_timeout_cb(void *data)
{
	struct bt_popup_appdata *ad;

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;

	BT_DBG("Request time out, Canceling reqeust");

	/* Destory UI and timer */
	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	__bluetooth_remove_all_event(ad);
	return 0;
}

static void __bluetooth_input_cancel_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	bt_device_cancel_bonding();

	__bluetooth_win_del(ad);
}

static void __bluetooth_send_signal_pairing_confirm_result(void *data, int response)
{
	if (data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	DBusMessage *msg = NULL;

	BT_DBG("+");

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
			      BT_SYS_POPUP_INTERFACE,
			      BT_SYS_POPUP_METHOD_RESPONSE);

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	BT_DBG("-");
}

static void __bluetooth_send_signal_reset_confirm_result(void *data, int response)
{
	if (data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	DBusMessage *msg = NULL;
	int enable = 0;

	BT_DBG("+");

	if (ad->popup_check != NULL)
		enable = elm_check_state_get(ad->popup_check);

	BT_DBG("response: %d enable: %d", response, enable);
	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
			      BT_SYS_POPUP_INTERFACE,
			      BT_SYS_POPUP_METHOD_RESET_RESPONSE);

	dbus_message_append_args(msg,
				 DBUS_TYPE_INT32, &response,
				 DBUS_TYPE_INT32, &enable,
				 DBUS_TYPE_INVALID);

	e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
	dbus_message_unref(msg);

	BT_DBG("-");
}

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *style = elm_object_style_get(obj);
	if (!g_strcmp0(style, "popup/circle/right")) {
		BT_INFO("Accept the pairing passkey");
		__bluetooth_send_signal_pairing_confirm_result(ad, 1);
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		BT_INFO("Reject the pairing passkey");
		__bluetooth_send_signal_pairing_confirm_result(ad, 0);
		dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	}
	__unlock_display();

	evas_object_del(obj);
	__bluetooth_win_del(ad);
}

void __bluetooth_device_connection_state_changed_cb(bool connected,
						bt_device_connection_info_s *conn_info,
						void *user_data)
{
	struct bt_popup_appdata *ad;

	ad = (struct bt_popup_appdata *)user_data;
	if (ad == NULL)
		return;

	BT_DBG("__bluetooth_device_connection_state_changed_cb [%d]", connected);
	BT_DBG("address [%s]", conn_info->remote_address);
	BT_DBG("link type [%d]", conn_info->link);
	BT_DBG("disconnection reason [%d]", conn_info->disconn_reason);

	if (!connected && ad->popup) {
		evas_object_del(ad->popup);
		__bluetooth_win_del(ad);
	}

	__unlock_display();
}

void __bluetooth_device_bond_created_cb(int result, bt_device_info_s *device_info, void *user_data)
{
	struct bt_popup_appdata *ad;

	ad = (struct bt_popup_appdata *)user_data;
	if (ad == NULL)
		return;

	if(result == BT_ERROR_NONE) {
		BT_DBG("A bond is created.");
		BT_DBG("%s, %s", device_info->remote_name, device_info->remote_address);
	} else {
		BT_ERR("Creating a bond is failed.");
	}

	if (ad->popup) {
		evas_object_del(ad->popup);
		__bluetooth_win_del(ad);
	}

	__unlock_display();
}

static void __bluetooth_reset_cb(void *data, Evas_Object *obj, void *event_info)
{
	int reset = 0;

	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *style = elm_object_style_get(obj);

	__bluetooth_send_signal_pairing_confirm_result(ad, 0);
	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyConfirmation",
				   G_TYPE_UINT, BT_AGENT_CANCEL,
				   G_TYPE_INVALID, G_TYPE_INVALID);

	if (!g_strcmp0(style, "popup/circle/right")) {
		BT_INFO("Confirm Soft reset");
		reset = 1;
	}
	__bluetooth_send_signal_reset_confirm_result(ad, reset);

	__unlock_display();

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
		BT_ERR("e_dbus_bus_get failed");
		return FALSE;
	}

	BT_DBG("e_dbus_bus_get success  ");
	return TRUE;
}

static void __bluetooth_authorization_request_cb(void *data,
					       Evas_Object *obj,
					       void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	guint reply_val;
	int enable = 0;
	const char *style;

	if (obj == NULL || ad == NULL)
		return;

	if (ad->popup_check != NULL)
		enable = elm_check_state_get(ad->popup_check);

	style = elm_object_style_get(obj);
	if (!g_strcmp0(style, "popup/circle/right")) {
		reply_val = enable ? BT_AGENT_ACCEPT_ALWAYS : BT_AGENT_ACCEPT;
	} else {
		reply_val = BT_AGENT_CANCEL;
	}

	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
		G_TYPE_UINT, reply_val,
		G_TYPE_INVALID, G_TYPE_INVALID);

	evas_object_del(obj);
	__bluetooth_win_del(ad);
}

static void __bluetooth_ime_hide(void)
{
	Ecore_IMF_Context *imf_context = NULL;
	imf_context = ecore_imf_context_add(ecore_imf_context_default_id_get());
	if (imf_context)
		ecore_imf_context_input_panel_hide(imf_context);
}

static void __bluetooth_draw_auth_popup(struct bt_popup_appdata *ad,
			const char *msg, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char *txt = NULL;
	Evas_Object *btn;
	Evas_Object *icon;
	Evas_Object *layout;
	Evas_Object *layout_inner;
	Evas_Object *label;
	Evas_Object *check;
	Evas_Object *ao = NULL;
	BT_DBG("+");

	ret_if(!ad);
	ret_if(!msg);

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}
	elm_object_style_set(ad->popup, "circle");
//	uxt_popup_set_rotary_event_enabled(ad->popup, EINA_TRUE);
	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK, __bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed", __bluetooth_popup_hide_finished_cb, NULL);

	layout = elm_layout_add(ad->popup);
	elm_layout_theme_set(layout, "layout", "popup", "content/circle/buttons2");
	elm_object_content_set(ad->popup, layout);

	layout_inner = elm_layout_add(layout);
	elm_layout_file_set(layout_inner, CUSTOM_POPUP_PATH, "popup_checkview_internal");
	evas_object_size_hint_weight_set(layout_inner, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "elm.swallow.content", layout_inner);

	txt = elm_entry_utf8_to_markup(msg);

	label = elm_label_add(layout_inner);
	elm_object_style_set(label, "popup/default");
	elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
	elm_object_text_set(label, txt);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_part_content_set(layout_inner, "label", label);

	check = elm_check_add(ad->popup);
	ad->popup_check = check;
	elm_object_style_set(check, "popup");
	elm_object_text_set(check, BT_STR_ALWAYS_ALLOW);
	elm_object_part_content_set(layout_inner, "check", check);
	evas_object_show(check);

	evas_object_show(check);
	evas_object_show(layout_inner);
	evas_object_show(layout);
	elm_object_content_set(ad->popup, layout);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn , "popup/circle/left");
	elm_object_part_content_set(ad->popup, "button1", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_delete.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_CANCEL);
	evas_object_show(icon);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn , "popup/circle/right");
	elm_object_part_content_set(ad->popup, "button2", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_check.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_OK);
	evas_object_show(icon);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	ao = elm_access_object_register(ad->popup, ad->win_main);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, txt);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	if (txt)
		free(txt);

	BT_DBG("-");
}

static void __bluetooth_draw_reset_popup(struct bt_popup_appdata *ad,
			const char *msg, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char *txt = NULL;
	Evas_Object *btn;
	Evas_Object *icon;
	Evas_Object *layout;
	Evas_Object *ao = NULL;
	Evas_Object *layout_inner = NULL;
	Evas_Object *check = NULL;
	Evas_Object *label = NULL;
	BT_DBG("+");

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}
	elm_object_style_set(ad->popup, "circle");
//	uxt_popup_set_rotary_event_enabled(ad->popup, EINA_TRUE);
	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK, __bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed", __bluetooth_popup_hide_finished_cb, NULL);

	layout = elm_layout_add(ad->popup);
	elm_layout_theme_set(layout, "layout", "popup", "content/circle/buttons2");
	elm_object_content_set(ad->popup, layout);

	layout_inner = elm_layout_add(layout);
	elm_layout_file_set(layout_inner, CUSTOM_POPUP_PATH, "popup_checkview_internal");
	evas_object_size_hint_weight_set(layout_inner, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(layout, "elm.swallow.content", layout_inner);

	if (msg != NULL)
		txt = elm_entry_utf8_to_markup(msg);

	label = elm_label_add(layout_inner);
	elm_object_style_set(label, "popup/default");
	elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
	elm_object_text_set(label, txt);
	evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_part_content_set(layout_inner, "label", label);

	check = elm_check_add(ad->popup);
	ad->popup_check = check;
	elm_object_style_set(check, "popup");
	elm_object_text_set(check, _("Factory reset"));
	elm_object_part_content_set(layout_inner, "check", check);
	evas_object_show(check);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn, "popup/circle/left");
	elm_object_part_content_set(ad->popup, "button1", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_delete.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_CANCEL);
	evas_object_show(icon);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn, "popup/circle/right");
	elm_object_part_content_set(ad->popup, "button2", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_check.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_OK);
	evas_object_show(icon);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	ao = elm_access_object_register(ad->popup, ad->win_main);
	if (ao != NULL) {
		elm_access_info_set(ao, ELM_ACCESS_INFO, msg);
	} else {
		BT_ERR("elm_access_object_register error!");
	}

	if (txt != NULL)
		free(txt);
	BT_DBG("-");
}

char *__bluetooth_convert_rgba_to_hex(int r, int g, int b, int a)
{
	int hexcolor = 0;
	char* string = NULL;

	string = g_try_malloc0(sizeof(char )* 255);
	if (string == NULL)
		return NULL;

	hexcolor = (r << 24) + (g << 16) + (b << 8) + a;
	sprintf(string, "%08x", hexcolor );

	return string;
}


static void __bluetooth_draw_passkey_display_popup(struct bt_popup_appdata *ad,
			const char *text, Evas_Smart_Cb func, void *data)
{
	FN_START
	Evas_Object *btn;
	Evas_Object *icon;
	Evas_Object *layout;

	ret_if(!ad);

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}
	elm_object_style_set(ad->popup, "circle");
//	uxt_popup_set_rotary_event_enabled(ad->popup, EINA_TRUE);
	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK,
			__bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed",
			__bluetooth_popup_hide_finished_cb, NULL);

	__bluetooth_set_win_level(ad->popup);
	layout = elm_layout_add(ad->popup);
	elm_layout_theme_set(layout, "layout", "popup",
			"content/circle/buttons1");

	elm_object_part_text_set(layout, "elm.text", text);
	elm_object_content_set(ad->popup, layout);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn , "popup/circle");
	elm_object_part_content_set(ad->popup, "button1", btn);
	evas_object_smart_callback_add(btn , "clicked", func, data);

	icon = elm_image_add(btn);
	elm_image_file_set(icon,
			POPUP_IMAGE_PATH"/tw_ic_popup_btn_delete.png", NULL);
	evas_object_size_hint_weight_set(icon,
			EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_CANCEL);
	evas_object_show(icon);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);
	FN_END
}

static void __bluetooth_draw_text_popup(struct bt_popup_appdata *ad,
			const char *text,
			void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	FN_START
	Evas_Object *btn;
	Evas_Object *icon;
	Evas_Object *layout;

	ret_if(!ad);

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}
	elm_object_style_set(ad->popup, "circle");
//	uxt_popup_set_rotary_event_enabled(ad->popup, EINA_TRUE);
	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK, __bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed", __bluetooth_popup_hide_finished_cb, NULL);

	__bluetooth_set_win_level(ad->popup);
	layout = elm_layout_add(ad->popup);
	elm_layout_theme_set(layout, "layout", "popup", "content/circle/buttons2");

	elm_object_part_text_set(layout, "elm.text", text);
	elm_object_content_set(ad->popup, layout);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn , "popup/circle/left");
	elm_object_part_content_set(ad->popup, "button1", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_delete.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_CANCEL);
	evas_object_show(icon);

	btn = elm_button_add(ad->popup);
	elm_object_style_set(btn , "popup/circle/right");
	elm_object_part_content_set(ad->popup, "button2", btn);
	evas_object_smart_callback_add(btn , "clicked", func, ad);

	icon = elm_image_add(btn);
	elm_image_file_set(icon, POPUP_IMAGE_PATH"/tw_ic_popup_btn_check.png", NULL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_content_set(btn, "elm.swallow.content", icon);
	elm_access_info_set(btn, ELM_ACCESS_INFO, BT_STR_OK);
	evas_object_show(icon);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);
	FN_END
}

static void __bluetooth_draw_text_popup_no_button(struct bt_popup_appdata *ad,
			const char *device_name, const char *passkey)
{
	FN_START
	Evas_Object *layout = NULL;
	char *text1 = NULL;
	char *text2 = NULL;

	ret_if(!ad);

	ad->popup = elm_popup_add(ad->win_main);
	if (ad->popup == NULL) {
		BT_ERR("elm_popup_add is failed");
		return;
	}

	elm_object_style_set(ad->popup, "circle");
//	uxt_popup_set_rotary_event_enabled(ad->popup, EINA_TRUE);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK, __bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed", __bluetooth_popup_hide_finished_cb, NULL);

	__bluetooth_set_win_level(ad->popup);

	layout = elm_layout_add(ad->popup);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "no_button_passkey_confirm_popup");
	elm_object_content_set(ad->popup, layout);

	text1 = g_strdup_printf(BT_STR_GEAR_WILL_CONNECT_WITH_PS, device_name);
	text2 = g_strdup_printf("%s %s", BT_STR_PASSKEY, passkey);

	elm_object_part_text_set(layout, "elm.text1", text1);
	elm_object_part_text_set(layout, "elm.text2", text2);

	g_free(text1);
	g_free(text2);
	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);
	FN_END
}

static Eina_Bool __bluetooth_pop_cb(void *data, Elm_Object_Item *it)
{
	FN_START;
	retv_if(data == NULL, EINA_FALSE);
	struct bt_popup_appdata *ad = data;
	__bluetooth_win_del(ad);
	FN_END;
	return EINA_FALSE;
}

static void __bluetooth_editfield_del_cb(void *data, Evas *e,
			       Evas_Object *obj, void *event_info)
{
	FN_START;
	Evas_Object *editfield = data;
	Ecore_IMF_Context *imf_context =
		(Ecore_IMF_Context *)elm_entry_imf_context_get(editfield);
	if (imf_context) {
		ecore_imf_context_input_panel_event_callback_clear(imf_context);
	}
	FN_END;
}

void __bluetooth_layout_wearable_input_pop(bt_pincode_input_object *po)
{
	FN_START;
	ret_if (!po);
	ret_if (!po->naviframe);

	elm_naviframe_item_pop(po->naviframe);
	if (po->pincode && strlen(po->pincode) > 0 && po->ok_btn) {
		BT_DBG("po->pincode : %s, strlen(po->pincode) : %d", po->pincode, strlen(po->pincode));
		elm_object_disabled_set(po->ok_btn, EINA_FALSE);
	}

	FN_END;
}

gboolean __bluetooth_view_base_window_is_focus(bt_pincode_input_object *po)
{
	FN_START;
	Eina_Bool ret;
	retv_if (!po, FALSE);
	retv_if (!po->win_main, FALSE);

	ret = elm_win_focus_get(po->win_main);
	return ret == EINA_TRUE ? TRUE : FALSE;
	FN_END;
}


static void __bluetooth_editfield_state_changed_cb(void *data, Ecore_IMF_Context *ctx, int value)
{
	FN_START;
	bt_pincode_input_object *po = data;

	ret_if (!po);

	if (value == ECORE_IMF_INPUT_PANEL_STATE_HIDE) {
		if (__bluetooth_view_base_window_is_focus(po)) {
			BT_INFO("Key pad is now closed by user");
			__bluetooth_layout_wearable_input_pop(po);
		} else {
			BT_INFO("Keypad is now closed by other window show");
		}
	}
	FN_END;
}

static void __bluetooth_editfield_focused_cb(void *data, Evas_Object *obj,
				void *event_info)
{
	FN_START;
	elm_entry_cursor_end_set(obj);
	FN_END;
}

static void __bluetooth_editfield_changed_cb(void *data,
						       Evas_Object *obj, void *event_info)
{
	FN_START;
	bt_pincode_input_object *po = data;
	ret_if(!po);
	ret_if(!obj);
	const char *pincode = elm_entry_entry_get(obj);

	BT_DBG("pincode : %s", pincode);
	if (pincode && strlen(pincode) > 0) {
		elm_entry_input_panel_return_key_disabled_set(po->editfield_keypad,
							EINA_FALSE);
		if (po->ok_btn)
			elm_object_disabled_set(po->ok_btn, EINA_FALSE);
	} else {
		elm_entry_input_panel_return_key_disabled_set(po->editfield_keypad,
							EINA_TRUE);
		if (po->ok_btn)
			elm_object_disabled_set(po->ok_btn, EINA_TRUE);
	}
	FN_END;
}

static void __bluetooth_editfield_keydown_cb(void *data, Evas *e, Evas_Object *obj,
							void *event_info)
{
	Evas_Event_Key_Down *ev = NULL;
	Evas_Object *editfield = obj;
	const char *pincode = NULL;

	ret_if(data == NULL);
	ret_if(event_info == NULL);
	ret_if(editfield == NULL);

	bt_pincode_input_object *po = data;
	ret_if(!po);

	ev = (Evas_Event_Key_Down *)event_info;
	BT_INFO("ENTER ev->key:%s", ev->key);

	if (g_strcmp0(ev->key, "KP_Enter") == 0 ||
			g_strcmp0(ev->key, "Return") == 0) {

		Ecore_IMF_Context *imf_context = NULL;

		pincode = elm_entry_entry_get(editfield);
		if (po->pincode)
			g_free(po->pincode);
		po->pincode = g_strdup(pincode);

		imf_context =
			(Ecore_IMF_Context*)elm_entry_imf_context_get(editfield);
		if (imf_context)
			ecore_imf_context_input_panel_hide(imf_context);

		//elm_object_focus_set(editfield, EINA_FALSE);
	}
}

static Evas_Object *__bluetooth_create_editfield(bt_pincode_input_object *po)
{
	FN_START;
	retv_if(!po, NULL);

	Evas_Object *editfield = NULL;
	Ecore_IMF_Context *imf_context = NULL;
	Elm_Entry_Filter_Limit_Size limit_filter_data;

#if 0
	editfield = uxt_edit_field_add(po->layout_keypad, EA_EDITFIELD_SCROLL_SINGLELINE_PASSWORD);
	retvm_if(!editfield, NULL, "fail to add editfield!");
#endif
	editfield = elm_entry_add(po->layout_keypad);
	elm_entry_single_line_set(editfield, EINA_TRUE);
	elm_entry_scrollable_set(editfield, EINA_TRUE);

	eext_entry_selection_back_event_allow_set(editfield, EINA_TRUE);
//	uxt_edit_field_set_clear_button_enabled(editfield, EINA_FALSE);
	elm_entry_prediction_allow_set(editfield, EINA_FALSE);
	elm_entry_input_panel_layout_set(editfield, ELM_INPUT_PANEL_LAYOUT_NUMBER);
	elm_entry_input_panel_return_key_type_set(editfield, ELM_INPUT_PANEL_RETURN_KEY_TYPE_DONE);

	if (po->input_guide_text)
		elm_object_part_text_set(editfield, "elm.guide", po->input_guide_text);
	if (po->input_text)
		elm_object_part_text_set(editfield, "elm.text", po->input_text);

	elm_object_part_content_set(po->layout_keypad, "elm.swallow.content", editfield);

	imf_context = (Ecore_IMF_Context *)elm_entry_imf_context_get(editfield);
	if (imf_context) {
		ecore_imf_context_input_panel_event_callback_add(imf_context,
								 ECORE_IMF_INPUT_PANEL_STATE_EVENT,
								 __bluetooth_editfield_state_changed_cb, po);
	}
	evas_object_event_callback_add(editfield, EVAS_CALLBACK_DEL,
				       __bluetooth_editfield_del_cb, editfield);
	evas_object_smart_callback_add(editfield, "focused",
				       __bluetooth_editfield_focused_cb, NULL);
	evas_object_smart_callback_add(editfield, "changed",
					__bluetooth_editfield_changed_cb, po);
	evas_object_event_callback_add(editfield, EVAS_CALLBACK_KEY_DOWN,
					__bluetooth_editfield_keydown_cb, po);

	limit_filter_data.max_char_count = 16;
	elm_entry_markup_filter_append(editfield, elm_entry_filter_limit_size, &limit_filter_data);

	FN_END;
	return editfield;
}

static Evas_Object *__bluetooth_create_conformant(Evas_Object *parent)
{
	FN_START;
	retv_if(!parent, NULL);
	Evas_Object *conformant = elm_conformant_add(parent);
	retvm_if(!conformant, NULL, "elm_conformant_add is failed!");

	evas_object_size_hint_weight_set(conformant, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_layout_theme_set(conformant, "conformant", "base", "without_resize");
	FN_END;
	return conformant;
}

static Evas_Object *__bluetooth_create_naviframe(Evas_Object *parent)
{
	FN_START;
	Evas_Object *naviframe = elm_naviframe_add(parent);
	if (naviframe) {
		evas_object_size_hint_weight_set(naviframe, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		eext_object_event_callback_add(naviframe, EEXT_CALLBACK_BACK, eext_naviframe_back_cb, NULL);
	}
	FN_END;
	return naviframe;
}

static Eext_Circle_Surface *__bluetooth_create_circle_surface_from_naviframe(Evas_Object *naviframe)
{
	FN_START;
	Eext_Circle_Surface *circle_surface;
	retv_if(!naviframe, NULL);

	circle_surface = eext_circle_surface_naviframe_add(naviframe);
	FN_END;
	return circle_surface;
}

static void __bluetooth_pincode_btn_cb(void *data, Evas_Object *obj,
					void *event_info)
{
	FN_START;
	retm_if(data == NULL, "data is NULL!");
	struct bt_popup_appdata *ad = data;
	retm_if(ad == NULL, "ad is NULL!");
	bt_pincode_input_object *po = ad->po;
	retm_if(po == NULL, "ad is NULL!");

	if(ad->event_type == BT_EVENT_PIN_REQUEST) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					"ReplyPinCode", G_TYPE_UINT, BT_AGENT_ACCEPT,
					G_TYPE_STRING, po->pincode,
					G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy,
					"ReplyPasskey", G_TYPE_UINT, BT_AGENT_ACCEPT,
					G_TYPE_STRING, po->pincode,
					G_TYPE_INVALID, G_TYPE_INVALID);
	}

	if (po->pincode) {
		g_free(po->pincode);
		po->pincode = NULL;
	}
	__bluetooth_win_del(ad);
	FN_END;
}

static char *__bluetooth_gl_pairing_title_label_get(void *data, Evas_Object *obj, const char *part)
{
	if (g_strcmp0(part, "elm.text") == 0) {
		FN_END;
		return g_strdup(BT_STR_PAIRING_REQUEST);
	}
	return NULL;
}

/*
static Evas_Object *__bluetooth_gl_pairing_editfield_get(void *data, Evas_Object *obj, const char *part)
{
	FN_START;
	retvm_if(data == NULL, NULL,  "data is NULL!");
	bt_pincode_input_object *po = data;
	retvm_if(po->genlist == NULL, NULL,  "ad->genlist is NULL!");
	retv_if(strcmp(part, "elm.icon") != 0, NULL);
	Evas_Object *layout = NULL;

	layout = __bluetooth_create_editfield(po);

	FN_END;
	return layout;
}
*/

static char *__bluetooth_gl_pairing_enter_pin_label_get(void *data, Evas_Object *obj, const char *part)
{
	if (g_strcmp0(part, "elm.text") == 0) {
		FN_END;
		return g_strdup_printf("<align=center>%s</align>", BT_STR_ENTER_PIN);
	}
	return NULL;
}

static char *__bluetooth_gl_pairing_description_label_get(void *data, Evas_Object *obj, const char *part)
{
	retv_if(!data, NULL);
	char *dev_name = (char *)data;
	char temp[100] = {'\0',};

	if (!strcmp(part, "elm.text")) {
		snprintf(temp, sizeof(temp) - 1, BT_STR_ENTER_PIN_TO_PAIR_WITH_PS, dev_name);
		return g_strdup_printf("<align=center>%s</align>", temp);
	}
	return NULL;
}

static Evas_Object *__bluetooth_create_pin_code_genlist(
						bt_pincode_input_object *po)
{
	FN_START;
	retv_if(!po, NULL);
	retv_if(!po->win_main, NULL);
	Evas_Object *genlist = NULL;
	Evas_Object *circle_genlist = NULL;

	genlist = elm_genlist_add(po->win_main);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);
	circle_genlist = eext_circle_object_genlist_add(genlist, po->circle_surface);
	po->circle_genlist = circle_genlist;
	eext_circle_object_genlist_scroller_policy_set(circle_genlist,
			ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
	eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);
//	uxt_genlist_set_bottom_margin_enabled(genlist, EINA_TRUE);

	FN_END;
	return genlist;
}

bt_pincode_input_object *__bluetooth_pincode_input_new(struct bt_popup_appdata *ad)
{
	FN_START;
	retv_if(!ad, NULL);
	bt_pincode_input_object *object = NULL;
	object = g_new0(bt_pincode_input_object, 1);
	retvm_if (!object, NULL, "layout_wearable_input_new() failed.");
	object->win_main = ad->win_main;
	FN_END;
	return object;
}

gboolean __bluetooth_pincode_input_create(bt_pincode_input_object *po)
{
	FN_START;
	retv_if(!po, FALSE);
	retv_if(!po->naviframe, FALSE);
	Evas_Object *naviframe = NULL;
	Evas_Object *layout = NULL;
	Evas_Object *editfield = NULL;
	Elm_Object_Item *git = NULL;

	naviframe = po->naviframe;

	layout = elm_layout_add(naviframe);
	retv_if(!layout, FALSE);

	if (!elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "entry_layout")) {
		BT_ERR("elm_layout_file_set() is failed.");
		evas_object_del(layout);
		return FALSE;
	}
	po->layout_keypad = layout;

	editfield = __bluetooth_create_editfield(po);
	if (!editfield) {
		__bluetooth_win_del(po);
		return FALSE;
	}
	elm_object_part_content_set(layout,
				    "elm.swallow.content", editfield);
	elm_object_focus_set(editfield, EINA_TRUE);

	po->editfield_keypad = editfield;

	git = elm_naviframe_item_push(po->naviframe, NULL, NULL, NULL, layout, NULL);
	retv_if(!git, FALSE);
	po->naviframe_item = git;

	elm_naviframe_item_title_enabled_set(git, EINA_FALSE, EINA_FALSE);

	FN_END;
	return TRUE;
}

static void __bluetooth_gl_enter_pin_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
	FN_START;
	retm_if(data == NULL, "data is NULL!");
	struct bt_popup_appdata *ad = data;
	retm_if(ad == NULL, "ad is NULL!");
	bt_pincode_input_object *po = ad->po;
	retm_if(po == NULL, "po is NULL!");
	Elm_Object_Item *item = event_info;
	retm_if(item == NULL, "item is NULL!");

	elm_genlist_item_selected_set(item, EINA_FALSE);

	if (__bluetooth_pincode_input_create(po) == FALSE) {
		BT_ERR("__bluetooth_pincode_input_create fail!");
		__bluetooth_win_del(ad);
	}
	FN_END;
}

static void __bluetooth_draw_input_view(struct bt_popup_appdata *ad,
			const char *title, const char *dev_name)
{
	FN_START;
	Evas_Object *bg = NULL;
	Evas_Object *conformant = NULL;
	Evas_Object *naviframe = NULL;
	Eext_Circle_Surface *circle_surface = NULL;
	Evas_Object *layout_main = NULL;
	Evas_Object *layout = NULL;
	Elm_Object_Item *navi_it = NULL;
	Evas_Object *button = NULL;
	Elm_Object_Item *git = NULL;
	bt_pincode_input_object *po = NULL;
	ret_if(!ad);
	ret_if(!ad->win_main);
	ret_if(!dev_name);

	po = __bluetooth_pincode_input_new(ad);
	if (!po) {
		BT_ERR("__bluetooth_pincode_input_new() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	ad->po = po;

	/* bg */
	bg = elm_bg_add(ad->win_main);
	if (!bg) {
		BT_ERR("elm_bg_add() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win_main, bg);
	evas_object_show(bg);
	po->bg = bg;

	/* conformant */
	conformant = __bluetooth_create_conformant(ad->win_main);
	if (!conformant) {
		BT_ERR("__bluetooth_create_conformant() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	elm_win_resize_object_add(ad->win_main, conformant);
	elm_win_conformant_set(ad->win_main, EINA_TRUE);
	evas_object_show(conformant);
	po->conformant = conformant;

	/* layout_main */
	layout_main = elm_layout_add(conformant);
	if (!layout_main) {
		BT_ERR("elm_layout_add() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	elm_layout_theme_set(layout_main, "layout", "application", "default");
	evas_object_size_hint_weight_set(layout_main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_content_set(conformant, layout_main);
	po->layout_main = layout_main;

	/* naviframe */
	naviframe = __bluetooth_create_naviframe(layout_main);
	if (!naviframe) {
		BT_ERR("__bluetooth_create_naviframe() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	elm_object_part_content_set(layout_main, "elm.swallow.content", naviframe);
	evas_object_show(naviframe);
	po->naviframe = naviframe;

	/* circle_surface */
	circle_surface = __bluetooth_create_circle_surface_from_naviframe(naviframe);
	if (!circle_surface) {
		BT_ERR("_create_circle_surface_from_conformant() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	po->circle_surface = circle_surface;

	/* Bottom button layout */
	layout = elm_layout_add(naviframe);
	elm_layout_theme_set(layout, "layout", "bottom_button", "default");
	evas_object_size_hint_weight_set (layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(layout);

	/* Create genlist */
	po->genlist = __bluetooth_create_pin_code_genlist(po);

	/* Title item class */
	Elm_Genlist_Item_Class *itc_pairing_title = elm_genlist_item_class_new();
	itc_pairing_title->item_style = "title";
	itc_pairing_title->func.text_get = __bluetooth_gl_pairing_title_label_get;
	itc_pairing_title->func.content_get = NULL;
	itc_pairing_title->func.del = NULL;

	/* Pairing editfield item class */
	if (po->pairing_editfield_itc == NULL) {
		po->pairing_editfield_itc = elm_genlist_item_class_new();
		po->pairing_editfield_itc->item_style = "1text";
		po->pairing_editfield_itc->func.text_get = __bluetooth_gl_pairing_enter_pin_label_get;
		po->pairing_editfield_itc->func.content_get = NULL;//__bluetooth_gl_pairing_editfield_get;
		po->pairing_editfield_itc->func.del = NULL;
	}

	/* Pairing description item class */
	Elm_Genlist_Item_Class *itc_pairing_description = elm_genlist_item_class_new();
	itc_pairing_description->item_style = "multiline";
	itc_pairing_description->func.text_get = __bluetooth_gl_pairing_description_label_get;
	itc_pairing_description->func.content_get = NULL;
	itc_pairing_description->func.del = NULL;

	/* append items to genlist */
	git = elm_genlist_item_append(po->genlist,
			itc_pairing_title, po, NULL, ELM_GENLIST_ITEM_NONE,
			NULL, NULL);

	elm_genlist_item_select_mode_set(git,
					 ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

	git = elm_genlist_item_append(po->genlist,
			po->pairing_editfield_itc, po, NULL, ELM_GENLIST_ITEM_NONE,
			__bluetooth_gl_enter_pin_clicked_cb, ad);

	git = elm_genlist_item_append(po->genlist,
			itc_pairing_description, elm_entry_utf8_to_markup(dev_name), NULL, ELM_GENLIST_ITEM_NONE,
			NULL, NULL);

	elm_genlist_item_select_mode_set(git,
					 ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);

	elm_object_part_content_set(layout, "elm.swallow.content",
			po->genlist);
	/* Push naviframe item */
	navi_it = elm_naviframe_item_push(naviframe, NULL, NULL,
				NULL, layout, "empty");

	elm_naviframe_prev_btn_auto_pushed_set(naviframe, EINA_FALSE);
	elm_naviframe_item_pop_cb_set(navi_it, __bluetooth_pop_cb, ad);
	po->naviframe_item = navi_it;

	/* OK button */
	button = elm_button_add(layout);
	if (!button) {
		BT_ERR("elm_button_add() is failed.");
		__bluetooth_win_del(ad);
		return;
	}
	elm_object_style_set(button, "bottom");
	evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_smart_callback_add(button, "clicked", __bluetooth_pincode_btn_cb, ad);
	elm_object_text_set(button, "OK");
	elm_object_disabled_set(button, EINA_TRUE);
	evas_object_show(button);
	po->ok_btn = button;

	elm_object_part_content_set(layout, "elm.swallow.button", button);
	FN_END;
}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	__bluetooth_ime_hide();
}

static DBusGProxy* __bluetooth_create_agent_proxy(DBusGConnection *conn,
								const char *path)
{
	DBusGProxy *proxy;

	proxy = dbus_g_proxy_new_for_name(conn, "org.projectx.bt",
			path, "org.bluez.Agent1");
	if (!proxy)
		BT_ERR("dbus_g_proxy_new_for_name is failed");

	return proxy;

}

/* AUL bundle handler */
static int __bluetooth_launch_handler(struct bt_popup_appdata *ad,
					void *reset_data)
{
	bundle *kb = (bundle *)reset_data;
	char view_title[BT_TITLE_STR_MAX_LEN] = { 0 };
	char text[BT_GLOBALIZATION_STR_LENGTH] = { 0 };
	int timeout = 0;
	const char *device_name = NULL;
	const char *passkey = NULL;
	const char *agent_path;
	char *conv_str = NULL;

	if (!kb) {
		BT_ERR("Bundle is NULL");
		return -1;
	}

	BT_INFO("Event Type = [0x%X]", ad->event_type);

	switch (ad->event_type) {
	case BT_EVENT_PIN_REQUEST:
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, conv_str);

		if (conv_str)
			free(conv_str);
		break;

	case BT_EVENT_PASSKEY_CONFIRM_REQUEST:
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS,
			     conv_str, passkey);
			if (conv_str)
				free(conv_str);
			BT_INFO("title: %s", view_title);

			__bluetooth_draw_text_popup(ad, view_title,
					__bluetooth_passkey_confirm_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
		break;

	case BT_EVENT_PASSKEY_AUTO_ACCEPTED: {
		int ret;
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		ret =  bt_device_set_connection_state_changed_cb(
				__bluetooth_device_connection_state_changed_cb, ad);
		if (ret != BT_ERROR_NONE)
			return -1;

		ret = bt_device_set_bond_created_cb(__bluetooth_device_bond_created_cb, ad);
		if (ret != BT_ERROR_NONE)
			return -1;

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			__bluetooth_draw_text_popup_no_button(ad, conv_str, passkey);
			if (conv_str)
				free(conv_str);

			__bluetooth_send_signal_pairing_confirm_result(ad, 1);
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					"ReplyConfirmation",
					G_TYPE_UINT, BT_AGENT_ACCEPT,
					G_TYPE_INVALID, G_TYPE_INVALID);
		}
		break;
	}

	case BT_EVENT_PASSKEY_REQUEST: {
		const char *device_name = NULL;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			 "%s", BT_STR_PAIRING_REQUEST);

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			 BT_STR_ENTER_PIN_TO_PAIR, conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, conv_str);

		if (conv_str)
			free(conv_str);
		break;
	}

	case BT_EVENT_PASSKEY_DISPLAY_REQUEST:
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_passkey_display_popup(ad, view_title,
					__bluetooth_input_cancel_cb, ad);
		} else {
			BT_ERR("wrong parameter : %s, %s", device_name, passkey);
			timeout = BT_ERROR_TIMEOUT;
		}
		break;

	case BT_EVENT_AUTHORIZE_REQUEST:
		timeout = BT_AUTHORIZATION_TIMEOUT;

		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				BT_STR_ALLOW_PS_TO_CONNECT_Q, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_auth_popup(ad, view_title,
				__bluetooth_authorization_request_cb);
		break;

	case BT_EVENT_KEYBOARD_PASSKEY_REQUEST:
		device_name = bundle_get_val(kb, "device-name");
		passkey = bundle_get_val(kb, "passkey");

		if (device_name && passkey) {
			conv_str = elm_entry_utf8_to_markup(device_name);

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			     BT_STR_ENTER_PS_ON_PS_TO_PAIR, passkey, conv_str);

			if (conv_str)
				free(conv_str);

			__bluetooth_draw_passkey_display_popup(ad, view_title,
					__bluetooth_input_cancel_cb, ad);
		} else {
			BT_ERR("wrong parameter : %s, %s", device_name, passkey);
			timeout = BT_ERROR_TIMEOUT;
		}
		break;

	case BT_EVENT_UNABLE_TO_PAIRING:
		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bluetooth_draw_toast_popup(ad, BT_STR_UNABLE_TO_CONNECT);
		break;

	case BT_EVENT_HANDSFREE_AUTO_CONNECT_REQUEST:
		timeout = BT_TOAST_NOTIFICATION_TIMEOUT;
		__bluetooth_draw_toast_popup(ad, BT_STR_CONNECTING);
		break;

	case BT_EVENT_SYSTEM_RESET_REQUEST:
		device_name = bundle_get_val(kb, "device-name");
		agent_path = bundle_get_val(kb, "agent-path");

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
					BT_STR_RESET, device_name, device_name);
			__bluetooth_draw_reset_popup(ad, view_title,
					__bluetooth_reset_cb);
		} else {
			BT_ERR("device name NULL");
			timeout = BT_ERROR_TIMEOUT;
		}
		break;

	default:
		BT_ERR("Unknown event_type : %s", ad->event_type);
		return -1;
	}

	if (ad->event_type != BT_EVENT_FILE_RECEIVED && timeout != 0) {
		ad->timer = ecore_timer_add(timeout,
				(Ecore_Task_Cb)__bluetooth_request_timeout_cb, ad);
	}

	BT_INFO("-");

	return 0;
}

static void __bluetooth_timeout_cb(void *data, Evas_Object *obj, void *event_info)
{
	evas_object_del(obj);
}

static void __bluetooth_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text)
{
	Evas_Object *ao = NULL;

	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast/circle");
	elm_popup_orient_set(ad->popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	eext_object_event_callback_add(ad->popup, EEXT_CALLBACK_BACK, __bluetooth_popup_hide_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "dismissed", __bluetooth_popup_hide_finished_cb, NULL);
	evas_object_smart_callback_add(ad->popup, "block,clicked", __bluetooth_popup_block_clicked_cb, NULL);
	elm_object_part_text_set(ad->popup,"elm.text", toast_text);

	__bluetooth_set_win_level(ad->popup);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

#if 0
	ao = elm_object_part_access_object_get(ad->popup, "access.outline");
        if (ao != NULL)
		elm_access_info_set(ao, ELM_ACCESS_INFO, toast_text);
#endif
}

static void __bluetooth_draw_error_toast_popup(struct bt_popup_appdata *ad, char *toast_text)
{
	Evas_Object *ao = NULL;

	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast/circle");
	elm_object_text_set(ad->popup, toast_text);
	elm_popup_timeout_set(ad->popup, 2.0);
	evas_object_smart_callback_add(ad->popup, "timeout", __bluetooth_timeout_cb, NULL);

	__bluetooth_set_win_level(ad->popup);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

#if 0
	ao = elm_object_part_access_object_get(ad->popup, "access.outline");
        if (ao != NULL)
		elm_access_info_set(ao, ELM_ACCESS_INFO, toast_text);
#endif
}

static Eina_Bool __exit_idler_cb(void *data)
{
	elm_exit();
	return ECORE_CALLBACK_CANCEL;
}

static void __popup_terminate(void)
{
	if (ecore_idler_add(__exit_idler_cb, NULL))
		return;

	__exit_idler_cb(NULL);
}

static void __bluetooth_win_del(void *data)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_cleanup(ad);
	__popup_terminate();
}

static int __bluetooth_error_toast_timeout_cb(void *data)
{
	struct bt_popup_appdata *ad;

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;

	BT_DBG("Toast Popup timeout");

	/* Destory toast popup and timer */
	if (ad->timer) {
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}

	if (ad->popup) {
		elm_access_object_unregister(ad->popup);
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}
	return 0;
}
static Evas_Object *__bluetooth_create_win(const char *name, void *data)
{
	Evas_Object *eo;
	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_alpha_set(eo, EINA_TRUE);
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
	}
	evas_object_show(eo);
	return eo;
}

static void __bluetooth_session_init(struct bt_popup_appdata *ad)
{
	DBusGConnection *conn = NULL;
	GError *err = NULL;

	g_type_init();

	conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err);

	if (!conn) {
		BT_ERR("ERROR: Can't get on system bus [%s]",
			     err->message);
		g_error_free(err);
		return;
	}

	ad->conn = conn;

	ad->obex_proxy = dbus_g_proxy_new_for_name(conn,
						   "org.bluez.frwk_agent",
						   "/org/obex/ops_agent",
						   "org.openobex.Agent");
	if (!ad->obex_proxy)
		BT_ERR("Could not create obex dbus proxy");

	if (!__bluetooth_init_app_signal(ad))
		BT_ERR("__bluetooth_init_app_signal failed");
}

void __bluetooth_set_color_table(void *data)
{
	FN_START;
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

#if 0
	/* Set color table */
	ea_theme_changeable_ui_enabled_set(EINA_TRUE);
	ad->color_table = ea_theme_color_table_new(COLOR_TABLE);
	if (ad->color_table == NULL)
		BT_ERR("ea_theme_color_table_new failed!");
	else if (EINA_TRUE != ea_theme_colors_set(ad->color_table, EA_THEME_STYLE_DEFAULT))
		BT_ERR("ea_theme_colors_set failed!");

	ad->font_table = ea_theme_font_table_new(FONT_TABLE);
	if (ad->font_table == NULL)
		BT_ERR("ea_theme_color_table_new failed!");
	else if (EINA_TRUE != ea_theme_fonts_set(ad->font_table))
		BT_ERR("ea_theme_fonts_set failed!");
#endif
	FN_END;
}

static bool __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;

	BT_DBG("__bluetooth_create() start.");

	elm_app_base_scale_set(1.3);
	/* create window */
	win = __bluetooth_create_win(PACKAGE, ad);
	if (win == NULL) {
		BT_ERR("__bluetooth_create_win is failed");
		return false;
	}
	ad->win_main = win;
	/* Handle rotation */
	if (elm_win_wm_rotation_supported_get(ad->win_main)) {
		int rots[4] = {0, 90, 180, 270};
		elm_win_wm_rotation_available_rotations_set(ad->win_main, rots, 4);
	}

	/* init internationalization */
	bindtextdomain(BT_COMMON_PKG, BT_LOCALEDIR);
	textdomain(BT_COMMON_PKG);

	ecore_imf_init();
	__bluetooth_set_color_table(ad);

	__bluetooth_session_init(ad);
	if (bt_initialize() != BT_ERROR_NONE) {
		BT_ERR("bt_initialize is failed");
	}

	return true;
}

static void __bluetooth_terminate(void *data)
{
	BT_DBG("__bluetooth_terminate()");

	struct bt_popup_appdata *ad = data;

	if (bt_deinitialize() != BT_ERROR_NONE) {
		BT_ERR("bt_deinitialize is failed");
	}
	__bluetooth_ime_hide();

	if (ad->conn) {
		dbus_g_connection_unref(ad->conn);
		ad->conn = NULL;
	}

#if 0
	if (ad->color_table != NULL) {
		ea_theme_color_table_free(ad->color_table);
		ad->color_table = NULL;
	}

	if (ad->font_table != NULL) {
		ea_theme_font_table_free(ad->font_table);
		ad->font_table = NULL;
	}
#endif

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;
}

static void __bluetooth_pause(void *data)
{
	BT_DBG("__bluetooth_pause()");
	return;
}

static void __bluetooth_resume(void *data)
{
	BT_DBG("__bluetooth_resume()");
	return;
}

static void __bluetooth_reset(app_control_h app_control, void *user_data)
{
	struct bt_popup_appdata *ad = user_data;
	bundle *b = NULL;
	const char *event_type = NULL;
	int block = 0;
	int ret = 0;

	BT_DBG("__bluetooth_reset()");

	if (ad == NULL) {
		BT_ERR("App data is NULL");
		return;
	}

	ret = app_control_to_bundle(app_control, &b);

	/* Start Main UI */
	event_type = bundle_get_val(b, "event-type");
	if (event_type == NULL) {
		BT_ERR("event type is NULL");
		return;
	}
	BT_INFO("event_type : %s", event_type);

	__bluetooth_parse_event(ad, event_type);

	if (!strcasecmp(event_type, "terminate")) {
		BT_ERR("get terminate event");
		__bluetooth_win_del(ad);
		return;
	}

	if (syspopup_has_popup(b)) {
		/* Destroy the existing popup*/
		BT_ERR("Aleady popup existed");
		__bluetooth_cleanup(ad);

		/* create window */
		ad->win_main = __bluetooth_create_win(PACKAGE, ad);
		if (ad->win_main == NULL) {
			BT_ERR("fail to create win!");
			return;
		}

		ret = syspopup_reset(b);
		if (ret == -1) {
			BT_ERR("syspopup_reset err");
			return;
		}

		goto DONE;
	}

	ret = syspopup_create(b, &handler, ad->win_main, ad);
	if (ret == -1) {
		BT_ERR("syspopup_create err");
		__bluetooth_remove_all_event(ad);
		return;
	}


DONE:
	ret = __bluetooth_launch_handler(ad, b);
	if (ret != 0) {
		BT_ERR("__bluetooth_launch_handler is failed. event[%d], ret[%d]",
				ad->event_type, ret);
		__bluetooth_remove_all_event(ad);
		return;
	}

	if (vconf_get_bool(VCONFKEY_SETAPPL_BLOCKMODE_WEARABLE_BOOL, &block))
		BT_ERR("Get Block Status fail!!");

	if (!block) {
		BT_INFO("Block mode is not set");

		/* Change LCD brightness */
		if (device_display_change_state(DISPLAY_STATE_NORMAL) != 0)
			BT_ERR("Fail to change LCD");

		if (ad->event_type == BT_EVENT_PASSKEY_CONFIRM_REQUEST ||
			   ad->event_type == BT_EVENT_SYSTEM_RESET_REQUEST) {
			__bluetooth_notify_event(FEEDBACK_PATTERN_GENERAL);
			__lock_display();
		} else if (ad->event_type == BT_EVENT_PASSKEY_AUTO_ACCEPTED)
			__lock_display();
	}

	return;
}

EXPORT int main(int argc, char *argv[])
{
	struct bt_popup_appdata ad;
	memset(&ad, 0x0, sizeof(struct bt_popup_appdata));

	ui_app_lifecycle_callback_s callback = {0,};

	callback.create = __bluetooth_create;
	callback.terminate = __bluetooth_terminate;
	callback.pause = __bluetooth_pause;
	callback.resume = __bluetooth_resume;
	callback.app_control = __bluetooth_reset;

	return ui_app_main(&argc, &argv, &callback, &ad);
}

/*
* bt-syspopup
*
* Copyright 2013 Samsung Electronics Co., Ltd
*
* Contact: Hocheol Seo <hocheol.seo@samsung.com>
*           GirishAshok Joshi <girish.joshi@samsung.com>
*           DoHyun Pyun <dh79.pyun@samsung.com>
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
#include <dd-display.h>
#include <app.h>
#include <device/display.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <syspopup.h>
#include <E_DBus.h>
#include <aul.h>
#include <bluetooth.h>
#include <bluetooth_internal.h>
#include <feedback.h>
#include "bt-syspopup-m.h"
#include <notification.h>
#include <bundle.h>
#include <app_control.h>
#include <app_control_internal.h>
#include <efl_assist.h>
#include <efl_extension.h>

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad);
static void __bluetooth_win_del(void *data);

static void __bluetooth_input_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_input_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_keyback_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_mouseup_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info);

static void __bluetooth_terminate(void *data);

static void __bluetooth_remove_all_event(struct bt_popup_appdata *ad);

//static void __bluetooth_set_win_level(Evas_Object *parent);

static void __popup_terminate(void);

static int __bluetooth_term(bundle *b, void *data)
{
	struct bt_popup_appdata *ad;

	BT_DBG("System-popup: terminate");

	if (data == NULL)
		return 0;

	ad = (struct bt_popup_appdata *)data;
	/* Destory UI and timer */
	if (ad->timer) {
		BT_DBG("removing the timer");
		ecore_timer_del(ad->timer);
		ad->timer = NULL;
	}
	__bluetooth_remove_all_event(ad);
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
	ret_if(ad == NULL);

	if (ad->popup) {
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}
	if (ad->layout) {
		evas_object_del(ad->layout);
		ad->layout = NULL;
	}
	if (ad->conform) {
		evas_object_del(ad->conform);
		ad->conform = NULL;
	}
	if (ad->agent_proxy) {
		g_object_unref(ad->agent_proxy);
		ad->agent_proxy = NULL;
	}

	g_free(ad->description);
}

static void __bt_main_win_rot_changed_cb(void *data, Evas_Object *obj,
					 void *event)
{
	FN_START;

	ret_if(!data);

	struct bt_popup_appdata *ad = data;
	int changed_ang = elm_win_rotation_get(obj);
	BT_INFO("New angle: %d, old angle: %d", changed_ang,
				ad->rotation);
	if (changed_ang == ad->rotation) {
		return;
	} else {
		ad->rotation = changed_ang;
	}

	BT_INFO("Rotation: %d", ad->rotation);

	if (ad->popup) {
		if (ad->event_type == BT_EVENT_PIN_REQUEST ||
				ad->event_type == BT_EVENT_PASSKEY_CONFIRM_REQUEST){
			if (ad->rotation == BT_ROTATE_0 ||
					ad->rotation == BT_ROTATE_180)
				elm_layout_file_set(elm_object_content_get(ad->popup),
						CUSTOM_POPUP_PATH, "passwd_popup");
			else
				elm_layout_file_set(elm_object_content_get(ad->popup),
						CUSTOM_POPUP_PATH, "passwd_popup_landscape_ly");
		}
	}
	FN_END;
}

static void __bluetooth_cleanup_win(struct bt_popup_appdata *ad)
{
	ret_if(ad == NULL);

	if (ad->win_main) {
		evas_object_smart_callback_del(ad->win_main, "wm,rotation,changed",
					__bt_main_win_rot_changed_cb);
		evas_object_del(ad->win_main);
		ad->win_main = NULL;
	}
}

static void __bluetooth_player_free_job_cb(void *data)
{
	FN_START;
	player_h sound_player = data;
	player_state_e state = PLAYER_STATE_NONE;

	sound_stream_focus_state_e state_for_playback;
	int ret = PLAYER_ERROR_NONE;
	sound_stream_info_h stream_info = NULL;

	retm_if(sound_player == NULL, "invalid parameter");

	if (player_get_state(sound_player, &state) == PLAYER_ERROR_NONE) {

		BT_INFO("the state of sound player %d", state);

		if (state == PLAYER_STATE_PLAYING) {
			player_stop(sound_player);
			player_unprepare(sound_player);
		}
		if (state == PLAYER_STATE_READY) {
			player_unprepare(sound_player);
		}
	}
	player_destroy(sound_player);

	ret = sound_manager_get_focus_state(stream_info, &state_for_playback, NULL);
	if (state_for_playback == SOUND_STREAM_FOCUS_STATE_ACQUIRED) {
		ret = sound_manager_release_focus(stream_info, SOUND_STREAM_FOCUS_FOR_PLAYBACK, NULL);
		if (ret != SOUND_MANAGER_ERROR_NONE)
			BT_ERR("sound_manager_release_focus() get failed : %d", ret);
	}

	ret = sound_manager_destroy_stream_information(stream_info);
	if (ret != SOUND_MANAGER_ERROR_NONE)
		BT_ERR("sound_manager_destroy_stream_information() get failed : %d", ret);

	FN_END;
}

static void __bluetooth_player_free(player_h sound_player)
{
	FN_START;
	retm_if(sound_player == NULL, "invalid parameter");

	ecore_job_add(__bluetooth_player_free_job_cb, sound_player);
	sound_player = NULL;
	FN_END;
}

static void
__bluetooth_player_del_timeout_timer(struct bt_popup_appdata *ad)
{
	FN_START;
	if (ad->playing_timer) {
		ecore_timer_del(ad->playing_timer);
		ad->playing_timer = NULL;
	}
	FN_END;
}

static void
__bluetooth_player_completed_cb(void *user_data)
{
	retm_if(user_data == NULL, "invalid parameter");
	struct bt_popup_appdata *ad = user_data;

	BT_DBG("Media player completed");

	__bluetooth_player_del_timeout_timer(ad);
	__bluetooth_player_free(ad->player);
}

static void
__bluetooth_sound_stream_focus_state_changed_cb(sound_stream_info_h stream_info, sound_stream_focus_change_reason_e reason_for_change, const char *additional_info, void *user_data)
{
	retm_if(user_data == NULL, "invalid parameter");
	struct bt_popup_appdata *ad = user_data;

	__bluetooth_player_del_timeout_timer(ad);
	__bluetooth_player_free(ad->player);
}

static void
__bluetooth_player_error_cb(int error_code, void *user_data)
{
	retm_if(user_data == NULL, "invalid parameter");
	struct bt_popup_appdata *ad = user_data;

	BT_ERR("Error code [%d]", (int)error_code);

	__bluetooth_player_del_timeout_timer(ad);
	__bluetooth_player_free(ad->player);
}

static Eina_Bool __bluetooth_player_timeout_cb(void *data)
{
	retvm_if(data == NULL, ECORE_CALLBACK_CANCEL, "invalid parameter");
	struct bt_popup_appdata *ad = data;

	__bluetooth_player_free(ad->player);
	ad->playing_timer = NULL;

	return ECORE_CALLBACK_CANCEL;
}

static void __bluetooth_player_start_job_cb(void *data)
{
	FN_START;
	int ret = PLAYER_ERROR_NONE;
	ret_if(!data);
	struct bt_popup_appdata *ad = data;
	ret_if(!ad);

	ret = player_start(ad->player);
	if (ret != PLAYER_ERROR_NONE) {
		BT_ERR("player_start [%d]", ret);
		__bluetooth_player_free(ad->player);
		return;
	}
	ad->playing_timer = ecore_timer_add(15,
			__bluetooth_player_timeout_cb, ad);
	FN_END;
}

static int __bluetooth_notify_event(struct bt_popup_appdata *ad)
{
	FN_START;
	retvm_if(!ad, PLAYER_ERROR_INVALID_PARAMETER, "invalid parameter");
	char *path = NULL;
	player_state_e state = PLAYER_STATE_NONE;
	sound_stream_info_h *stream_info = &ad->stream_info;

	int ret = PLAYER_ERROR_NONE;
	int sndRet = SOUND_MANAGER_ERROR_NONE;

	BT_DBG("Notify event");

	__bluetooth_player_del_timeout_timer(ad);

	if (ad->player != NULL)
		__bluetooth_player_free(ad->player);

	if (*stream_info != NULL) {
		sndRet = sound_manager_destroy_stream_information(*stream_info);
		if (sndRet != SOUND_MANAGER_ERROR_NONE) {
			BT_ERR("sound_manager_destroy_stream_information() get failed : %x", ret);
		}
	}

	sndRet = sound_manager_create_stream_information(SOUND_STREAM_TYPE_NOTIFICATION, __bluetooth_sound_stream_focus_state_changed_cb, (void*)(&ad->player), stream_info);
	if (sndRet != SOUND_MANAGER_ERROR_NONE) {
		BT_ERR("sound_manager_create_stream_information() get failed :%x", sndRet);
		return PLAYER_ERROR_INVALID_PARAMETER;
	}

	sndRet = sound_manager_set_focus_reacquisition(*stream_info, false);
	if (sndRet != SOUND_MANAGER_ERROR_NONE) {
		BT_ERR("sound_manager_set_focus_reacquisition() set failed : %d", ret);
		return sndRet;
	}

	sndRet = sound_manager_acquire_focus(*stream_info, SOUND_STREAM_FOCUS_FOR_PLAYBACK, NULL);
	if (sndRet != SOUND_MANAGER_ERROR_NONE) {
		BT_ERR("sound_manager_acquire_focus() get failed : %d", ret);
		return sndRet;
	}

	ret = player_create(&ad->player);
	if (ret != PLAYER_ERROR_NONE) {
		BT_ERR("creating the player handle failed[%d]", ret);
		ad->player = NULL;
		return ret;
	}

	player_get_state(ad->player, &state);
	if (state > PLAYER_STATE_READY) {
		__bluetooth_player_free(ad->player);
		return ret;
	}

	/* Set the notification sound from vconf*/
	path = vconf_get_str(VCONF_NOTI_SOUND);
	if (path) {
		ret = player_set_uri(ad->player, path);
		if (ret != 0)
			BT_ERR("player_set_uri Failed : %d", ret);

	} else {
		BT_ERR("vconf_get_str failed");
		__bluetooth_player_free(ad->player);
		return ret;
	}

	if (*stream_info != NULL) {
		ret = player_set_audio_policy_info(ad->player, *stream_info);
		if (ret != PLAYER_ERROR_NONE) {
			BT_ERR("player_set_audio_policy_info failed : %d", ret);
			__bluetooth_player_free(ad->player);
			return ret;
		}
	}



	ret = player_prepare(ad->player);
	if (ret != PLAYER_ERROR_NONE) {
		BT_ERR("realizing the player handle failed[%d]", ret);
		__bluetooth_player_free(ad->player);
		return ret;
	}

	player_get_state(ad->player, &state);
	if (state != PLAYER_STATE_READY) {
		BT_ERR("state of player is invalid %d", state);
		__bluetooth_player_free(ad->player);
		return ret;
	}

	/* register callback */
	ret = player_set_completed_cb(ad->player, __bluetooth_player_completed_cb, ad);
	if (ret != PLAYER_ERROR_NONE) {
		BT_ERR("player_set_completed_cb() ERR: %x!!!!", ret);
		__bluetooth_player_free(ad->player);
		return ret;
	}

	ret = player_set_error_cb(ad->player, __bluetooth_player_error_cb, ad);
	if (ret != PLAYER_ERROR_NONE) {
		__bluetooth_player_free(ad->player);
		return ret;
	}

	ecore_job_add(__bluetooth_player_start_job_cb, ad);

	FN_END;
	return ret;
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
	else if (!strcasecmp(event_type, "phonebook-request"))
		ad->event_type = BT_EVENT_PHONEBOOK_REQUEST;
	else if (!strcasecmp(event_type, "message-request"))
		ad->event_type = BT_EVENT_MESSAGE_REQUEST;
	else if (!strcasecmp(event_type, "pairing-retry-request"))
		ad->event_type = BT_EVENT_RETRY_PAIR_REQUEST;
	else if (!strcasecmp(event_type, "music-auto-connect-request"))
		ad->event_type = BT_EVENT_MUSIC_AUTO_CONNECT_REQUEST;
	else if (!strcasecmp(event_type, "remote-legacy-pair-failed"))
		ad->event_type = BT_EVENT_LEGACY_PAIR_FAILED_FROM_REMOTE;
	else
		ad->event_type = 0x0000;
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
	case BT_EVENT_PHONEBOOK_REQUEST:
	case BT_EVENT_MESSAGE_REQUEST:
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

	case BT_EVENT_RETRY_PAIR_REQUEST: {
		DBusMessage *msg = NULL;
		int response = BT_AGENT_REJECT;

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
			      BT_SYS_POPUP_INTERFACE,
			      BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg,
			 DBUS_TYPE_INT32, &response,
			 DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);

		break;
	}

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

static void __bluetooth_input_request_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);
	int response;
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

	if (!g_strcmp0(event, BT_STR_PAIR))
		response = BT_AGENT_ACCEPT;
	else
		response = BT_AGENT_CANCEL;

	if (convert_input_text == NULL)
		return;

	BT_DBG_SECURE("PIN/Passkey[%s] event[%d] response[%d - %s]",
		     convert_input_text, ad->event_type, response,
		     (response == BT_AGENT_ACCEPT) ? "Accept" : "Cancel");

	if (ad->event_type == BT_EVENT_PIN_REQUEST) {
		dbus_g_proxy_call_no_reply(ad->agent_proxy,
				   "ReplyPinCode", G_TYPE_UINT, response,
				   G_TYPE_STRING, convert_input_text,
				   G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		dbus_g_proxy_call_no_reply(ad->agent_proxy,
				   "ReplyPasskey", G_TYPE_UINT, response,
				   G_TYPE_STRING, convert_input_text,
				   G_TYPE_INVALID, G_TYPE_INVALID);
	}
	__bluetooth_delete_input_view(ad);

	free(convert_input_text);

	__bluetooth_win_del(ad);
}

static void __bluetooth_input_cancel_cb(void *data,
				       Evas_Object *obj, void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	bt_device_cancel_bonding();

	__bluetooth_win_del(ad);
}

static void __bluetooth_passkey_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_CONFIRM)) {
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

static void __bluetooth_adapter_state_changed_cb(int result, bt_adapter_state_e state, void *user_data)
{
	BT_DBG("bluetooth state changed callback");
	int ret;
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)user_data;

	if (state == BT_ADAPTER_ENABLED) {
		BT_DBG("bt enabled");

		if (ad->visibility_timeout) {
			ret = bt_adapter_set_visibility(BT_ADAPTER_VISIBILITY_MODE_LIMITED_DISCOVERABLE, atoi(ad->visibility_timeout));
			BT_DBG("set visibility, returns:%d", ret);
		} else {
			ret = bt_adapter_set_visibility(BT_ADAPTER_VISIBILITY_MODE_LIMITED_DISCOVERABLE, 120);
			BT_DBG("set visibility, returns:%d", ret);
		}

		__bluetooth_win_del(ad);
	} else
		BT_DBG("bt disabled");
}

static void __bluetooth_visibility_confirm_cb(void *data,
					 Evas_Object *obj, void *event_info)
{
	BT_DBG("visibility confirm cb called");

	if (obj == NULL || data == NULL)
		return;

	int ret = BT_ERROR_NONE;
	bt_adapter_state_e state = BT_ADAPTER_DISABLED;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_CONFIRM)) {
		/* check state */
		ret = bt_initialize();
		if (ret) {
			BT_DBG("bt initialize, returns:%d", ret);
			return;
		}

		ret = bt_adapter_set_state_changed_cb(__bluetooth_adapter_state_changed_cb, ad);
		if (ret) {
			BT_DBG("bt enable, returns:%d", ret);
			return;
		}

		ret = bt_adapter_get_state(&state);
		if (ret) {
			BT_DBG("bt get state, returns:%d", ret);
			return;
		}

		if (state == BT_ADAPTER_DISABLED) {
			ret = bt_adapter_enable();
			BT_DBG("bt enable, returns:%d", ret);
		} else {
			if (ad->visibility_timeout) {
				ret = bt_adapter_set_visibility(BT_ADAPTER_VISIBILITY_MODE_LIMITED_DISCOVERABLE, atoi(ad->visibility_timeout));
				BT_DBG("set visibility, returns:%d", ret);
			} else {
				ret = bt_adapter_set_visibility(BT_ADAPTER_VISIBILITY_MODE_LIMITED_DISCOVERABLE, 120);
				BT_DBG("set visibility, returns:%d", ret);
			}
			__bluetooth_win_del(ad);
		}
	} else
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
	} else {
		BT_DBG("e_dbus_bus_get success");
		e_dbus_request_name(ad->EDBusHandle,
				    BT_SYS_POPUP_IPC_NAME, 0, NULL, NULL);
	}
	return TRUE;
}

static void __bluetooth_app_confirm_cb(void *data,
				     Evas_Object *obj, void *event_info)
{
	BT_DBG("__bluetooth_app_confirm_cb ");
	if (obj == NULL || data == NULL)
		return;

	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	const char *event = elm_object_text_get(obj);

	DBusMessage *msg = NULL;
	int response;

	msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

	if (!g_strcmp0(event, BT_STR_OK))
		response = BT_AGENT_ACCEPT;
	else
		response = BT_AGENT_REJECT;

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
	guint reply_val;

	if (obj == NULL || ad == NULL)
		return;

	const char *event = elm_object_text_get(obj);

	if (!g_strcmp0(event, BT_STR_OK) || !g_strcmp0(event, BT_STR_ACCEPT)
				|| !g_strcmp0(event, BT_STR_ALLOW)) {
		reply_val = (ad->make_trusted == TRUE) ?
				BT_AGENT_ACCEPT_ALWAYS : BT_AGENT_ACCEPT;
	} else {
		reply_val = BT_AGENT_CANCEL;
	}

	dbus_g_proxy_call_no_reply(ad->agent_proxy, "ReplyAuthorize",
		G_TYPE_UINT, reply_val,
		G_TYPE_INVALID, G_TYPE_INVALID);

	ad->make_trusted = FALSE;

	__bluetooth_win_del(ad);
}

static void __bluetooth_information_cb(void *data,
					       Evas_Object *obj,
					       void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	if (obj == NULL || ad == NULL)
		return;

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

	if (!g_strcmp0(event, BT_STR_OK))
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_ACCEPT,
					   G_TYPE_INVALID, G_TYPE_INVALID);
	else
		dbus_g_proxy_call_no_reply(ad->obex_proxy, "ReplyAuthorize",
					   G_TYPE_UINT, BT_AGENT_CANCEL,
					   G_TYPE_INVALID, G_TYPE_INVALID);

	__bluetooth_win_del(ad);
}

static void __bluetooth_ime_hide(void)
{
	FN_START;
	Ecore_IMF_Context *imf_context = NULL;
	imf_context = ecore_imf_context_add(ecore_imf_context_default_id_get());
	if (imf_context)
		ecore_imf_context_input_panel_hide(imf_context);
	FN_END;
}

static void __bluetooth_entry_keydown_cb(void *data, Evas *e, Evas_Object *obj,
							void *event_info)
{
	FN_START;
	Evas_Event_Key_Down *ev;
	Evas_Object *entry = obj;

	if (data == NULL || event_info == NULL || entry == NULL)
		return;

	ev = (Evas_Event_Key_Down *)event_info;
	BT_INFO("ENTER ev->key:%s", ev->key);

	if (g_strcmp0(ev->key, "KP_Enter") == 0 ||
			g_strcmp0(ev->key, "Return") == 0) {

		Ecore_IMF_Context *imf_context = NULL;

		imf_context =
			(Ecore_IMF_Context*)elm_entry_imf_context_get(entry);
		if (imf_context)
			ecore_imf_context_input_panel_hide(imf_context);

		elm_object_focus_set(entry, EINA_FALSE);
	}
	FN_END;
}

static void __bluetooth_entry_change_cb(void *data, Evas_Object *obj,
				      void *event_info)
{
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;
	char *input_text = NULL;
	char *convert_input_text = NULL;
	int text_length = 0;

	input_text = (char *)elm_entry_entry_get(obj);

	if (input_text) {
		convert_input_text = elm_entry_markup_to_utf8(input_text);
		if (convert_input_text) {
			text_length = strlen(convert_input_text);

			if (text_length == 0) {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_TRUE);
				elm_entry_input_panel_return_key_disabled_set(obj,
						EINA_TRUE);
			} else {
				elm_object_disabled_set(ad->edit_field_save_btn,
							EINA_FALSE);
				elm_entry_input_panel_return_key_disabled_set(obj,
						EINA_FALSE);
			}

			free(convert_input_text);
		}
	}
}

static void __bluetooth_auth_check_clicked_cb(void *data, Evas_Object *obj,
							void *event_info)
{
	struct bt_popup_appdata *ad = data;
	Eina_Bool state = elm_check_state_get(obj);

	BT_INFO("Check %d", state);
	ad->make_trusted = state;
}

static void __bluetooth_auth_check_label_clicked_cb(void *data, Evas_Object *obj,
							void *event_info)
{
	FN_START;
	ret_if(!obj || !data);

	Elm_Object_Item *item = NULL;
	item = (Elm_Object_Item *)event_info;
	ret_if(!item);

	struct bt_popup_appdata *ad = data;
	ret_if(!ad);

	Evas_Object *content = elm_object_item_part_content_get(item, "elm.icon.2");
	Evas_Object *ck = elm_object_part_content_get(content, "elm.swallow.content");

	elm_genlist_item_selected_set(item, EINA_FALSE);

	Eina_Bool state = elm_check_state_get(ck);

	elm_check_state_set(ck, !state);

	BT_INFO("Check %d", !state);
	ad->make_trusted = !state;

	FN_END;
}
static void __bluetooth_mouseup_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;

	BT_DBG("Mouse event callback function is called + ");

	if (ev->button == 3) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_auth_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_auth_cb);
		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg,
					 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		__bluetooth_win_del(ad);
	}
	BT_DBG("Mouse event callback -");
}

static void __bluetooth_keyback_auth_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
#if 0
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	DBusMessage *msg = NULL;
	int response = BT_AGENT_REJECT;
#endif

	BT_DBG("Keyboard event callback function is called + ");

#if 0
	if (!strcmp(ev->keyname, KEY_BACK)) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_auth_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_auth_cb);

		msg = dbus_message_new_signal(BT_SYS_POPUP_IPC_RESPONSE_OBJECT,
				      BT_SYS_POPUP_INTERFACE,
				      BT_SYS_POPUP_METHOD_RESPONSE);

		dbus_message_append_args(msg,
					 DBUS_TYPE_INT32, &response, DBUS_TYPE_INVALID);

		e_dbus_message_send(ad->EDBusHandle, msg, NULL, -1, NULL);
		dbus_message_unref(msg);
		__bluetooth_win_del(ad);
	}
#endif
	BT_DBG("Keyboard Mouse event callback -");
}

static void __bluetooth_draw_auth_popup(struct bt_popup_appdata *ad,
			const char *title, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	char temp_str[BT_TITLE_STR_MAX_LEN + BT_TEXT_EXTRA_LEN] = { 0 };
	Evas_Object *btn1 = NULL;
	Evas_Object *btn2 = NULL;
	Evas_Object *layout = NULL;
	Evas_Object *label = NULL;
	Evas_Object *check = NULL;
	BT_DBG("+");

	ad->make_trusted = TRUE;

	ad->popup = elm_popup_add(ad->layout);
	elm_popup_align_set(ad->popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
	elm_object_style_set(ad->popup, "transparent");

	/*set window level to HIGH*/
//	__bluetooth_set_win_level(ad->popup);

	layout = elm_layout_add(ad->popup);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "auth_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND,
							EVAS_HINT_EXPAND);

	if (title != NULL) {
		elm_object_part_text_set(ad->popup, "title,text",
				BT_STR_ALLOW_APP_PERMISSION);

		snprintf(temp_str, BT_TITLE_STR_MAX_LEN + BT_TEXT_EXTRA_LEN,
					"%s", title);

		label = elm_label_add(ad->popup);
		elm_object_style_set(label, "popup/default");
		elm_label_line_wrap_set(label, ELM_WRAP_MIXED);
		elm_object_text_set(label, temp_str);
		evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, 0.0);
		evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
		elm_object_part_content_set(layout, "popup_title", label);
		evas_object_show(label);
	}

	/* check */
	check = elm_check_add(ad->popup);
	elm_object_style_set(check, "popup");
	elm_check_state_set(check, EINA_FALSE);
	elm_object_text_set(check, BT_STR_DONT_ASK_AGAIN);
	evas_object_size_hint_align_set(check, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_show(check);
	evas_object_smart_callback_add(check, "changed",
					__bluetooth_auth_check_clicked_cb, ad);
	elm_object_part_content_set(layout, "check", check);

	evas_object_show(layout);
	elm_object_content_set(ad->popup, layout);

	btn1 = elm_button_add(ad->popup);
	elm_object_style_set(btn1, "popup");
	elm_object_text_set(btn1, btn1_text);
	elm_object_part_content_set(ad->popup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", func, ad);

	btn2 = elm_button_add(ad->popup);
	elm_object_style_set(btn2, "popup");
	elm_object_text_set(btn2, btn2_text);
	elm_object_part_content_set(ad->popup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", func, ad);

	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_mouseup_auth_cb, ad);
	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_keyback_auth_cb, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);

	BT_DBG("-");
}

static void __bluetooth_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;

	BT_DBG("Mouse event callback function is called + ");

	if (ev->button == 3) {
		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
	BT_DBG("Mouse event callback -");
}

static void __bluetooth_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Key_Down *ev = event_info;
#if 0
	struct bt_popup_appdata *ad = data;
#endif

	BT_DBG("Keyboard event callback function is called %s+ ", ev->keyname);
#if 0
	if (!strcmp(ev->keyname, KEY_BACK)) {

		evas_object_event_callback_del(obj, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_mouseup_cb);
		evas_object_event_callback_del(obj, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_keyback_cb);
		__bluetooth_remove_all_event(ad);
	}
#endif
	BT_DBG("Keyboard Mouse event callback -");
}

static void __bluetooth_draw_popup(struct bt_popup_appdata *ad,
			const char *title, char *text, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	Evas_Object *btn1;
	Evas_Object *btn2;

	BT_DBG("__bluetooth_draw_popup");

	ad->popup = elm_popup_add(ad->layout);
	elm_popup_align_set(ad->popup, ELM_NOTIFY_ALIGN_FILL, 1.0);

	/*set window level to HIGH*/
//	__bluetooth_set_win_level(ad->popup);

#ifdef TIZEN_REDWOOD
	elm_object_style_set(ad->popup, "transparent");
#endif
	if (title != NULL) {
		elm_object_part_text_set(ad->popup, "title,text", title);
	}

	if (text != NULL) {
		char *markup_text = NULL;
		markup_text = elm_entry_utf8_to_markup(text);
		elm_object_text_set(ad->popup, markup_text);
		free(markup_text);
	}

	if ((btn1_text != NULL) && (btn2_text != NULL)) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);

		btn2 = elm_button_add(ad->popup);
		elm_object_style_set(btn2, "popup");
		elm_object_text_set(btn2, btn2_text);
		elm_object_part_content_set(ad->popup, "button2", btn2);
		evas_object_smart_callback_add(btn2, "clicked", func, ad);
	} else if (btn1_text != NULL) {
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);
	}

	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_mouseup_cb, ad);
	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_keyback_cb, ad);

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	elm_object_focus_set(ad->popup, EINA_TRUE);

	BT_DBG("__bluetooth_draw_popup END");
}

static void __bluetooth_input_mouseup_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	FN_START;
	Evas_Event_Mouse_Up *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;

	BT_DBG("ev->button : %d", ev->button);

	if (ev->button == 3) {
		ret_if(ad == NULL);
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);

		/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */
		input_text = (char *)elm_entry_entry_get(ad->entry);
		if (input_text) {
			convert_input_text =
				elm_entry_markup_to_utf8(input_text);
		}
		if (convert_input_text == NULL)
			return;

		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		}
		__bluetooth_delete_input_view(ad);
		free(convert_input_text);
		if (ad->entry) {
			evas_object_del(ad->entry);
			ad->entry = NULL;
		}
		__bluetooth_win_del(ad);
	}
	FN_END;
}

static void __bluetooth_input_keyback_cb(void *data,
			Evas *e, Evas_Object *obj, void *event_info)
{
	FN_START;
#if 0
	Evas_Event_Key_Down *ev = event_info;
	struct bt_popup_appdata *ad = data;
	int response = BT_AGENT_CANCEL;
	char *input_text = NULL;
	char *convert_input_text = NULL;

	if (!strcmp(ev->keyname, KEY_BACK)) {
		if (ad == NULL)
			return;
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->entry, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_MOUSE_UP,
				__bluetooth_input_mouseup_cb);
		evas_object_event_callback_del(ad->popup, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_input_keyback_cb);
		/* BT_EVENT_PIN_REQUEST / BT_EVENT_PASSKEY_REQUEST */
		input_text = (char *)elm_entry_entry_get(ad->entry);
		if (input_text) {
			convert_input_text =
				elm_entry_markup_to_utf8(input_text);
		}
		if (convert_input_text == NULL)
			return;

		if (ad->event_type == BT_EVENT_PIN_REQUEST) {
			BT_DBG("It is PIN Request event ");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPinCode", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		} else {
			BT_DBG("It is PASSKEYRequest event ");
			dbus_g_proxy_call_no_reply(ad->agent_proxy,
					   "ReplyPasskey", G_TYPE_UINT, response,
					   G_TYPE_STRING, convert_input_text,
					   G_TYPE_INVALID, G_TYPE_INVALID);
		}
		__bluetooth_delete_input_view(ad);
		free(convert_input_text);
		__bluetooth_win_del(ad);
	}
#endif
	FN_END;
}

static void __bluetooth_pin_check_clicked_cb(void *data, Evas_Object *obj,
                                                       void *event_info)
{
	FN_START;
	ret_if(!obj || !data);

	Eina_Bool state = elm_check_state_get(obj);

	BT_INFO("Check %d", state);
	if (state) {
		elm_entry_password_set((Evas_Object *)data, EINA_FALSE);
	} else {
		elm_entry_password_set((Evas_Object *)data, EINA_TRUE);
	}
	elm_entry_cursor_end_set((Evas_Object *)data);

	FN_END;
}

static void __bluetooth_pswd_check_box_sel(void *data, Evas_Object *obj,
                                                       void *event_info)
{
	FN_START;
	ret_if(!obj || !data);

	Elm_Object_Item *item = NULL;
	item = (Elm_Object_Item *)event_info;
	ret_if(!item);

	struct bt_popup_appdata *ad = data;
	ret_if(!ad);

	Evas_Object *content = elm_object_item_part_content_get(item, "elm.icon.2");
	Evas_Object *ck = elm_object_part_content_get(content, "elm.swallow.content");

	elm_genlist_item_selected_set(item, EINA_FALSE);

	Eina_Bool state = elm_check_state_get(ck);

	elm_check_state_set(ck, !state);

	if (ad->entry) {
		__bluetooth_pin_check_clicked_cb(ad->entry, ck, NULL);
	}

	FN_END;
}

static void __bluetooth_entry_edit_mode_show_cb(void *data, Evas *e, Evas_Object *obj,
		void *event_info)
{
	FN_START;
	evas_object_event_callback_del(obj, EVAS_CALLBACK_SHOW,
			__bluetooth_entry_edit_mode_show_cb);

	elm_object_focus_set(obj, EINA_TRUE);
	FN_END;
}

static void __bluetooth_entry_activated_cb(void *data, Evas_Object *obj, void *event_info)
{
	FN_START;
	if (!obj)
		return;

	elm_object_focus_set(obj, EINA_FALSE);
	FN_END;
}



static Evas_Object *__bluetooth_passwd_entry_icon_get(
				void *data, Evas_Object *obj, const char *part)
{
	FN_START;
	retv_if(obj == NULL || data == NULL, NULL);

	Evas_Object *entry = NULL;
	Evas_Object *layout = NULL;
	struct bt_popup_appdata *ad = data;
	static Elm_Entry_Filter_Limit_Size limit_filter_data;

	if (!strcmp(part, "elm.swallow.content")) {
		layout = elm_layout_add(obj);
		elm_layout_theme_set(layout, "layout", "editfield", "singleline");
		evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(layout, EVAS_HINT_FILL, 0.0);
		evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, 0.0);

		static Elm_Entry_Filter_Accept_Set accept_set = {
			.accepted = "0123456789",
			.rejected = NULL
		};

		entry = elm_entry_add(layout);
#if 0
		ea_entry_selection_back_event_allow_set(entry, EINA_TRUE);
#endif
		evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0.0);
		evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, 0.0);
		elm_entry_single_line_set(entry, EINA_TRUE);
		elm_entry_scrollable_set(entry, EINA_TRUE);
		elm_entry_password_set(entry, EINA_FALSE);

		elm_object_signal_emit(entry, "elm,action,hide,search_icon", "");
		elm_object_part_text_set(entry, "guide", BT_STR_PIN);
		eext_entry_selection_back_event_allow_set(entry, EINA_TRUE);
		elm_entry_prediction_allow_set(entry, EINA_FALSE);

//		elm_entry_input_panel_imdata_set(entry, "action=disable_emoticons", 24);
		elm_entry_input_panel_return_key_type_set(entry,
				ELM_INPUT_PANEL_RETURN_KEY_TYPE_DONE);
		elm_entry_input_panel_enabled_set(entry, EINA_TRUE);

		elm_entry_markup_filter_append(entry, elm_entry_filter_accept_set,
				&accept_set);

		elm_entry_cnp_mode_set(entry, ELM_CNP_MODE_PLAINTEXT);

		elm_entry_password_set(entry, EINA_TRUE);
		elm_entry_input_panel_layout_set(entry, ELM_INPUT_PANEL_LAYOUT_NUMBERONLY);

		elm_entry_input_panel_return_key_type_set(entry, ECORE_IMF_INPUT_PANEL_RETURN_KEY_TYPE_DONE);
		elm_entry_input_panel_return_key_disabled_set(entry, EINA_TRUE);

		if (ad->event_type == BT_EVENT_PASSKEY_REQUEST)
			limit_filter_data.max_char_count = BT_PK_MLEN;
		else
			limit_filter_data.max_char_count = BT_PIN_MLEN;

		elm_entry_markup_filter_append(entry,
			elm_entry_filter_limit_size, &limit_filter_data);

		evas_object_event_callback_add(entry, EVAS_CALLBACK_KEY_DOWN,
				__bluetooth_entry_keydown_cb, ad);

		evas_object_smart_callback_add(entry, "activated",
				__bluetooth_entry_activated_cb, ad);
		evas_object_smart_callback_add(entry, "changed",
			__bluetooth_entry_change_cb, ad);

		evas_object_event_callback_add(entry, EVAS_CALLBACK_SHOW,
				__bluetooth_entry_edit_mode_show_cb, ad);

		elm_object_part_content_set(layout, "elm.swallow.content", entry);
		//evas_object_show(entry);
		//elm_object_focus_set(entry, EINA_TRUE);

		ad->entry = entry;

	}

	FN_END;
	return layout;
}

static char *__bluetooth_popup_desc_label_get(void *data, Evas_Object *obj,
					      const char *part)
{
	FN_START;

	struct bt_popup_appdata *ad;

	retv_if(!data, NULL);

	ad = data;

	if (!strcmp(part, "elm.text.multiline"))
		return g_strdup(ad->description);

	FN_END;

	return NULL;
}

static char *__bluetooth_access_check_label_get(void *data, Evas_Object *obj,
					      const char *part)
{
	FN_START;
	retv_if(!data, NULL);
	retv_if(!strcmp(part, "elm.text.main.left"),
			g_strdup(BT_STR_DO_NOT_SHOW_AGAIN));
	FN_END;
	return NULL;
}

static char *__bluetooth_passwd_show_passwd_label_get(void *data, Evas_Object *obj,
					      const char *part)
{
	FN_START;
	retv_if(!data, NULL);
	if (!strcmp("elm.text", part))
		return g_strdup(BT_STR_SHOW_PIN);

	FN_END;
	return NULL;
}
static Evas_Object *__bluetooth_access_check_icon_get(
				void *data, Evas_Object *obj, const char *part)

{
	FN_START;
	retv_if(strcmp(part, "elm.icon.2"), NULL);
	struct bt_popup_appdata *ad = data;
	retv_if(!ad, NULL);
	Evas_Object *layout = NULL;
	layout = elm_layout_add(obj);

	elm_layout_theme_set(layout, "layout", "list/C/type.2", "default");
	Evas_Object *check = elm_check_add(layout);
	evas_object_propagate_events_set(check, EINA_FALSE);
	elm_object_style_set(check, "popup");
	if (ad->make_trusted == 0)
		elm_check_state_set(check, EINA_FALSE);
	else
		elm_check_state_set(check, EINA_TRUE);
	evas_object_size_hint_align_set(check, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_smart_callback_add(check, "changed",
				__bluetooth_auth_check_clicked_cb, ad);

	elm_object_tree_focus_allow_set(check, EINA_FALSE);
	elm_layout_content_set(layout, "elm.swallow.content", check);
	FN_END;
	return layout;
}

static Evas_Object *__bluetooth_passwd_show_passwd_icon_get(
				void *data, Evas_Object *obj, const char *part)

{
	FN_START;
	Evas_Object *check = NULL;

	retv_if(strcmp(part, "elm.swallow.end"), NULL);

	struct bt_popup_appdata *ad = data;
	retv_if(!ad, NULL);

	check = elm_check_add(obj);
	evas_object_propagate_events_set(check, EINA_FALSE);

	evas_object_size_hint_align_set(check, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(check, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_smart_callback_add(check, "changed",
			__bluetooth_pin_check_clicked_cb, ad->entry);

	FN_END;
	return check;
}

static void __bluetooth_draw_input_view(struct bt_popup_appdata *ad,
			const char *title, const char *text,
			void (*func)
			(void *data, Evas_Object *obj, void *event_info))
{
	FN_START;
	Evas_Object *passpopup = NULL;
	Evas_Object *l_button = NULL;
	Evas_Object *r_button = NULL;
	Evas_Object *genlist = NULL;
	Elm_Object_Item *git = NULL;

	ret_if(ad == NULL || ad->win_main == NULL || ad->layout == NULL);

//	evas_object_show(ad->win_main);

	passpopup = elm_popup_add(ad->layout);
	ad->popup = passpopup;

	elm_popup_align_set(passpopup, ELM_NOTIFY_ALIGN_FILL, 1.0);
	evas_object_size_hint_weight_set(passpopup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	/*set window level to HIGH*/
//	__bluetooth_set_win_level(ad->popup);

	elm_object_part_text_set(passpopup, "title,text", title);

	genlist = elm_genlist_add(passpopup);
	evas_object_size_hint_weight_set(genlist,
			EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);
	elm_scroller_content_min_limit(genlist, EINA_FALSE, EINA_TRUE);

	/* Description */
	ad->passwd_desc_itc = elm_genlist_item_class_new();
	if (ad->passwd_desc_itc) {
		ad->passwd_desc_itc->item_style = "multiline";
		ad->passwd_desc_itc->func.text_get = __bluetooth_popup_desc_label_get;
		ad->passwd_desc_itc->func.content_get = NULL;
		ad->passwd_desc_itc->func.state_get = NULL;
		ad->passwd_desc_itc->func.del = NULL;

		g_free(ad->description);
		ad->description = g_strdup(text);

		git = elm_genlist_item_append(genlist, ad->passwd_desc_itc, ad, NULL,
				ELM_GENLIST_ITEM_NONE, NULL, NULL);
		elm_genlist_item_select_mode_set(git,
						 ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
	}

	/* Entry genlist item */
	ad->passwd_entry_itc = elm_genlist_item_class_new();
	if (ad->passwd_entry_itc) {
		ad->passwd_entry_itc->item_style = "full";
		ad->passwd_entry_itc->func.text_get = NULL;
		ad->passwd_entry_itc->func.content_get = __bluetooth_passwd_entry_icon_get;
		ad->passwd_entry_itc->func.state_get = NULL;
		ad->passwd_entry_itc->func.del = NULL;

		elm_genlist_item_append(genlist, ad->passwd_entry_itc, ad,
				NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
	}

	/* Show password */
	ad->passwd_show_passwd_itc = elm_genlist_item_class_new();
	if (ad->passwd_show_passwd_itc) {
		ad->passwd_show_passwd_itc->item_style = "type1";
		ad->passwd_show_passwd_itc->func.text_get = __bluetooth_passwd_show_passwd_label_get;
		ad->passwd_show_passwd_itc->func.content_get = __bluetooth_passwd_show_passwd_icon_get;
		ad->passwd_show_passwd_itc->func.state_get = NULL;
		ad->passwd_show_passwd_itc->func.del = NULL;

		git = elm_genlist_item_append(genlist, ad->passwd_show_passwd_itc, ad,
				NULL, ELM_GENLIST_ITEM_NONE,
				__bluetooth_pswd_check_box_sel, ad);
	}

	l_button = elm_button_add(passpopup);
	elm_object_style_set(l_button, "popup");
	elm_object_text_set(l_button, BT_STR_CANCEL);
	elm_object_part_content_set(passpopup, "button1", l_button);
	evas_object_smart_callback_add(l_button, "clicked", func, ad);
	elm_object_tree_focus_allow_set(l_button, EINA_FALSE);


	r_button = elm_button_add(passpopup);
	elm_object_style_set(r_button, "popup");
	elm_object_text_set(r_button, BT_STR_PAIR);
	elm_object_part_content_set(passpopup, "button2", r_button);
	evas_object_smart_callback_add(r_button, "clicked", func, ad);
	elm_object_disabled_set(r_button, EINA_TRUE);
	elm_object_tree_focus_allow_set(r_button, EINA_FALSE);
	ad->edit_field_save_btn = r_button;


	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(ad->popup, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

	evas_object_event_callback_add(r_button, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(r_button, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

	evas_object_event_callback_add(l_button, EVAS_CALLBACK_MOUSE_UP,
			__bluetooth_input_mouseup_cb, ad);
	evas_object_event_callback_add(l_button, EVAS_CALLBACK_KEY_DOWN,
			__bluetooth_input_keyback_cb, ad);

#if 0
	elm_genlist_realization_mode_set(genlist, EINA_TRUE);
#endif
	evas_object_show(genlist);
	elm_object_content_set(passpopup, genlist);
	evas_object_show(passpopup);
	evas_object_show(ad->win_main);

	FN_END;
}

static void __bluetooth_draw_access_request_popup(struct bt_popup_appdata *ad,
			const char *title, char *text, char *btn1_text,
			char *btn2_text, void (*func) (void *data,
			Evas_Object *obj, void *event_info))
{
	Evas_Object *layout = NULL;
	Evas_Object *btn1 = NULL;
	Evas_Object *btn2 = NULL;
	Evas_Object *genlist = NULL;
	Elm_Object_Item *git = NULL;

	BT_DBG("+");
	ad->make_trusted = FALSE;

	ad->popup = elm_popup_add(ad->layout);
	elm_popup_align_set(ad->popup, ELM_NOTIFY_ALIGN_FILL, 1.0);

	/*set window level to HIGH*/
//	__bluetooth_set_win_level(ad->popup);

	if (title != NULL) {
		elm_object_part_text_set(ad->popup, "title,text", title);
	}

	/* layout */
	layout = elm_layout_add(ad->popup);
	elm_layout_file_set(layout, CUSTOM_POPUP_PATH, "access_req_popup");
	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	genlist = elm_genlist_add(layout);
	elm_genlist_homogeneous_set(genlist, EINA_TRUE);
	evas_object_size_hint_weight_set(genlist,
			EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

	/* Description text*/
	ad->desc_itc = elm_genlist_item_class_new();
	if (ad->desc_itc) {
		ad->desc_itc->item_style = "multiline";
		ad->desc_itc->func.text_get = __bluetooth_popup_desc_label_get;
		ad->desc_itc->func.content_get = NULL;
		ad->desc_itc->func.state_get = NULL;
		ad->desc_itc->func.del = NULL;

		g_free(ad->description);
		ad->description = g_strdup(text);

		git = elm_genlist_item_append(genlist, ad->desc_itc, ad, NULL,
				ELM_GENLIST_ITEM_NONE, NULL, NULL);
		elm_genlist_item_select_mode_set(git,
				ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
	}

	/* Do not show again */
	ad->check_itc = elm_genlist_item_class_new();
	if (ad->check_itc) {
		ad->check_itc->item_style = "1line";
		ad->check_itc->func.text_get = __bluetooth_access_check_label_get;
		ad->check_itc->func.content_get = __bluetooth_access_check_icon_get;
		ad->check_itc->func.state_get = NULL;
		ad->check_itc->func.del = NULL;

		git = elm_genlist_item_append(genlist, ad->check_itc, ad,
				NULL, ELM_GENLIST_ITEM_NONE,
				__bluetooth_auth_check_label_clicked_cb, ad);
	}

	if (btn1_text != NULL && btn2_text != NULL) {
		/* Cancel button */
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);

		/* Allow button */
		btn2 = elm_button_add(ad->popup);
		elm_object_style_set(btn2, "popup");
		elm_object_text_set(btn2, btn2_text);
		elm_object_part_content_set(ad->popup, "button2", btn2);
		evas_object_smart_callback_add(btn2, "clicked", func, ad);
	}

#if 0
	elm_genlist_realization_mode_set(genlist, EINA_TRUE);
#endif
	evas_object_show(genlist);
	elm_object_part_content_set(layout, "elm.swallow.layout", genlist);

	elm_object_content_set(ad->popup, layout);
	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	BT_DBG("-");
}

static void __bluetooth_draw_information_popup(struct bt_popup_appdata *ad,
			const char *title, char *text, char *btn1_text,
			void (*func) (void *data, Evas_Object *obj, void *event_info))
{
	BT_DBG("+");
	Evas_Object *btn1 = NULL;

	ad->popup = elm_popup_add(ad->layout);
	elm_popup_align_set(ad->popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK, func, ad);

	/*set window level to HIGH*/
//	__bluetooth_set_win_level(ad->popup);

	if (title)
		elm_object_part_text_set(ad->popup, "title,text", title);

	if (text)
		elm_object_text_set(ad->popup, text);

	if (btn1_text != NULL) {
		/* OK button */
		btn1 = elm_button_add(ad->popup);
		elm_object_style_set(btn1, "popup");
		elm_object_text_set(btn1, btn1_text);
		elm_object_part_content_set(ad->popup, "button1", btn1);
		evas_object_smart_callback_add(btn1, "clicked", func, ad);
	}

	evas_object_show(ad->popup);
	evas_object_show(ad->win_main);
	BT_DBG("-");
}

static void __bluetooth_delete_input_view(struct bt_popup_appdata *ad)
{
	FN_START;
	__bluetooth_ime_hide();
	FN_END;
}

static DBusGProxy* __bluetooth_create_agent_proxy(DBusGConnection *conn,
								const char *path)
{
	return dbus_g_proxy_new_for_name(conn, "org.projectx.bt", path,
							"org.bluez.Agent1");
}

static void
__bluetooth_popup_block_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
	FN_START;
	if (obj)
		evas_object_del(obj);
}

static void __bluetooth_draw_toast_popup(struct bt_popup_appdata *ad, char *toast_text)
{
	FN_START;
	ad->popup = elm_popup_add(ad->win_main);
	elm_object_style_set(ad->popup, "toast");
	evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	ea_object_event_callback_add(ad->popup, EA_CALLBACK_BACK, ea_popup_back_cb, NULL);
	elm_object_part_text_set(ad->popup, "elm.text.content", toast_text);
	evas_object_smart_callback_add(ad->popup, "block,clicked", __bluetooth_popup_block_clicked_cb, NULL);

//	__bluetooth_set_win_level(ad->popup);

	evas_object_show(ad->popup);
	FN_END;
}

/* AUL bundle handler */
static int __bluetooth_launch_handler(struct bt_popup_appdata *ad,
			     void *reset_data, const char *event_type)
{
	FN_START;
	bundle *kb = (bundle *) reset_data;
	char view_title[BT_TITLE_STR_MAX_LEN] = { 0 };
	char text[BT_GLOBALIZATION_STR_LENGTH] = { 0 };
	int timeout = 0;
	char *device_name = NULL;
	char *passkey = NULL;
	char *file = NULL;
	char *agent_path;
	char *conv_str = NULL;
	char *stms_str = NULL;
	int ret;

	if (!reset_data || !event_type)
		return -1;

	BT_INFO("Event Type = %s[0X%X]", event_type, ad->event_type);

	if (!strcasecmp(event_type, "pin-request")) {
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		retv_if(!ad->agent_proxy, -1);

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			BT_STR_BLUETOOTH_PAIRING_REQUEST);

		stms_str = BT_STR_ENTER_THE_PIN_TO_PAIR;

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted PIN for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);
	} else if (!strcasecmp(event_type, "passkey-confirm-request")) {
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "passkey", &passkey);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);


		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name && passkey) {
			stms_str = BT_STR_CONFIRM_PASSKEY_PS_TO_PAIR_WITH_PS;

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				stms_str, passkey, device_name);

			BT_INFO("title: %s", view_title);

			__bluetooth_draw_popup(ad, BT_STR_BLUETOOTH_PAIRING_REQUEST,
					view_title, BT_STR_CANCEL, BT_STR_CONFIRM,
					__bluetooth_passkey_confirm_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "visibility-request")) {
		BT_INFO("visibility request popup");

		__bluetooth_draw_popup(ad, "Bluetooth permission request",
					"Requesting permission to turn on Bluetooth and to set Visibility. Do you want to do this?", BT_STR_CANCEL, BT_STR_CONFIRM,
					__bluetooth_visibility_confirm_cb);

	} else if (!strcasecmp(event_type, "passkey-request")) {
		char *device_name = NULL;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			BT_STR_BLUETOOTH_PAIRING_REQUEST);

		stms_str = BT_STR_ENTER_THE_PIN_TO_PAIR;

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

		/* Request user inputted Passkey for basic pairing */
		__bluetooth_draw_input_view(ad, view_title, text,
					  __bluetooth_input_request_cb);

	} else if (!strcasecmp(event_type, "passkey-display-request")) {
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "passkey", &passkey);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (device_name && passkey) {
			stms_str = BT_STR_ENTER_PS_ON_PS_TO_PAIR;

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				stms_str, passkey, device_name);

			BT_INFO("title: %s", view_title);

			__bluetooth_draw_popup(ad, BT_STR_SHOW_PASSWORD, view_title,
						BT_STR_CANCEL, NULL,
						__bluetooth_input_cancel_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		stms_str = BT_STR_ALLOW_PS_TO_CONNECT_Q;

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_ACCEPT,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "app-confirm-request")) {
		BT_DBG("app-confirm-request");
		timeout = BT_AUTHORIZATION_TIMEOUT;

		char *title = NULL;
		char *type = NULL;

		ret = bundle_get_str(kb, "title", &title);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "type", &type);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (!title)
			return -1;

		if (strcasecmp(type, "twobtn") == 0) {
			__bluetooth_draw_popup(ad, NULL, title, BT_STR_CANCEL, BT_STR_OK,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "onebtn") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, NULL, title, BT_STR_OK, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			timeout = BT_NOTIFICATION_TIMEOUT;
			__bluetooth_draw_popup(ad, NULL, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "push-authorize-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "file", &file);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (device_name) {
			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				"%s%s%s", BT_STR_RECEIVE_PS_FROM_PS_Q, file, device_name);
		}

		__bluetooth_draw_popup(ad, BT_STR_RECEIVE_FILE, view_title, BT_STR_CANCEL, BT_STR_OK,
				__bluetooth_push_authorization_request_cb);
	} else if (!strcasecmp(event_type, "confirm-overwrite-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "file", &file);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		stms_str = BT_STR_OVERWRITE_FILE_Q;

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				stms_str, file);

		__bluetooth_draw_popup(ad, BT_STR_RECEIVE_FILE, view_title,
				BT_STR_CANCEL, BT_STR_OK,
				__bluetooth_app_confirm_cb);
	} else if (!strcasecmp(event_type, "keyboard-passkey-request")) {
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "passkey", &passkey);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);


		if (device_name && passkey) {
			stms_str = BT_STR_ENTER_PS_ON_PS_TO_PAIR;

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				stms_str, passkey, device_name);

			BT_INFO("title: %s", view_title);

			__bluetooth_draw_popup(ad, BT_STR_BLUETOOTH_PAIRING_REQUEST,
						view_title,
						BT_STR_CANCEL, NULL,
						__bluetooth_input_cancel_cb);
		} else {
			timeout = BT_ERROR_TIMEOUT;
		}
	} else if (!strcasecmp(event_type, "bt-information")) {
		BT_DBG("bt-information");
		timeout = BT_NOTIFICATION_TIMEOUT;

		char *title = NULL;
		char *type = NULL;

		ret = bundle_get_str(kb, "title", &title);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "type", &type);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (title != NULL) {
			if (strlen(title) > 255)
				return -1;
		} else
			return -1;

		if (strcasecmp(type, "onebtn") == 0) {
			__bluetooth_draw_popup(ad, NULL, title, BT_STR_OK, NULL,
					     __bluetooth_app_confirm_cb);
		} else if (strcasecmp(type, "none") == 0) {
			__bluetooth_draw_popup(ad, NULL, title, NULL, NULL,
					     __bluetooth_app_confirm_cb);
		}
	} else if (!strcasecmp(event_type, "exchange-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name) {
			stms_str = BT_STR_WANTS_TO_SEND_YOU_A_FILE;

			snprintf(view_title, BT_TITLE_STR_MAX_LEN,
				stms_str, device_name);
		}

		__bluetooth_draw_popup(ad, BT_STR_RECEIVE_FILE,
				view_title, BT_STR_CANCEL, BT_STR_ACCEPT,
				     __bluetooth_authorization_request_cb);
	} else if (!strcasecmp(event_type, "phonebook-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);


		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		stms_str = BT_STR_PS_CONTACT_REQUEST;

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

#ifdef TIZEN_REDWOOD
		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
#else
		__bluetooth_draw_access_request_popup(ad, BT_STR_ALLOW_APP_PERMISSION,
					view_title, BT_STR_CANCEL, BT_STR_ALLOW,
					__bluetooth_authorization_request_cb);
#endif
	} else if (!strcasecmp(event_type, "message-request")) {
		timeout = BT_AUTHORIZATION_TIMEOUT;

		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ret = bundle_get_str(kb, "agent-path", &agent_path);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		ad->agent_proxy = __bluetooth_create_agent_proxy(ad->conn, agent_path);
		if (!ad->agent_proxy)
			return -1;

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		stms_str = BT_STR_PS_MESSAGE_REQUEST;

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

#ifdef TIZEN_REDWOOD
		__bluetooth_draw_auth_popup(ad, view_title, BT_STR_CANCEL, BT_STR_OK,
				     __bluetooth_authorization_request_cb);
#else
		__bluetooth_draw_access_request_popup(ad, BT_STR_ALLOW_APP_PERMISSION,
					view_title, BT_STR_CANCEL, BT_STR_ALLOW,
					__bluetooth_authorization_request_cb);
#endif
	} else if (!strcasecmp(event_type, "pairing-retry-request")) {
		int ret;
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		stms_str = BT_STR_UNABLE_TO_PAIR;

		snprintf(view_title, BT_TITLE_STR_MAX_LEN,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

		ret = notification_status_message_post(view_title);
		if (ret != NOTIFICATION_ERROR_NONE)
			BT_ERR("notification_status_message_post() is failed : %d\n", ret);
	} else if (!strcasecmp(event_type, "remote-legacy-pair-failed")) {
		BT_DBG("remote-legacy-pair-failed");
		ret = bundle_get_str(kb, "device-name", &device_name);
		if (ret != BUNDLE_ERROR_NONE)
			BT_ERR("bundle_get_str() is failed : %d\n", ret);

		if (device_name)
			conv_str = elm_entry_utf8_to_markup(device_name);

		stms_str = BT_STR_UNABLE_TO_PAIR;

		snprintf(text, BT_GLOBALIZATION_STR_LENGTH,
			stms_str, conv_str);

		if (conv_str)
			free(conv_str);

		__bluetooth_draw_information_popup(ad, "Bluetooth Error",
					text, BT_STR_OK,
					__bluetooth_information_cb);
	} else if (!strcasecmp(event_type, "music-auto-connect-request")) {
		__bluetooth_draw_toast_popup(ad, "Connecting...");
	} else {
		return -1;
	}

	if (ad->event_type != BT_EVENT_FILE_RECEIVED && timeout != 0) {
		ad->timer = ecore_timer_add(timeout, (Ecore_Task_Cb)
					__bluetooth_request_timeout_cb, ad);
	}
	BT_DBG("-");
	return 0;
}

static void __popup_terminate(void)
{
	BT_DBG("+");
	elm_exit();
	BT_DBG("-");
}

static void __bluetooth_win_del(void *data)
{
	BT_DBG("+");
	struct bt_popup_appdata *ad = (struct bt_popup_appdata *)data;

	__bluetooth_cleanup(ad);
	__bluetooth_cleanup_win(ad);
	__popup_terminate();
	BT_DBG("+");
}

/* utilx and ecore_x APIs are unnecessary in Tizen 3.x based on wayland */
#if 0
static void __bluetooth_set_win_level(Evas_Object *parent)
{
	ret_if(!parent);
	Ecore_X_Window xwin;
	int lock_state;

	xwin = elm_win_xwindow_get(parent);
	if (xwin == 0) {
		BT_ERR("elm_win_xwindow_get is failed");
	} else {
		BT_DBG("Setting window type");
		ecore_x_netwm_window_type_set(xwin,
				ECORE_X_WINDOW_TYPE_NOTIFICATION);
		if (vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state) != 0) {
			BT_ERR("Fail to get the lock_state value");
		}

		/*
		Issue: Pairing request pop appears in the Lock screen when DUT is locked
		and observed inconsistency. (TMWC-746)
		In platform image, don't have the additional logic to handle this.
		So just the set notification level as LOW.
		 */
		if (lock_state == VCONFKEY_IDLE_UNLOCK) {
			utilx_set_system_notification_level(ecore_x_display_get(),
					xwin, UTILX_NOTIFICATION_LEVEL_HIGH);
		} else
			utilx_set_system_notification_level(ecore_x_display_get(),
					xwin, UTILX_NOTIFICATION_LEVEL_LOW);
	}
}
#endif

static Evas_Object *__bluetooth_create_win(const char *name)
{
	Evas_Object *eo;
	eo = elm_win_add(NULL, name, ELM_WIN_BASIC);
	retv_if(!eo, NULL);

	elm_win_alpha_set(eo, EINA_TRUE);
	/* Handle rotation */
	if (elm_win_wm_rotation_supported_get(eo)) {
		int rots[4] = {0, 90, 180, 270};
		elm_win_wm_rotation_available_rotations_set(eo,
							(const int*)(&rots), 4);
	}
	elm_win_title_set(eo, name);
	elm_win_borderless_set(eo, EINA_TRUE);
#if 0
	ecore_x_window_size_get(ecore_x_window_root_first_get(),
				&w, &h);
	evas_object_resize(eo, w, h);
#endif
	return eo;
}

static void __bluetooth_session_init(struct bt_popup_appdata *ad)
{
	DBusGConnection *conn = NULL;
	GError *err = NULL;

#if 0
	g_type_init();
#endif

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
		BT_ERR("__bt_syspopup_init_app_signal failed");
}

static void __bluetooth_vconf_change_cb(keynode_t *key, void *data)
{
	retm_if(NULL == key, "key is NULL");
	retm_if(NULL == data, "data is NULL");
#if 0
	struct bt_popup_appdata *ad = data;
	char *vconf_name = vconf_keynode_get_name(key);

	if (!g_strcmp0(vconf_name, VCONFKEY_IDLE_LOCK_STATE) &&
		ad->popup)
		__bluetooth_set_win_level(ad->popup);
#endif
}

static bool __bluetooth_create(void *data)
{
	struct bt_popup_appdata *ad = data;
	Evas_Object *win = NULL;
	Evas_Object *conformant = NULL;
	Evas_Object *layout = NULL;
	int ret;

	BT_DBG("__bluetooth_create() start.");

	/* create window */
	win = __bluetooth_create_win(PACKAGE);
	retv_if(win == NULL, false);

#if 0
	/* Enable Changeable UI feature */
	ea_theme_changeable_ui_enabled_set(EINA_TRUE);
#endif

	evas_object_smart_callback_add(win, "wm,rotation,changed",
		__bt_main_win_rot_changed_cb, data);
	ad->win_main = win;

	conformant = elm_conformant_add(ad->win_main);
	retv_if(!conformant, false);

	elm_win_conformant_set(ad->win_main, EINA_TRUE);
	elm_win_resize_object_add(ad->win_main, conformant);
	evas_object_size_hint_weight_set(conformant, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(conformant, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(conformant);
	ad->conform = conformant;

	layout = elm_layout_add(conformant);
	elm_object_content_set(conformant, layout);
	ad->layout = layout;
	evas_object_show(layout);

	/* init internationalization */
	bindtextdomain(BT_COMMON_PKG, BT_LOCALEDIR);
	textdomain(BT_COMMON_PKG);

	ecore_imf_init();

	__bluetooth_session_init(ad);
	ret = vconf_notify_key_changed(VCONFKEY_IDLE_LOCK_STATE,
			__bluetooth_vconf_change_cb, ad);
	if (ret != 0)
		BT_ERR("vconf_notify_key_changed fail!");
	if (bt_initialize() != BT_ERROR_NONE) {
		BT_ERR("bt_initialize is failed");
	}

	return true;
}

static void __bluetooth_terminate(void *data)
{
	struct bt_popup_appdata *ad = data;

	if (bt_deinitialize() != BT_ERROR_NONE) {
		BT_ERR("bt_deinitialize is failed");
	}
	__bluetooth_ime_hide();

	if (ad->conn) {
		dbus_g_connection_unref(ad->conn);
		ad->conn = NULL;
	}

	if (ad->popup)
		evas_object_del(ad->popup);

	if (ad->win_main)
		evas_object_del(ad->win_main);

	ad->popup = NULL;
	ad->win_main = NULL;
}

static void __bluetooth_pause(void *data)
{
	return;
}

static void __bluetooth_resume(void *data)
{
	return;
}

static void __bluetooth_reset(app_control_h app_control, void *data)
{
	struct bt_popup_appdata *ad = data;
	bundle *b = NULL;
	char *event_type = NULL;
	char *operation = NULL;
	char *timeout = NULL;
	int ret = 0;

	BT_DBG("__bluetooth_reset()");

	if (ad == NULL) {
		BT_ERR("App data is NULL");
		return;
	}

	ret = app_control_export_as_bundle(app_control, &b);
	if (ret != 0) {
		BT_ERR("Failed to Convert the service handle to bundle data");
		free(b);
		return;
	}

	ret = app_control_get_extra_data(app_control, "timeout", &timeout);
	if (ret < 0)
		BT_ERR("Get data error");
	else {
		BT_INFO("Get visibility timeout : %s", timeout);
		if (timeout)
			ad->visibility_timeout = timeout;
	}

	/* Start Main UI */
	ret = bundle_get_str(b, "event-type", &event_type);
	if (ret != BUNDLE_ERROR_NONE)
		BT_ERR("bundle_get_str() is failed : %d\n", ret);

	/* Get app control operation */
	if (app_control_get_operation(app_control, &operation) < 0)
		BT_ERR("Get operation error");

	BT_INFO("operation: %s", operation);
	BT_DBG("event-type: %s", event_type);

	if (event_type != NULL) {
		if (!strcasecmp(event_type, "terminate")) {
			__bluetooth_win_del(ad);
			goto release;
		}

		if (syspopup_has_popup(b))
			__bluetooth_cleanup(ad); /* Destroy the existing popup*/

		__bluetooth_parse_event(ad, event_type);

		if (syspopup_reset(b) == -1 &&
				syspopup_create(b, &handler, ad->win_main, ad) == -1) {
			BT_ERR("Both syspopup_create and syspopup_reset failed");
			__bluetooth_remove_all_event(ad);
		} else {
			ret = __bluetooth_launch_handler(ad,
						       b, event_type);

			if (ret != 0)
				__bluetooth_remove_all_event(ad);

			__bluetooth_notify_event(ad);

			/* Change LCD brightness */
			ret = device_display_change_state(DISPLAY_STATE_NORMAL);
			if (ret != 0)
				BT_ERR("Fail to change LCD");
		}
	} else if (g_strcmp0(operation, APP_CONTROL_OPERATION_SETTING_BT_VISIBILITY) == 0) {
		BT_INFO("visibility operation");

		bundle_add_str(b, "_INTERNAL_SYSPOPUP_NAME_", "bt-syspopup");

		if (syspopup_has_popup(b))
			__bluetooth_cleanup(ad); /* Destroy the existing popup*/

		if (syspopup_create(b, &handler, ad->win_main, ad) == -1) {
			BT_ERR("Both syspopup_create and syspopup_reset failed");
			__bluetooth_remove_all_event(ad);
		} else {
			ret = __bluetooth_launch_handler(ad, b, "visibility-request");

			if (ret != 0)
				__bluetooth_remove_all_event(ad);

			__bluetooth_notify_event(ad);

			/* Change LCD brightness */
			ret = device_display_change_state(DISPLAY_STATE_NORMAL);
			if (ret != 0)
				BT_ERR("Fail to change LCD");
		}

	}
release:
	bundle_free(b);
}

EXPORT int main(int argc, char *argv[])
{
	BT_DBG("Start bt-syspopup main()");

	ui_app_lifecycle_callback_s callback = {0,};

	struct bt_popup_appdata ad = {0,};

	callback.create = __bluetooth_create;
	callback.terminate = __bluetooth_terminate;
	callback.pause = __bluetooth_pause;
	callback.resume = __bluetooth_resume;
	callback.app_control = __bluetooth_reset;

	BT_DBG("ui_app_main() is called.");
	int ret = ui_app_main(argc, argv, &callback, &ad);
	if (ret != APP_ERROR_NONE) {
		BT_ERR("ui_app_main() is failed. err = %d", ret);
	}

	BT_DBG("End bt-syspopup main()");
	return ret;
}

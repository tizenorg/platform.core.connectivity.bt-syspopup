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

#ifndef __DEF_BT_SYSPOPUP_H_
#define __DEF_BT_SYSPOPUP_H_

#include <Elementary.h>
#include <dlog.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#ifndef PREFIX
#define PREFIX "/usr"
#endif

#define TEMP_DIR	"/tmp"
#define PACKAGE		"bt-syspopup"
#define APPNAME		"bt-syspopup"
#define LOCALEDIR	PREFIX"/share/locale"
#define ICON_DIR	PREFIX"/share/icon"

#define BT_COMMON_PKG		"ug-setting-bluetooth-efl"
#define BT_COMMON_RES		"/opt/ug/res/locale"

#define _EDJ(obj) elm_layout_edje_get(obj)

#define BT_POPUP_ICON_CANCEL ICON_DIR"/01_header_icon_cancel.png"
#define BT_POPUP_ICON_DONE ICON_DIR"/01_header_icon_done.png"

#define BT_AUTHENTICATION_TIMEOUT		35
#define BT_AUTHORIZATION_TIMEOUT		15
#define BT_NOTIFICATION_TIMEOUT		2
#define BT_ERROR_TIMEOUT			1

#define BT_PIN_MLEN 16		/* Pin key max length */
#define BT_PK_MLEN 6		/* Passkey max length */
#define BT_CONTROLBAR_MAX_LENGTH 3

#define BT_GLOBALIZATION_STR_LENGTH 256
#define BT_DEVICE_NAME_LENGTH_MAX 256
#define BT_FILE_NAME_LENGTH_MAX 256
#define BT_TEXT_EXTRA_LEN 20

#define BT_TITLE_STR_MAX_LEN \
	(BT_GLOBALIZATION_STR_LENGTH+BT_DEVICE_NAME_LENGTH_MAX+BT_FILE_NAME_LENGTH_MAX)

#define BT_MESSAGE_STRING_SIZE 256*2+1

#define BT_POPUP	"BT_POPUP"

#define bt_log_print(tag, format, args...) LOG(LOG_DEBUG, \
	tag, "%s:%d "format, __func__, __LINE__, ##args)

#define BT_SYS_POPUP_IPC_NAME "org.projectx"
#define BT_SYS_POPUP_IPC_RESPONSE_OBJECT "/org/projectx/bt_syspopup_res"
#define BT_SYS_POPUP_INTERFACE "User.Bluetooth.syspopup"
#define BT_SYS_POPUP_METHOD_RESPONSE "Response"

/* String defines to support multi-languages */
#define BT_STR_ENTER_PIN	\
	dgettext(BT_COMMON_PKG, "IDS_BT_HEADER_ENTERPIN")
#define BT_STR_ALLOW_PS_TO_CONNECT_Q	\
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_ALLOW_PS_TO_CONNECT_Q")

#define BT_STR_RECEIVE_PS_FROM_PS_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_RECEIVE_PS_FROM_PS_Q")

#define BT_STR_PASSKEY_MATCH_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_MATCH_PASSKEYS_ON_PS_Q")

#define BT_STR_OVERWRITE_FILE_Q \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_PS_ALREADY_EXISTS_OVERWRITE_Q")

/* We will add these strings in BT_COMMON_PKG to translate */
#define BT_STR_DISABLED_RESTRICTS \
	"Security policy restricts use of Bluletooth connection"

#define BT_STR_HANDS_FREE_RESTRICTS \
	"Security policy restricts use of Bluletooth connection to hands-free features only"

#define BT_STR_OK dgettext("sys_string", "IDS_COM_SK_OK")
#define BT_STR_YES dgettext("sys_string", "IDS_COM_SK_YES")
#define BT_STR_NO dgettext("sys_string", "IDS_COM_SK_NO")
#define BT_STR_DONE dgettext("sys_string", "IDS_COM_SK_DONE")
#define BT_STR_CANCEL dgettext("sys_string", "IDS_COM_SK_CANCEL")


typedef enum {
	BT_CHANGED_MODE_ENABLE,
	BT_CHANGED_MODE_DISABLE,
} bt_changed_mode_type_t;

typedef enum {
	BT_EVENT_PIN_REQUEST = 0x0001,
	BT_EVENT_PASSKEY_CONFIRM_REQUEST = 0x0002,
	BT_EVENT_PASSKEY_REQUEST = 0x0004,
	BT_EVENT_PASSKEY_DISPLAY_REQUEST = 0x0008,
	BT_EVENT_AUTHORIZE_REQUEST = 0x0010,
	BT_EVENT_APP_CONFIRM_REQUEST = 0x0020,
	BT_EVENT_PUSH_AUTHORIZE_REQUEST = 0x0040,
	BT_EVENT_CONFIRM_OVERWRITE_REQUEST = 0x0080,
	BT_EVENT_FILE_RECIEVED = 0x0100,
	BT_EVENT_KEYBOARD_PASSKEY_REQUEST = 0x0200,
	BT_EVENT_INFORMATION = 0x0400,
	BT_EVENT_TERMINATE = 0x0800,
} bt_popup_event_type_t;

typedef enum {
	BT_AGENT_ACCEPT,
	BT_AGENT_REJECT,
	BT_AGENT_CANCEL,
	BT_CORE_AGENT_TIMEOUT,
} bt_agent_accept_type_t;

struct bt_popup_appdata {
	Evas *evas;
	Evas_Object *win_main;
	Evas_Object *layout_main;
	Evas_Object *popup;

	/* Passkey layout objects */
	Evas_Object *navi_fr;
	Evas_Object *entry;
	Evas_Object *edit_field_save_btn;
	Evas_Object *ticker_noti;

	Ecore_Timer *timer;
	Ecore_Event_Handler *event_handle;

	DBusGProxy *agent_proxy;
	DBusGProxy *obex_proxy;
	E_DBus_Connection *EDBusHandle;

	Elm_Genlist_Item_Class sp_itc;
	Elm_Genlist_Item_Class itc;

	char passkey[BT_PK_MLEN + 1];

	int changed_mode;
	bt_popup_event_type_t event_type;
};

#endif				/* __DEF_BT_SYSPOPUP_H_ */

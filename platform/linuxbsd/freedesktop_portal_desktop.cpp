/**************************************************************************/
/*  freedesktop_portal_desktop.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "freedesktop_portal_desktop.h"

#ifdef DBUS_ENABLED

#include "core/crypto/crypto_core.h"
#include "core/error/error_macros.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

#ifdef SOWRAP_ENABLED
#include "dbus-so_wrap.h"
#else
#include <dbus/dbus.h>
#endif

#include <unistd.h>

#define BUS_OBJECT_NAME "org.freedesktop.portal.Desktop"
#define BUS_OBJECT_PATH "/org/freedesktop/portal/desktop"

#define BUS_INTERFACE_PROPERTIES "org.freedesktop.DBus.Properties"
#define BUS_INTERFACE_SETTINGS "org.freedesktop.portal.Settings"
#define BUS_INTERFACE_FILE_CHOOSER "org.freedesktop.portal.FileChooser"
#define BUS_INTERFACE_SCREENSHOT "org.freedesktop.portal.Screenshot"

bool FreeDesktopPortalDesktop::try_parse_variant(DBusMessage *p_reply_message, ReadVariantType p_type, void *r_value) {
	DBusMessageIter iter[3];

	dbus_message_iter_init(p_reply_message, &iter[0]);
	if (dbus_message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT) {
		return false;
	}

	dbus_message_iter_recurse(&iter[0], &iter[1]);
	if (dbus_message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT) {
		return false;
	}

	dbus_message_iter_recurse(&iter[1], &iter[2]);
	if (p_type == VAR_TYPE_COLOR) {
		if (dbus_message_iter_get_arg_type(&iter[2]) != DBUS_TYPE_STRUCT) {
			return false;
		}
		DBusMessageIter struct_iter;
		dbus_message_iter_recurse(&iter[2], &struct_iter);
		int idx = 0;
		while (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_DOUBLE) {
			double value = 0.0;
			dbus_message_iter_get_basic(&struct_iter, &value);
			if (value < 0.0 || value > 1.0) {
				return false;
			}
			if (idx == 0) {
				static_cast<Color *>(r_value)->r = value;
			} else if (idx == 1) {
				static_cast<Color *>(r_value)->g = value;
			} else if (idx == 2) {
				static_cast<Color *>(r_value)->b = value;
			}
			idx++;
			if (!dbus_message_iter_next(&struct_iter)) {
				break;
			}
		}
		if (idx != 3) {
			return false;
		}
	} else if (p_type == VAR_TYPE_UINT32) {
		if (dbus_message_iter_get_arg_type(&iter[2]) != DBUS_TYPE_UINT32) {
			return false;
		}
		dbus_message_iter_get_basic(&iter[2], r_value);
	} else if (p_type == VAR_TYPE_BOOL) {
		if (dbus_message_iter_get_arg_type(&iter[2]) != DBUS_TYPE_BOOLEAN) {
			return false;
		}
		dbus_message_iter_get_basic(&iter[2], r_value);
	}
	return true;
}

bool FreeDesktopPortalDesktop::read_setting(const char *p_namespace, const char *p_key, ReadVariantType p_type, void *r_value) {
	if (unsupported) {
		return false;
	}

	DBusError error;
	dbus_error_init(&error);

	DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (dbus_error_is_set(&error)) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			ERR_PRINT(vformat("Error opening D-Bus connection: %s", error.message));
		}
		dbus_error_free(&error);
		unsupported = true;
		return false;
	}

	DBusMessage *message = dbus_message_new_method_call(
			BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE_SETTINGS,
			"Read");
	dbus_message_append_args(
			message,
			DBUS_TYPE_STRING, &p_namespace,
			DBUS_TYPE_STRING, &p_key,
			DBUS_TYPE_INVALID);

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 50, &error);
	dbus_message_unref(message);
	if (dbus_error_is_set(&error)) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			ERR_PRINT(vformat("Error on D-Bus communication: %s", error.message));
		}
		dbus_error_free(&error);
		dbus_connection_unref(bus);
		return false;
	}

	bool success = try_parse_variant(reply, p_type, r_value);

	dbus_message_unref(reply);
	dbus_connection_unref(bus);

	return success;
}

uint32_t FreeDesktopPortalDesktop::get_appearance_color_scheme() {
	if (unsupported) {
		return 0;
	}

	uint32_t value = 0;
	if (read_setting("org.freedesktop.appearance", "color-scheme", VAR_TYPE_UINT32, &value)) {
		return value;
	} else {
		return 0;
	}
}

Color FreeDesktopPortalDesktop::get_appearance_accent_color() {
	if (unsupported) {
		return Color(0, 0, 0, 0);
	}

	Color value;
	if (read_setting("org.freedesktop.appearance", "accent-color", VAR_TYPE_COLOR, &value)) {
		return value;
	} else {
		return Color(0, 0, 0, 0);
	}
}

uint32_t FreeDesktopPortalDesktop::get_high_contrast() {
	if (unsupported) {
		return -1;
	}

	dbus_bool_t value = false;
	if (read_setting("org.gnome.desktop.a11y.interface", "high-contrast", VAR_TYPE_BOOL, &value)) {
		return value;
	}
	return -1;
}

static const char *cs_empty = "";

void FreeDesktopPortalDesktop::append_dbus_string(DBusMessageIter *p_iter, const String &p_string) {
	CharString cs = p_string.utf8();
	const char *cs_ptr = cs.ptr();
	if (cs_ptr) {
		dbus_message_iter_append_basic(p_iter, DBUS_TYPE_STRING, &cs_ptr);
	} else {
		dbus_message_iter_append_basic(p_iter, DBUS_TYPE_STRING, &cs_empty);
	}
}

void FreeDesktopPortalDesktop::append_dbus_dict_options(DBusMessageIter *p_iter, const TypedArray<Dictionary> &p_options, HashMap<String, String> &r_ids) {
	DBusMessageIter dict_iter;
	DBusMessageIter var_iter;
	DBusMessageIter arr_iter;
	const char *choices_key = "choices";

	dbus_message_iter_open_container(p_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_iter);
	dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &choices_key);
	dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "a(ssa(ss)s)", &var_iter);
	dbus_message_iter_open_container(&var_iter, DBUS_TYPE_ARRAY, "(ss(ss)s)", &arr_iter);

	r_ids.clear();
	for (int i = 0; i < p_options.size(); i++) {
		const Dictionary &item = p_options[i];
		if (!item.has("name") || !item.has("values") || !item.has("default")) {
			continue;
		}
		const String &name = item["name"];
		const Vector<String> &options = item["values"];
		int default_idx = item["default"];

		DBusMessageIter struct_iter;
		DBusMessageIter array_iter;
		DBusMessageIter array_struct_iter;
		dbus_message_iter_open_container(&arr_iter, DBUS_TYPE_STRUCT, nullptr, &struct_iter);
		append_dbus_string(&struct_iter, "option_" + itos(i)); // ID.
		append_dbus_string(&struct_iter, name); // User visible name.
		r_ids["option_" + itos(i)] = name;

		dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "(ss)", &array_iter);
		for (int j = 0; j < options.size(); j++) {
			dbus_message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, nullptr, &array_struct_iter);
			append_dbus_string(&array_struct_iter, itos(j));
			append_dbus_string(&array_struct_iter, options[j]);
			dbus_message_iter_close_container(&array_iter, &array_struct_iter);
		}
		dbus_message_iter_close_container(&struct_iter, &array_iter);
		if (options.is_empty()) {
			append_dbus_string(&struct_iter, (default_idx) ? "true" : "false"); // Default selection.
		} else {
			append_dbus_string(&struct_iter, itos(default_idx)); // Default selection.
		}

		dbus_message_iter_close_container(&arr_iter, &struct_iter);
	}
	dbus_message_iter_close_container(&var_iter, &arr_iter);
	dbus_message_iter_close_container(&dict_iter, &var_iter);
	dbus_message_iter_close_container(p_iter, &dict_iter);
}

void FreeDesktopPortalDesktop::append_dbus_dict_filters(DBusMessageIter *p_iter, const Vector<String> &p_filter_names, const Vector<String> &p_filter_exts, const Vector<String> &p_filter_mimes) {
	DBusMessageIter dict_iter;
	DBusMessageIter var_iter;
	DBusMessageIter arr_iter;
	const char *filters_key = "filters";

	ERR_FAIL_COND(p_filter_names.size() != p_filter_exts.size());
	ERR_FAIL_COND(p_filter_names.size() != p_filter_mimes.size());

	dbus_message_iter_open_container(p_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_iter);
	dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &filters_key);
	dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "a(sa(us))", &var_iter);
	dbus_message_iter_open_container(&var_iter, DBUS_TYPE_ARRAY, "(sa(us))", &arr_iter);
	for (int i = 0; i < p_filter_names.size(); i++) {
		DBusMessageIter struct_iter;
		DBusMessageIter array_iter;
		DBusMessageIter array_struct_iter;
		dbus_message_iter_open_container(&arr_iter, DBUS_TYPE_STRUCT, nullptr, &struct_iter);
		append_dbus_string(&struct_iter, p_filter_names[i]);

		dbus_message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "(us)", &array_iter);
		const String &flt_orig = p_filter_exts[i];
		String flt;
		for (int j = 0; j < flt_orig.length(); j++) {
			if (is_unicode_letter(flt_orig[j])) {
				flt += vformat("[%c%c]", String::char_lowercase(flt_orig[j]), String::char_uppercase(flt_orig[j]));
			} else {
				flt += flt_orig[j];
			}
		}
		int filter_slice_count = flt.get_slice_count(",");
		for (int j = 0; j < filter_slice_count; j++) {
			dbus_message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, nullptr, &array_struct_iter);
			String str = (flt.get_slicec(',', j).strip_edges());
			{
				const unsigned flt_type = 0;
				dbus_message_iter_append_basic(&array_struct_iter, DBUS_TYPE_UINT32, &flt_type);
			}
			append_dbus_string(&array_struct_iter, str);
			dbus_message_iter_close_container(&array_iter, &array_struct_iter);
		}
		const String &mime = p_filter_mimes[i];
		filter_slice_count = mime.get_slice_count(",");
		for (int j = 0; j < filter_slice_count; j++) {
			dbus_message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, nullptr, &array_struct_iter);
			String str = mime.get_slicec(',', j).strip_edges();
			{
				const unsigned flt_type = 1;
				dbus_message_iter_append_basic(&array_struct_iter, DBUS_TYPE_UINT32, &flt_type);
			}
			append_dbus_string(&array_struct_iter, str);
			dbus_message_iter_close_container(&array_iter, &array_struct_iter);
		}
		dbus_message_iter_close_container(&struct_iter, &array_iter);
		dbus_message_iter_close_container(&arr_iter, &struct_iter);
	}
	dbus_message_iter_close_container(&var_iter, &arr_iter);
	dbus_message_iter_close_container(&dict_iter, &var_iter);
	dbus_message_iter_close_container(p_iter, &dict_iter);
}

void FreeDesktopPortalDesktop::append_dbus_dict_string(DBusMessageIter *p_iter, const String &p_key, const String &p_value, bool p_as_byte_array) {
	DBusMessageIter dict_iter;
	DBusMessageIter var_iter;
	dbus_message_iter_open_container(p_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_iter);
	append_dbus_string(&dict_iter, p_key);

	if (p_as_byte_array) {
		DBusMessageIter arr_iter;
		dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "ay", &var_iter);
		dbus_message_iter_open_container(&var_iter, DBUS_TYPE_ARRAY, "y", &arr_iter);
		CharString cs = p_value.utf8();
		const char *cs_ptr = cs.get_data();
		do {
			dbus_message_iter_append_basic(&arr_iter, DBUS_TYPE_BYTE, cs_ptr);
		} while (*cs_ptr++);
		dbus_message_iter_close_container(&var_iter, &arr_iter);
	} else {
		dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "s", &var_iter);
		append_dbus_string(&var_iter, p_value);
	}

	dbus_message_iter_close_container(&dict_iter, &var_iter);
	dbus_message_iter_close_container(p_iter, &dict_iter);
}

void FreeDesktopPortalDesktop::append_dbus_dict_bool(DBusMessageIter *p_iter, const String &p_key, bool p_value) {
	DBusMessageIter dict_iter;
	DBusMessageIter var_iter;
	dbus_message_iter_open_container(p_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_iter);
	append_dbus_string(&dict_iter, p_key);

	dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "b", &var_iter);
	{
		int val = p_value;
		dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_BOOLEAN, &val);
	}

	dbus_message_iter_close_container(&dict_iter, &var_iter);
	dbus_message_iter_close_container(p_iter, &dict_iter);
}

bool FreeDesktopPortalDesktop::color_picker_parse_response(DBusMessageIter *p_iter, bool &r_cancel, Color &r_color) {
	ERR_FAIL_COND_V(dbus_message_iter_get_arg_type(p_iter) != DBUS_TYPE_UINT32, false);

	dbus_uint32_t resp_code;
	dbus_message_iter_get_basic(p_iter, &resp_code);
	if (resp_code != 0) {
		r_cancel = true;
	} else {
		r_cancel = false;
		ERR_FAIL_COND_V(!dbus_message_iter_next(p_iter), false);
		ERR_FAIL_COND_V(dbus_message_iter_get_arg_type(p_iter) != DBUS_TYPE_ARRAY, false);

		DBusMessageIter dict_iter;
		dbus_message_iter_recurse(p_iter, &dict_iter);
		while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter iter;
			dbus_message_iter_recurse(&dict_iter, &iter);
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
				const char *key;
				dbus_message_iter_get_basic(&iter, &key);
				dbus_message_iter_next(&iter);

				DBusMessageIter var_iter;
				dbus_message_iter_recurse(&iter, &var_iter);
				if (strcmp(key, "color") == 0) { // (ddd)
					if (dbus_message_iter_get_arg_type(&var_iter) == DBUS_TYPE_STRUCT) {
						DBusMessageIter struct_iter;
						dbus_message_iter_recurse(&var_iter, &struct_iter);
						int idx = 0;
						while (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_DOUBLE) {
							double value = 0.0;
							dbus_message_iter_get_basic(&struct_iter, &value);
							if (idx == 0) {
								r_color.r = value;
							} else if (idx == 1) {
								r_color.g = value;
							} else if (idx == 2) {
								r_color.b = value;
							}
							idx++;
							if (!dbus_message_iter_next(&struct_iter)) {
								break;
							}
						}
					}
				}
			}
			if (!dbus_message_iter_next(&dict_iter)) {
				break;
			}
		}
	}
	return true;
}

bool FreeDesktopPortalDesktop::file_chooser_parse_response(DBusMessageIter *p_iter, const Vector<String> &p_names, const HashMap<String, String> &p_ids, bool &r_cancel, Vector<String> &r_urls, int &r_index, Dictionary &r_options) {
	ERR_FAIL_COND_V(dbus_message_iter_get_arg_type(p_iter) != DBUS_TYPE_UINT32, false);

	dbus_uint32_t resp_code;
	dbus_message_iter_get_basic(p_iter, &resp_code);
	if (resp_code != 0) {
		r_cancel = true;
	} else {
		r_cancel = false;
		ERR_FAIL_COND_V(!dbus_message_iter_next(p_iter), false);
		ERR_FAIL_COND_V(dbus_message_iter_get_arg_type(p_iter) != DBUS_TYPE_ARRAY, false);

		DBusMessageIter dict_iter;
		dbus_message_iter_recurse(p_iter, &dict_iter);
		while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter iter;
			dbus_message_iter_recurse(&dict_iter, &iter);
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
				const char *key;
				dbus_message_iter_get_basic(&iter, &key);
				dbus_message_iter_next(&iter);

				DBusMessageIter var_iter;
				dbus_message_iter_recurse(&iter, &var_iter);
				if (strcmp(key, "current_filter") == 0) { // (sa(us))
					if (dbus_message_iter_get_arg_type(&var_iter) == DBUS_TYPE_STRUCT) {
						DBusMessageIter struct_iter;
						dbus_message_iter_recurse(&var_iter, &struct_iter);
						while (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_STRING) {
							const char *value;
							dbus_message_iter_get_basic(&struct_iter, &value);
							String name = String::utf8(value);

							r_index = p_names.find(name);
							if (!dbus_message_iter_next(&struct_iter)) {
								break;
							}
						}
					}
				} else if (strcmp(key, "choices") == 0) { // a(ss) {
					if (dbus_message_iter_get_arg_type(&var_iter) == DBUS_TYPE_ARRAY) {
						DBusMessageIter struct_iter;
						dbus_message_iter_recurse(&var_iter, &struct_iter);
						while (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_STRUCT) {
							DBusMessageIter opt_iter;
							dbus_message_iter_recurse(&struct_iter, &opt_iter);
							const char *opt_key = nullptr;
							dbus_message_iter_get_basic(&opt_iter, &opt_key);
							String opt_skey = String::utf8(opt_key);
							dbus_message_iter_next(&opt_iter);
							const char *opt_val = nullptr;
							dbus_message_iter_get_basic(&opt_iter, &opt_val);
							String opt_sval = String::utf8(opt_val);

							if (p_ids.has(opt_skey)) {
								opt_skey = p_ids[opt_skey];
								if (opt_sval == "true") {
									r_options[opt_skey] = true;
								} else if (opt_sval == "false") {
									r_options[opt_skey] = false;
								} else {
									r_options[opt_skey] = opt_sval.to_int();
								}
							}

							if (!dbus_message_iter_next(&struct_iter)) {
								break;
							}
						}
					}
				} else if (strcmp(key, "uris") == 0) { // as
					if (dbus_message_iter_get_arg_type(&var_iter) == DBUS_TYPE_ARRAY) {
						DBusMessageIter uri_iter;
						dbus_message_iter_recurse(&var_iter, &uri_iter);
						while (dbus_message_iter_get_arg_type(&uri_iter) == DBUS_TYPE_STRING) {
							const char *value;
							dbus_message_iter_get_basic(&uri_iter, &value);
							r_urls.push_back(String::utf8(value).trim_prefix("file://").uri_file_decode());
							if (!dbus_message_iter_next(&uri_iter)) {
								break;
							}
						}
					}
				}
			}
			if (!dbus_message_iter_next(&dict_iter)) {
				break;
			}
		}
	}
	return true;
}

bool FreeDesktopPortalDesktop::color_picker(const String &p_xid, const Callable &p_callback) {
	if (unsupported) {
		return false;
	}

	DBusError err;
	dbus_error_init(&err);

	// Open connection and add signal handler.
	ColorPickerData cd;
	cd.callback = p_callback;

	CryptoCore::RandomGenerator rng;
	ERR_FAIL_COND_V_MSG(rng.init(), false, "Failed to initialize random number generator.");
	uint8_t uuid[64];
	Error rng_err = rng.get_random_bytes(uuid, 64);
	ERR_FAIL_COND_V_MSG(rng_err, false, "Failed to generate unique token.");

	String dbus_unique_name = String::utf8(dbus_bus_get_unique_name(monitor_connection));
	String token = String::hex_encode_buffer(uuid, 64);
	String path = vformat("/org/freedesktop/portal/desktop/request/%s/%s", dbus_unique_name.replace_char('.', '_').remove_char(':'), token);

	cd.path = path;
	cd.filter = vformat("type='signal',sender='org.freedesktop.portal.Desktop',path='%s',interface='org.freedesktop.portal.Request',member='Response',destination='%s'", path, dbus_unique_name);
	dbus_bus_add_match(monitor_connection, cd.filter.utf8().get_data(), &err);
	if (dbus_error_is_set(&err)) {
		ERR_PRINT(vformat("Failed to add DBus match: %s", err.message));
		dbus_error_free(&err);
		return false;
	}

	DBusMessage *message = dbus_message_new_method_call(BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE_SCREENSHOT, "PickColor");
	{
		DBusMessageIter iter;
		dbus_message_iter_init_append(message, &iter);

		append_dbus_string(&iter, p_xid);

		DBusMessageIter arr_iter;
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &arr_iter);
		append_dbus_dict_string(&arr_iter, "handle_token", token);
		dbus_message_iter_close_container(&iter, &arr_iter);
	}
	DBusMessage *reply = dbus_connection_send_with_reply_and_block(monitor_connection, message, DBUS_TIMEOUT_INFINITE, &err);
	dbus_message_unref(message);

	if (!reply || dbus_error_is_set(&err)) {
		ERR_PRINT(vformat("Failed to send DBus message: %s", err.message));
		dbus_error_free(&err);
		dbus_bus_remove_match(monitor_connection, cd.filter.utf8().get_data(), &err);
		return false;
	}

	// Update signal path.
	{
		DBusMessageIter iter;
		if (dbus_message_iter_init(reply, &iter)) {
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_OBJECT_PATH) {
				const char *new_path = nullptr;
				dbus_message_iter_get_basic(&iter, &new_path);
				if (String::utf8(new_path) != path) {
					dbus_bus_remove_match(monitor_connection, cd.filter.utf8().get_data(), &err);
					if (dbus_error_is_set(&err)) {
						ERR_PRINT(vformat("Failed to remove DBus match: %s", err.message));
						dbus_error_free(&err);
						return false;
					}
					cd.filter = String::utf8(new_path);
					dbus_bus_add_match(monitor_connection, cd.filter.utf8().get_data(), &err);
					if (dbus_error_is_set(&err)) {
						ERR_PRINT(vformat("Failed to add DBus match: %s", err.message));
						dbus_error_free(&err);
						return false;
					}
				}
			}
		}
	}
	dbus_message_unref(reply);

	MutexLock lock(color_picker_mutex);
	color_pickers.push_back(cd);

	return true;
}

bool FreeDesktopPortalDesktop::_is_interface_supported(const char *p_iface) {
	bool supported = false;
	DBusError err;
	dbus_error_init(&err);
	DBusConnection *bus = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		dbus_error_free(&err);
	} else {
		DBusMessage *message = dbus_message_new_method_call(BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE_PROPERTIES, "Get");
		if (message) {
			const char *name_space = p_iface;
			const char *key = "version";
			dbus_message_append_args(
					message,
					DBUS_TYPE_STRING, &name_space,
					DBUS_TYPE_STRING, &key,
					DBUS_TYPE_INVALID);
			DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 250, &err);
			if (dbus_error_is_set(&err)) {
				dbus_error_free(&err);
			} else if (reply) {
				DBusMessageIter iter;
				if (dbus_message_iter_init(reply, &iter)) {
					DBusMessageIter iter_ver;
					dbus_message_iter_recurse(&iter, &iter_ver);
					dbus_uint32_t ver_code;
					dbus_message_iter_get_basic(&iter_ver, &ver_code);
					print_verbose(vformat("PortalDesktop: %s version %d detected.", p_iface, ver_code));
					supported = true;
				}
				dbus_message_unref(reply);
			}
			dbus_message_unref(message);
		}
		dbus_connection_unref(bus);
	}
	return supported;
}

bool FreeDesktopPortalDesktop::is_file_chooser_supported() {
	static int supported = -1;
	if (supported == -1) {
		supported = _is_interface_supported(BUS_INTERFACE_FILE_CHOOSER);
	}
	return supported;
}

bool FreeDesktopPortalDesktop::is_settings_supported() {
	static int supported = -1;
	if (supported == -1) {
		supported = _is_interface_supported(BUS_INTERFACE_SETTINGS);
	}
	return supported;
}

bool FreeDesktopPortalDesktop::is_screenshot_supported() {
	static int supported = -1;
	if (supported == -1) {
		supported = _is_interface_supported(BUS_INTERFACE_SCREENSHOT);
	}
	return supported;
}

Error FreeDesktopPortalDesktop::file_dialog_show(DisplayServer::WindowID p_window_id, const String &p_xid, const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, DisplayServer::FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback, bool p_options_in_cb) {
	if (unsupported) {
		return FAILED;
	}

	ERR_FAIL_INDEX_V(int(p_mode), DisplayServer::FILE_DIALOG_MODE_SAVE_MAX, FAILED);
	ERR_FAIL_NULL_V(monitor_connection, FAILED);

	Vector<String> filter_names;
	Vector<String> filter_exts;
	Vector<String> filter_mimes;
	for (int i = 0; i < p_filters.size(); i++) {
		Vector<String> tokens = p_filters[i].split(";");
		if (tokens.size() >= 1) {
			String flt = tokens[0].strip_edges();
			String mime = (tokens.size() >= 3) ? tokens[2].strip_edges() : String();
			if (!flt.is_empty() || !mime.is_empty()) {
				if (tokens.size() >= 2) {
					if (flt == "*.*") {
						filter_exts.push_back("*");
					} else {
						filter_exts.push_back(flt);
					}
					filter_mimes.push_back(mime);
					filter_names.push_back(tokens[1]);
				} else {
					if (flt == "*.*") {
						filter_exts.push_back("*");
						filter_names.push_back(RTR("All Files") + " (*.*)");
					} else {
						filter_exts.push_back(flt);
						filter_names.push_back(flt);
					}
					filter_mimes.push_back(mime);
				}
			}
		}
	}
	if (filter_names.is_empty()) {
		filter_exts.push_back("*");
		filter_mimes.push_back("");
		filter_names.push_back(RTR("All Files") + " (*.*)");
	}

	DBusError err;
	dbus_error_init(&err);

	// Open connection and add signal handler.
	FileDialogData fd;
	fd.callback = p_callback;
	fd.prev_focus = p_window_id;
	fd.filter_names = filter_names;
	fd.opt_in_cb = p_options_in_cb;

	CryptoCore::RandomGenerator rng;
	ERR_FAIL_COND_V_MSG(rng.init(), FAILED, "Failed to initialize random number generator.");
	uint8_t uuid[64];
	Error rng_err = rng.get_random_bytes(uuid, 64);
	ERR_FAIL_COND_V_MSG(rng_err, rng_err, "Failed to generate unique token.");

	String dbus_unique_name = String::utf8(dbus_bus_get_unique_name(monitor_connection));
	String token = String::hex_encode_buffer(uuid, 64);
	String path = vformat("/org/freedesktop/portal/desktop/request/%s/%s", dbus_unique_name.replace_char('.', '_').remove_char(':'), token);

	fd.path = path;
	fd.filter = vformat("type='signal',sender='org.freedesktop.portal.Desktop',path='%s',interface='org.freedesktop.portal.Request',member='Response',destination='%s'", path, dbus_unique_name);
	dbus_bus_add_match(monitor_connection, fd.filter.utf8().get_data(), &err);
	if (dbus_error_is_set(&err)) {
		ERR_PRINT(vformat("Failed to add DBus match: %s", err.message));
		dbus_error_free(&err);
		return FAILED;
	}

	// Generate FileChooser message.
	const char *method = nullptr;
	if (p_mode == DisplayServer::FILE_DIALOG_MODE_SAVE_FILE) {
		method = "SaveFile";
	} else {
		method = "OpenFile";
	}

	DBusMessage *message = dbus_message_new_method_call(BUS_OBJECT_NAME, BUS_OBJECT_PATH, BUS_INTERFACE_FILE_CHOOSER, method);
	{
		DBusMessageIter iter;
		dbus_message_iter_init_append(message, &iter);

		append_dbus_string(&iter, p_xid);
		append_dbus_string(&iter, p_title);

		DBusMessageIter arr_iter;
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &arr_iter);

		append_dbus_dict_string(&arr_iter, "handle_token", token);
		append_dbus_dict_bool(&arr_iter, "multiple", p_mode == DisplayServer::FILE_DIALOG_MODE_OPEN_FILES);
		append_dbus_dict_bool(&arr_iter, "directory", p_mode == DisplayServer::FILE_DIALOG_MODE_OPEN_DIR);
		append_dbus_dict_filters(&arr_iter, filter_names, filter_exts, filter_mimes);

		append_dbus_dict_options(&arr_iter, p_options, fd.option_ids);
		append_dbus_dict_string(&arr_iter, "current_folder", p_current_directory, true);
		if (p_mode == DisplayServer::FILE_DIALOG_MODE_SAVE_FILE) {
			append_dbus_dict_string(&arr_iter, "current_name", p_filename);
		}

		dbus_message_iter_close_container(&iter, &arr_iter);
	}

	DBusMessage *reply = dbus_connection_send_with_reply_and_block(monitor_connection, message, DBUS_TIMEOUT_INFINITE, &err);
	dbus_message_unref(message);

	if (!reply || dbus_error_is_set(&err)) {
		ERR_PRINT(vformat("Failed to send DBus message: %s", err.message));
		dbus_error_free(&err);
		dbus_bus_remove_match(monitor_connection, fd.filter.utf8().get_data(), &err);
		return FAILED;
	}

	// Update signal path.
	{
		DBusMessageIter iter;
		if (dbus_message_iter_init(reply, &iter)) {
			if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_OBJECT_PATH) {
				const char *new_path = nullptr;
				dbus_message_iter_get_basic(&iter, &new_path);
				if (String::utf8(new_path) != path) {
					dbus_bus_remove_match(monitor_connection, fd.filter.utf8().get_data(), &err);
					if (dbus_error_is_set(&err)) {
						ERR_PRINT(vformat("Failed to remove DBus match: %s", err.message));
						dbus_error_free(&err);
						return FAILED;
					}
					fd.filter = String::utf8(new_path);
					dbus_bus_add_match(monitor_connection, fd.filter.utf8().get_data(), &err);
					if (dbus_error_is_set(&err)) {
						ERR_PRINT(vformat("Failed to add DBus match: %s", err.message));
						dbus_error_free(&err);
						return FAILED;
					}
				}
			}
		}
	}
	dbus_message_unref(reply);

	MutexLock lock(file_dialog_mutex);
	file_dialogs.push_back(fd);

	return OK;
}

void FreeDesktopPortalDesktop::process_callbacks() {
	{
		MutexLock lock(file_dialog_mutex);
		while (!pending_file_cbs.is_empty()) {
			FileDialogCallback cb = pending_file_cbs.front()->get();
			pending_file_cbs.pop_front();

			if (cb.opt_in_cb) {
				Variant ret;
				Callable::CallError ce;
				const Variant *args[4] = { &cb.status, &cb.files, &cb.index, &cb.options };

				cb.callback.callp(args, 4, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute file dialog callback: %s.", Variant::get_callable_error_text(cb.callback, args, 4, ce)));
				}
			} else {
				Variant ret;
				Callable::CallError ce;
				const Variant *args[3] = { &cb.status, &cb.files, &cb.index };

				cb.callback.callp(args, 3, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute file dialog callback: %s.", Variant::get_callable_error_text(cb.callback, args, 3, ce)));
				}
			}
		}
	}
	{
		MutexLock lock(color_picker_mutex);
		while (!pending_color_cbs.is_empty()) {
			ColorPickerCallback cb = pending_color_cbs.front()->get();
			pending_color_cbs.pop_front();

			Variant ret;
			Callable::CallError ce;
			const Variant *args[2] = { &cb.status, &cb.color };

			cb.callback.callp(args, 2, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Failed to execute color picker callback: %s.", Variant::get_callable_error_text(cb.callback, args, 2, ce)));
			}
		}
	}
}

void FreeDesktopPortalDesktop::_thread_monitor(void *p_ud) {
	FreeDesktopPortalDesktop *portal = (FreeDesktopPortalDesktop *)p_ud;

	while (!portal->monitor_thread_abort.is_set()) {
		if (portal->monitor_connection) {
			while (true) {
				DBusMessage *msg = dbus_connection_pop_message(portal->monitor_connection);
				if (!msg) {
					break;
				} else if (dbus_message_is_signal(msg, "org.freedesktop.portal.Settings", "SettingChanged")) {
					DBusMessageIter iter;
					if (dbus_message_iter_init(msg, &iter)) {
						const char *value;
						dbus_message_iter_get_basic(&iter, &value);
						String name_space = String::utf8(value);
						dbus_message_iter_next(&iter);
						dbus_message_iter_get_basic(&iter, &value);
						String key = String::utf8(value);

						if (name_space == "org.freedesktop.appearance" && (key == "color-scheme" || key == "accent-color")) {
							callable_mp(portal, &FreeDesktopPortalDesktop::_system_theme_changed_callback).call_deferred();
						}
					}
				} else if (dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response")) {
					String path = String::utf8(dbus_message_get_path(msg));
					{
						MutexLock lock(portal->file_dialog_mutex);
						for (int i = 0; i < portal->file_dialogs.size(); i++) {
							FreeDesktopPortalDesktop::FileDialogData &fd = portal->file_dialogs.write[i];
							if (fd.path == path) {
								DBusMessageIter iter;
								if (dbus_message_iter_init(msg, &iter)) {
									bool cancel = false;
									Vector<String> uris;
									Dictionary options;
									int index = 0;
									file_chooser_parse_response(&iter, fd.filter_names, fd.option_ids, cancel, uris, index, options);

									if (fd.callback.is_valid()) {
										FileDialogCallback cb;
										cb.callback = fd.callback;
										cb.status = !cancel;
										cb.files = uris;
										cb.index = index;
										cb.options = options;
										cb.opt_in_cb = fd.opt_in_cb;
										portal->pending_file_cbs.push_back(cb);
									}
									if (fd.prev_focus != DisplayServer::INVALID_WINDOW_ID) {
										callable_mp(DisplayServer::get_singleton(), &DisplayServer::window_move_to_foreground).call_deferred(fd.prev_focus);
									}
								}

								DBusError err;
								dbus_error_init(&err);
								dbus_bus_remove_match(portal->monitor_connection, fd.filter.utf8().get_data(), &err);
								dbus_error_free(&err);

								portal->file_dialogs.remove_at(i);
								break;
							}
						}
					}
					{
						MutexLock lock(portal->color_picker_mutex);
						for (int i = 0; i < portal->color_pickers.size(); i++) {
							FreeDesktopPortalDesktop::ColorPickerData &cd = portal->color_pickers.write[i];
							if (cd.path == path) {
								DBusMessageIter iter;
								if (dbus_message_iter_init(msg, &iter)) {
									bool cancel = false;
									Color c;
									color_picker_parse_response(&iter, cancel, c);

									if (cd.callback.is_valid()) {
										ColorPickerCallback cb;
										cb.callback = cd.callback;
										cb.color = c;
										cb.status = !cancel;
										portal->pending_color_cbs.push_back(cb);
									}
								}

								DBusError err;
								dbus_error_init(&err);
								dbus_bus_remove_match(portal->monitor_connection, cd.filter.utf8().get_data(), &err);
								dbus_error_free(&err);

								portal->color_pickers.remove_at(i);
								break;
							}
						}
					}
				}
				dbus_message_unref(msg);
			}
			dbus_connection_read_write(portal->monitor_connection, 0);
		}

		OS::get_singleton()->delay_usec(50'000);
	}
}

void FreeDesktopPortalDesktop::_system_theme_changed_callback() {
	if (system_theme_changed.is_valid()) {
		Variant ret;
		Callable::CallError ce;
		system_theme_changed.callp(nullptr, 0, ret, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_PRINT(vformat("Failed to execute system theme changed callback: %s.", Variant::get_callable_error_text(system_theme_changed, nullptr, 0, ce)));
		}
	}
}

FreeDesktopPortalDesktop::FreeDesktopPortalDesktop() {
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	unsupported = (initialize_dbus(dylibloader_verbose) != 0);
#else
	unsupported = false;
#endif

	if (unsupported) {
		return;
	}

	bool ver_ok = false;
	int version_major = 0;
	int version_minor = 0;
	int version_rev = 0;
	dbus_get_version(&version_major, &version_minor, &version_rev);
	ver_ok = (version_major == 1 && version_minor >= 10) || (version_major > 1); // 1.10.0
	print_verbose(vformat("PortalDesktop: DBus %d.%d.%d detected.", version_major, version_minor, version_rev));
	if (!ver_ok) {
		print_verbose("PortalDesktop: Unsupported DBus library version!");
		unsupported = true;
	}

	DBusError err;
	dbus_error_init(&err);
	monitor_connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		dbus_error_free(&err);
	} else {
		theme_path = "type='signal',sender='org.freedesktop.portal.Desktop',interface='org.freedesktop.portal.Settings',member='SettingChanged'";
		dbus_bus_add_match(monitor_connection, theme_path.utf8().get_data(), &err);
		if (dbus_error_is_set(&err)) {
			dbus_error_free(&err);
			dbus_connection_unref(monitor_connection);
			monitor_connection = nullptr;
		}
		dbus_connection_read_write(monitor_connection, 0);
	}

	if (!unsupported) {
		monitor_thread_abort.clear();
		monitor_thread.start(FreeDesktopPortalDesktop::_thread_monitor, this);
	}
}

FreeDesktopPortalDesktop::~FreeDesktopPortalDesktop() {
	monitor_thread_abort.set();
	if (monitor_thread.is_started()) {
		monitor_thread.wait_to_finish();
	}

	if (monitor_connection) {
		DBusError err;
		for (FreeDesktopPortalDesktop::FileDialogData &fd : file_dialogs) {
			dbus_error_init(&err);
			dbus_bus_remove_match(monitor_connection, fd.filter.utf8().get_data(), &err);
			dbus_error_free(&err);
		}
		dbus_error_init(&err);
		dbus_bus_remove_match(monitor_connection, theme_path.utf8().get_data(), &err);
		dbus_error_free(&err);
		dbus_connection_unref(monitor_connection);
	}
}

#endif // DBUS_ENABLED

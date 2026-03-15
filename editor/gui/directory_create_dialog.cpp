/**************************************************************************/
/*  directory_create_dialog.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
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

#include "directory_create_dialog.h"

#include "core/io/dir_access.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"

PathValidationResult PathValidationResult::from_error(const String &error) {
	PathValidationResult result;
	result.error = error;
	result.is_error = true;
	return result;
}

PathValidationResult PathValidationResult::from_suceess(const String &final_dir) {
	PathValidationResult result;
	result.final_dir = final_dir;
	result.is_error = false;
	return result;
}

String DirectoryCreateDialog::_sanitize_input(const String &p_path) const {
	String path = p_path.strip_edges();
	if (mode == MODE_DIRECTORY) {
		path = path.trim_suffix("/");
	}
	return path;
}

PathValidationResult DirectoryCreateDialog::_validate_path(const String &p_path) const {
	if (p_path.is_empty()) {
		return PathValidationResult::from_error(TTR("Name cannot be empty."));
	}
	if (mode == MODE_FILE && p_path.ends_with("/")) {
		return PathValidationResult::from_error(TTR("File name can't end with /."));
	}

	String final_dir_start = "";
	PackedStringArray final_dir_array = {};

	String processed_path = p_path;

	if (base_dir.begins_with("res://")) {
		final_dir_start = "res://";
		final_dir_array = base_dir.substr(6).split("/");
	} else {
		final_dir_array = base_dir.split("/");
	}

	if (processed_path.begins_with("res://")) {
		final_dir_array.clear();
		processed_path = processed_path.substr(6);
	} else if (processed_path[0] == '/') {
		final_dir_array.clear();
		processed_path = processed_path.substr(1);
	}

	const PackedStringArray splits = processed_path.split("/");
	for (int i = 0; i < splits.size(); i++) {
		const String &part = splits[i];
		bool is_file = mode == MODE_FILE && i == splits.size() - 1;

		if (part.is_empty()) {
			if (is_file) {
				return PathValidationResult::from_error(TTR("File name cannot be empty."));
			} else {
				return PathValidationResult::from_error(TTR("Folder name cannot be empty."));
			}
		}
		if (part == ".") {
			if (is_file) {
				return PathValidationResult::from_error(TTR("File name contains invalid characters."));
			} else {
				continue;
			}
		}
		if (part == "..") {
			if (is_file) {
				return PathValidationResult::from_error(TTR("File name contains invalid characters."));
			} else if (final_dir_array.size() <= 0) {
				return PathValidationResult::from_error(TTR("Path points to a location that is invalid."));
			} else {
				final_dir_array.remove_at(final_dir_array.size() - 1);
				continue;
			}
		}
		if (part.contains_char('\\') || part.contains_char(':') || part.contains_char('*') ||
				part.contains_char('|') || part.contains_char('>') || part.ends_with(".") || part.ends_with(" ")) {
			if (is_file) {
				return PathValidationResult::from_error(TTR("File name contains invalid characters."));
			} else {
				return PathValidationResult::from_error(TTR("Folder name contains invalid characters."));
			}
		}
		if (part[0] == '.') {
			if (is_file) {
				return PathValidationResult::from_error(TTR("File name may not begin with a dot."));
			} else {
				return PathValidationResult::from_error(TTR("Folder name may not begin with a dot."));
			}
		}

		final_dir_array.append(part);
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	da->change_dir(base_dir);
	if (da->file_exists(p_path)) {
		return PathValidationResult::from_error(TTR("File with that name already exists."));
	}
	if (da->dir_exists(p_path)) {
		return PathValidationResult::from_error(TTR("Folder with that name already exists."));
	}

	return PathValidationResult::from_suceess(
			final_dir_start + String("/").join(final_dir_array));
}

void DirectoryCreateDialog::_on_dir_path_changed() {
	const String path = _sanitize_input(dir_path->get_text());
	const PathValidationResult validation_result = _validate_path(path);

	if (validation_result.is_error) {
		validation_panel->set_message(MSG_ID_VALIDATION, validation_result.error, EditorValidationPanel::MSG_ERROR);
		validation_panel->set_message(MSG_ID_SUBFOLDER, "", EditorValidationPanel::MSG_OK);
	} else {
		if (path.contains_char('/')) {
			if (mode == MODE_DIRECTORY) {
				validation_panel->set_message(MSG_ID_VALIDATION,
						vformat(TTR("Folder will be created at: %s"), validation_result.final_dir),
						EditorValidationPanel::MSG_OK);
			} else {
				validation_panel->set_message(MSG_ID_VALIDATION,
						vformat(TTR("File will be created at: %s"), validation_result.final_dir),
						EditorValidationPanel::MSG_OK);
			}

			const bool parent_dir_exists = DirAccess::dir_exists_absolute(validation_result.final_dir.path_join(".."));
			if (parent_dir_exists) {
				validation_panel->set_message(MSG_ID_SUBFOLDER, "", EditorValidationPanel::MSG_OK);
			} else {
				validation_panel->set_message(MSG_ID_SUBFOLDER,
						TTR("One or more subfolders along the destination path do not exist yet and will be created."),
						EditorValidationPanel::MSG_WARNING);
			}

		} else if (mode == MODE_FILE) {
			validation_panel->set_message(MSG_ID_VALIDATION, TTRC("File name is valid."), EditorValidationPanel::MSG_OK);
			validation_panel->set_message(MSG_ID_SUBFOLDER, "", EditorValidationPanel::MSG_OK);
		}
	}
}

void DirectoryCreateDialog::ok_pressed() {
	const String path = _sanitize_input(dir_path->get_text());

	// The OK button should be disabled if the path is invalid, but just in case.
	const PathValidationResult validation_result = _validate_path(path);
	ERR_FAIL_COND_MSG(validation_result.is_error, validation_result.error);

	accept_callback.call(validation_result.final_dir);
	hide();
}

void DirectoryCreateDialog::_post_popup() {
	ConfirmationDialog::_post_popup();
	dir_path->grab_focus();
}

void DirectoryCreateDialog::config(const String &p_base_dir, const Callable &p_accept_callback, int p_mode, const String &p_title, const String &p_default_name) {
	set_title(p_title);
	base_dir = p_base_dir;
	base_path_label->set_text(vformat(TTR("Base path: %s"), base_dir));
	accept_callback = p_accept_callback;
	mode = p_mode;

	dir_path->set_text(p_default_name);
	validation_panel->update();

	if (p_mode == MODE_FILE) {
		int extension_pos = p_default_name.rfind_char('.');
		if (extension_pos > -1) {
			dir_path->select(0, extension_pos);
			return;
		}
	}
	dir_path->select_all();
}

DirectoryCreateDialog::DirectoryCreateDialog() {
	set_min_size(Size2i(480, 0) * EDSCALE);

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	base_path_label = memnew(Label);
	base_path_label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	base_path_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_WORD_ELLIPSIS);
	vb->add_child(base_path_label);

	Label *name_label = memnew(Label);
	name_label->set_text(TTR("Name:"));
	name_label->set_theme_type_variation("HeaderSmall");
	vb->add_child(name_label);

	dir_path = memnew(LineEdit);
	dir_path->set_accessibility_name(TTRC("Name:"));
	vb->add_child(dir_path);
	register_text_enter(dir_path);

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));
	vb->add_child(spacing);

	validation_panel = memnew(EditorValidationPanel);
	vb->add_child(validation_panel);
	validation_panel->add_line(MSG_ID_VALIDATION, TTRC("Folder name is valid."));
	validation_panel->add_line(MSG_ID_SUBFOLDER);
	validation_panel->set_update_callback(callable_mp(this, &DirectoryCreateDialog::_on_dir_path_changed));
	validation_panel->set_accept_button(get_ok_button());

	dir_path->connect(SceneStringName(text_changed), callable_mp(validation_panel, &EditorValidationPanel::update).unbind(1));
}

/**************************************************************************/
/*  resource_importer_image_frames.cpp                                    */
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

#include "resource_importer_image_frames.h"

#include "core/io/file_access.h"
#include "core/io/image_frames_loader.h"

String ResourceImporterImageFrames::get_importer_name() const {
	return "image_frames";
}

String ResourceImporterImageFrames::get_visible_name() const {
	return "ImageFrames";
}

void ResourceImporterImageFrames::get_recognized_extensions(List<String> *p_extensions) const {
	ImageFramesLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterImageFrames::get_save_extension() const {
	return "image_frames";
}

String ResourceImporterImageFrames::get_resource_type() const {
	return "ImageFrames";
}

bool ResourceImporterImageFrames::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterImageFrames::get_preset_count() const {
	return 0;
}

String ResourceImporterImageFrames::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterImageFrames::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

Error ResourceImporterImageFrames::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Ref<FileAccess> f = FileAccess::open(p_source_file, FileAccess::READ);

	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Cannot open file from path '" + p_source_file + "'.");
	uint64_t len = f->get_length();

	Vector<uint8_t> data;
	data.resize(len);

	f->get_buffer(data.ptrw(), len);

	f = FileAccess::open(p_save_path + ".image_frames", FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, "Cannot create file in path '" + p_save_path + ".image_frames'.");

	//save the header RDIM
	const uint8_t header[5] = { 'R', 'D', 'I', 'M', 'F' };
	f->store_buffer(header, 5);
	//SAVE the extension (so it can be recognized by the loader later
	f->store_pascal_string(p_source_file.get_extension().to_lower());
	//SAVE the actual image
	f->store_buffer(data.ptr(), len);

	return OK;
}

ResourceImporterImageFrames::ResourceImporterImageFrames() {
}

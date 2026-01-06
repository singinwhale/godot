#include "ruby_file_loader.h"

#include "ruby_file.h"
#include "scene/resources/text_file.h"

Ref<Resource> ResourceFormatLoaderRuby::load(const String& p_path, const String& p_original_path, Error* r_error, bool p_use_sub_threads, float* r_progress, CacheMode p_cache_mode) {
	Ref<RubyFile> res;
	res.instantiate();

	Error local_error;
	if (!r_error) {
		r_error = &local_error;
	}

	*r_error = res->load_text(p_path);
	if (*r_error == OK) {
		return res;
	}
	return {};
}

void ResourceFormatLoaderRuby::get_recognized_extensions(List<String> *r_extensions) const {
	if (!r_extensions->find("rb")) {
		r_extensions->push_back("rb");
	}
}

bool ResourceFormatLoaderRuby::handles_type(const String &p_type) const {
	return p_type == "RubyFile";
}

String ResourceFormatLoaderRuby::get_resource_type(const String &p_path) const {
	if (p_path.has_extension("rb")) {
		return "RubyFile";
	}
	return "";
}


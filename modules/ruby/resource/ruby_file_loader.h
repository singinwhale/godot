#pragma once
#include "core/io/resource_loader.h"

class ResourceFormatLoaderRuby : public ResourceFormatLoader {
	GDSOFTCLASS(ResourceFormatLoaderRuby, ResourceFormatLoader);

public:
	Ref<Resource> load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) override;
	virtual void get_recognized_extensions(List<String> *r_extensions) const override;
	bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

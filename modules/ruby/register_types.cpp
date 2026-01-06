/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "context/ruby_eval_context.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "resource/ruby_file_loader.h"
#include "resource/ruby_file.h"

#include <mruby.h>

static Ref<ResourceFormatLoaderRuby> ruby_loader;

// Forward declaration - defined in mruby_print.cpp
void mrb_init_godot_print(mrb_state *mrb);

void initialize_ruby_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		register_ruby_types();
		// Initialize mruby runtime
		mrb_state *mrb = mrb_open();
		if (mrb) {
			// Register custom print functions that redirect to Godot's printing system
			// This hooks puts, print, p, and all other output
			mrb_init_godot_print(mrb);
			mrb_show_version(mrb);

			mrb_close(mrb);
		}
	}
}

void uninitialize_ruby_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		register_ruby_types();
	}
}

void register_ruby_types() {
	GDREGISTER_CLASS(RubyEvalContext);
	GDREGISTER_CLASS(ResourceFormatLoaderRuby);
	GDREGISTER_CLASS(RubyFile);

	// Story system classes
	GDREGISTER_CLASS(StoryCharacter);
	GDREGISTER_CLASS(StoryVoiceLine);
	GDREGISTER_CLASS(StoryOption);
	GDREGISTER_CLASS(StoryChoice);

	ruby_loader.instantiate();
	ResourceLoader::add_resource_format_loader(ruby_loader);

}

void unregister_ruby_types() {

	ResourceLoader::remove_resource_format_loader(ruby_loader);
	ruby_loader.unref();
}

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

#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <mruby.h>
#include <mruby/compile.h>

// Forward declaration - defined in mruby_print.cpp
void mrb_init_godot_print(mrb_state *mrb);
extern "C" void mrb_show_version(mrb_state *mrb);

static mrb_state *mrb = nullptr;

void initialize_ruby_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Initialize mruby runtime
	mrb = mrb_open();
	if (mrb) {
		// Register custom print functions that redirect to Godot's printing system
		// This hooks puts, print, p, and all other output
		mrb_init_godot_print(mrb);
		mrb_show_version(mrb);

		// Test the custom puts function
		const int arena_index = mrb_gc_arena_save(mrb);
		mrb_load_string(mrb, "puts 'Hello from mruby!'\np 'Hello 2'");
		mrb_gc_arena_restore(mrb, arena_index);
	}
}

void uninitialize_ruby_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Clean up mruby runtime
	if (mrb) {
		mrb_close(mrb);
		mrb = nullptr;
	}
}

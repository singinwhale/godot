#include "ruby_eval_context.h"

#include "mruby.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "modules/ruby/mruby_print.h"
#include "modules/ruby/resource/ruby_file.h"
#include "mruby/compile.h"
#include "mruby/error.h"
#include "mruby/string.h"
#include "thirdparty/mruby/build/host/include/mruby/proc.h"
#include "thirdparty/mruby/build/minimal/include/mruby/debug.h"

String ruby_to_string(mrb_state *p_mrb, mrb_value p_str) {
	mrb_value str = mrb_obj_as_string(p_mrb, p_str);
	const char *cstr = RSTRING_PTR(str);
	mrb_int len = RSTRING_LEN(str);
	Span s = Span(cstr, (uint64_t)len);
	String godot_str;
	godot_str.append_ascii(s);
	return godot_str;
}

const char *mrb_get_calling_file(mrb_state *p_mrb) {
	const char *filename = NULL;
	mrb_callinfo *ci;
	const mrb_irep *irep = NULL;
	const mrb_code *pc = NULL;

	// Start from the caller's frame
	ptrdiff_t ciidx = (p_mrb->c->ci - p_mrb->c->cibase) - 1;

	// Search backwards through the call stack for a Ruby frame (not C function)
	for (ptrdiff_t i = ciidx; i >= 0; i--) {
		ci = &p_mrb->c->cibase[i];

		if (!ci->proc)
			continue;
		if (MRB_PROC_CFUNC_P(ci->proc))
			continue; // Skip C functions

		irep = ci->proc->body.irep;
		if (!irep)
			continue;
		if (!irep->debug_info)
			continue; // Need debug info for filename
		if (!ci->pc)
			continue;

		pc = ci->pc - 1;
		break; // Found a valid Ruby frame
	}

	if (irep && pc) {
		uint32_t idx = (uint32_t)(pc - irep->iseq);
		filename = mrb_debug_get_filename(p_mrb, irep, idx);
	}
	return filename;
}

namespace {
mrb_value godot_require(mrb_state *p_mrb, mrb_value p_self) {
	mrb_value p_path = mrb_get_arg1(p_mrb);
	String godot_str = ruby_to_string(p_mrb, p_path);

	print_line("require: " + godot_str);
	RubyEvalContext::compile_from_disk(p_mrb, godot_str, nullptr);
	return mrb_nil_value();
}

mrb_value godot_require_relative(mrb_state *p_mrb, mrb_value p_self) {
	mrb_value p_path = mrb_get_arg1(p_mrb);

	String gd_path = ruby_to_string(p_mrb, p_path);

	String self_string = ruby_to_string(p_mrb, p_self);
	print_line(vformat("require_relative: '%s' from '%s'", gd_path, self_string));

	const char *filename = mrb_get_calling_file(p_mrb);
	if (!filename) {
		print_error(vformat("%s: require_relative called from unknown location trying to require %s", ruby_to_string(p_mrb, p_self), gd_path));
	}

	String calling_file_path = String(filename);
	String load_path = calling_file_path.get_base_dir().path_join(gd_path);

	RubyEvalContext::compile_from_disk(p_mrb, load_path, nullptr);

	return mrb_nil_value();
}
}

void RubyEvalContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("eval_file", "path"), &RubyEvalContext::eval_file);
}

extern "C" {
void mrb_show_version(mrb_state *mrb);
}

void RubyEvalContext::eval_file(const String &p_path) {
	// Initialize mruby runtime
	mrb = mrb_open();
	if (mrb) {
		// Register custom print functions that redirect to Godot's printing system
		// This hooks puts, print, p, and all other output
		mrb_init_godot_print(mrb);
		mrb_show_version(mrb);

		compile_from_disk("res://mruby/mrblib/00class.rb");
		compile_from_disk("res://mruby/mrblib/00kernel.rb");
		compile_from_disk("res://mruby/mrblib/10error.rb");
		compile_from_disk("res://mruby/mrblib/array.rb");
		compile_from_disk("res://mruby/mrblib/compar.rb");
		compile_from_disk("res://mruby/mrblib/enum.rb");
		compile_from_disk("res://mruby/mrblib/hash.rb");
		compile_from_disk("res://mruby/mrblib/kernel.rb");
		compile_from_disk("res://mruby/mrblib/numeric.rb");
		compile_from_disk("res://mruby/mrblib/range.rb");
		compile_from_disk("res://mruby/mrblib/string.rb");
		compile_from_disk("res://mruby/mrblib/symbol.rb");

		mrb_define_method(mrb, mrb->kernel_module, "require", &godot_require, MRB_ARGS_ARG(1, 0));
		mrb_define_method(mrb, mrb->kernel_module, "require_relative", &godot_require_relative, MRB_ARGS_ARG(1, 0));

		compile_from_disk(p_path);
	} else {
		print_error("Failed to initialize mruby");
	}

	// Clean up mruby runtime
	if (mrb) {
		mrb_close(mrb);
		mrb = nullptr;
	}
}

void RubyEvalContext::compile_from_disk(const String &p_path, mrb_ccontext *p_compile_context) {
	compile_from_disk(mrb, p_path, p_compile_context);
}

void RubyEvalContext::compile_from_disk(mrb_state *mrb, const String &p_path, mrb_ccontext *p_compile_context) {
	String path = p_path;
	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	if (!path.has_extension("rb")) {
		path = path + ".rb";
	}

	Error err = OK;
	String lib_file = FileAccess::get_file_as_string(path, &err);
	Ref<RubyFile> file = ResourceLoader::load(path, "RubyFile", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	if (err != OK) {
		print_error(vformat("Failed to load ruby file '%s': %s", path, err ? error_names[err] : "unknown error - check log."));
		return;
	}

	print_verbose(vformat("Loading ruby file %s"), path);
	const int arena_index = mrb_gc_arena_save(mrb);
	if (!p_compile_context) {
		mrb_ccontext *compile_context = mrb_ccontext_new(mrb);
		CharString filename = path.ascii(true);
		mrb_ccontext_filename(mrb, compile_context, filename.ptr());
		compile_context->capture_errors = true;
		mrb_load_string_cxt(mrb, file->get_text().ascii(true).get_data(), compile_context);
		mrb_print_error(mrb);
		mrb_ccontext_free(mrb, compile_context);
	} else {
		mrb_load_string_cxt(mrb, file->get_text().ascii(true).get_data(), p_compile_context);
	}
	mrb_gc_arena_restore(mrb, arena_index);
}

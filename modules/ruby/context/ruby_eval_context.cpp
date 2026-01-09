#include "ruby_eval_context.h"

#include "mruby.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "modules/ruby/mruby_print.h"
#include "modules/ruby/resource/ruby_file.h"
#include "mruby/array.h"
#include "mruby/compile.h"
#include "mruby/debug.h"
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

String ruby_to_string(mrb_state *p_mrb, mrb_value p_str) {
	mrb_value str = mrb_obj_as_string(p_mrb, p_str);
	const char *cstr = RSTRING_PTR(str);
	mrb_int len = RSTRING_LEN(str);
	Span s = Span(cstr, (uint64_t)len);
	String godot_str;
	godot_str.append_ascii(s);
	return godot_str;
}

void ensure_absolute_ruby_file(String &path) {
	if (path.begins_with("uid://"))
		return;

	if (!path.begins_with("res://")) {
		path = "res://" + path;
	}
	if (!path.has_extension("rb")) {
		path = path + ".rb";
	}
}

CharString string_to_c(const String &p_str) {
	return p_str.ascii(true);
}

mrb_value string_to_ruby(mrb_state *mrb, const String &p_str) {
	CharString str_c = string_to_c(p_str);
	return mrb_str_new(mrb, str_c.ptr(), str_c.length());
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
	return RubyEvalContext::compile_from_disk(p_mrb, godot_str, nullptr);
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

	return RubyEvalContext::compile_from_disk(p_mrb, load_path, nullptr);
}


mrb_value godot_run(mrb_state *p_mrb, mrb_value p_self) {
	mrb_value file = mrb_proc_cfunc_env_get(p_mrb, 0);
	String godot_str = ruby_to_string(p_mrb, file);

	return RubyEvalContext::compile_from_disk(p_mrb, godot_str, nullptr);
}

mrb_value godot_say(mrb_state *p_mrb, mrb_value p_self) {
	return mrb_nil_value();
}

mrb_value godot_choose(mrb_state *p_mrb, mrb_value p_self) {
	return mrb_fiber_yield(p_mrb, 0, nullptr);
}
}

StringName StoryCharacter::get_emotion() const {
	return emotion;
}

void StoryCharacter::set_emotion(const StringName &p_emotion) {
	this->emotion = p_emotion;
}

StringName StoryCharacter::get_id() const {
	return id;
}

void StoryCharacter::set_id(const StringName &p_id) {
	this->id = p_id;
}

void StoryCharacter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_emotion"), &StoryCharacter::get_emotion);
	ClassDB::bind_method(D_METHOD("set_emotion", "emotion"), &StoryCharacter::set_emotion);
	ClassDB::bind_method(D_METHOD("get_id"), &StoryCharacter::get_id);
	ClassDB::bind_method(D_METHOD("set_id", "name"), &StoryCharacter::set_id);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "emotion"), "set_emotion", "get_emotion");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "id"), "set_id", "get_id");
}

void StoryCharacter::init_from_mrb(mrb_state *mrb, mrb_value self) {
	// Get @id instance variable
	mrb_value id_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@id"));
	this->id = ruby_to_string(mrb, id_val);

	// Get @emotion instance variable (it's a symbol)
	mrb_value emotion_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@emotion"));
	if (mrb_symbol_p(emotion_val)) {
		mrb_sym emotion_sym = mrb_symbol(emotion_val);
		mrb_value emotion_str = mrb_sym_str(mrb, emotion_sym);
		this->emotion = ruby_to_string(mrb, emotion_str);
	}
}

void StoryVoiceLine::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_character"), &StoryVoiceLine::get_character);
	ClassDB::bind_method(D_METHOD("set_character", "character"), &StoryVoiceLine::set_character);
	ClassDB::bind_method(D_METHOD("get_text"), &StoryVoiceLine::get_text);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &StoryVoiceLine::set_text);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "character", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "StoryCharacter"), "set_character", "get_character");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
}

void StoryVoiceLine::init_from_mrb(mrb_state *mrb, mrb_value self) {
	// Get @character instance variable
	mrb_value char_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@character"));
	if (!mrb_nil_p(char_val)) {
		Ref<StoryCharacter> char_ref;
		char_ref.instantiate();
		char_ref->init_from_mrb(mrb, char_val);
		this->character = char_ref;
	}

	// Get @text instance variable
	mrb_value text_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@text"));
	this->text = ruby_to_string(mrb, text_val);
}

Ref<StoryCharacter> StoryVoiceLine::get_character() const {
	return character;
}

void StoryVoiceLine::set_character(Ref<StoryCharacter> p_character) {
	character = p_character;
}

String StoryVoiceLine::get_text() const {
	return text;
}

void StoryVoiceLine::set_text(const String &p_text) {
	text = p_text;
}

void StoryOption::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &StoryOption::get_id);
	ClassDB::bind_method(D_METHOD("get_text"), &StoryOption::get_text);
	ClassDB::bind_method(D_METHOD("is_available"), &StoryOption::is_available);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "id"), "", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "available"), "", "is_available");
}

void StoryOption::init_from_mrb(mrb_state *mrb, mrb_value self) {
	// Get @id instance variable (could be symbol or string)
	mrb_value id_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@id"));
	if (mrb_symbol_p(id_val)) {
		mrb_sym id_sym = mrb_symbol(id_val);
		mrb_value id_str = mrb_sym_str(mrb, id_sym);
		this->id = ruby_to_string(mrb, id_str);
	} else {
		this->id = ruby_to_string(mrb, id_val);
	}

	// Get @text instance variable
	mrb_value text_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@text"));
	this->text = ruby_to_string(mrb, text_val);

	// Get @condition instance variable (might be nil)
	mrb_value cond_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@condition"));
	// For now, we just check if it exists - available if no condition or condition is truthy
	this->available = mrb_nil_p(cond_val) || mrb_test(cond_val);
}

StringName StoryOption::get_id() const {
	return id;
}

String StoryOption::get_text() const {
	return text;
}

bool StoryOption::is_available() const {
	return available;
}

StringName StoryChoice::get_id() const {
	return id;
}

TypedArray<Ref<StoryOption>> StoryChoice::get_options() const {
	return options;
}

void StoryChoice::init_from_mrb(mrb_state *mrb, mrb_value self) {
	// Helper struct for passing data to hash foreach callback
	struct OptionsCollector {
		mrb_state *mrb;
		TypedArray<Ref<StoryOption>> *options;

		// Static callback for iterating over the @options hash
		static int collect_option(mrb_state *mrb, mrb_value key, mrb_value val, void *data) {
			OptionsCollector *collector = static_cast<OptionsCollector *>(data);

			// Create a StoryOption from the Option ruby object
			Ref<StoryOption> option;
			option.instantiate();
			option->init_from_mrb(mrb, val);

			// Add to the options array
			collector->options->append(option);

			return 0; // Continue iteration
		}
	};

	// Get @id instance variable (could be symbol or string)
	mrb_value id_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@id"));
	if (mrb_symbol_p(id_val)) {
		mrb_sym id_sym = mrb_symbol(id_val);
		mrb_value id_str = mrb_sym_str(mrb, id_sym);
		this->id = ruby_to_string(mrb, id_str);
	} else {
		this->id = ruby_to_string(mrb, id_val);
	}

	// Get @question instance variable
	mrb_value question_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@question"));
	this->question = ruby_to_string(mrb, question_val);

	// Get @options instance variable (it's a hash)
	mrb_value options_val = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@options"));
	if (mrb_hash_p(options_val)) {
		this->options.clear();

		// Set up collector for the foreach callback
		OptionsCollector collector;
		collector.mrb = mrb;
		collector.options = &this->options;

		// Iterate over the hash
		struct RHash *hash = mrb_hash_ptr(options_val);
		mrb_hash_foreach(mrb, hash, OptionsCollector::collect_option, &collector);
	}
}

String StoryChoice::get_question() const {
	return question;
}

void StoryChoice::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &StoryChoice::get_id);
	ClassDB::bind_method(D_METHOD("get_options"), &StoryChoice::get_options);
	ClassDB::bind_method(D_METHOD("get_question"), &StoryChoice::get_question);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "id"), "", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "options", PROPERTY_HINT_ARRAY_TYPE, "StoryOption"), "", "get_options");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "question"), "", "get_question");
}

void RubyEvalContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("eval_file", "path"), &RubyEvalContext::eval_file);
	ClassDB::bind_method(D_METHOD("resume"), &RubyEvalContext::resume);
	ClassDB::bind_method(D_METHOD("choose", "option_id"), &RubyEvalContext::choose);
	ADD_SIGNAL(MethodInfo("on_say", PropertyInfo(Variant::OBJECT, "character", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "StoryVoiceline")));
	ADD_SIGNAL(MethodInfo("on_choice", PropertyInfo(Variant::OBJECT, "choice", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "StoryChoice")));
	ADD_SIGNAL(MethodInfo("on_show_character", PropertyInfo(Variant::OBJECT, "character", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "StoryCharacter")));
	ADD_SIGNAL(MethodInfo("on_show_scene", PropertyInfo(Variant::STRING_NAME, "scene")));
}

extern "C" {
void
mrb_show_version(mrb_state *mrb);

void
mrb_mruby_fiber_gem_init(mrb_state *mrb);

void
mrb_mruby_catch_gem_init(mrb_state *mrb);

RProc *
mrb_create_proc_from_string(mrb_state *mrb, const char *s, mrb_int len, mrb_value binding, const char *file, mrb_int line);
}

void RubyEvalContext::eval_file(const String &p_path) {
	// Initialize mruby runtime
	mruby_state = mrb_open();
	if (mruby_state) {
		// Add this as user data so bound methods can get back at the game state.
		mruby_state->ud = this;

		// Register custom print functions that redirect to Godot's printing system
		// This hooks puts, print, p, and all other output
		mrb_init_godot_print(mruby_state);
		mrb_mruby_fiber_gem_init(mruby_state);
		mrb_mruby_catch_gem_init(mruby_state);
		mrb_show_version(mruby_state);

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
		compile_from_disk("res://mruby/mrblib/catch.rb");

		mrb_define_method(mruby_state, mruby_state->kernel_module, "require", &godot_require, MRB_ARGS_ARG(1, 0));
		mrb_define_method(mruby_state, mruby_state->kernel_module, "require_relative", &godot_require_relative, MRB_ARGS_ARG(1, 0));

		compile_from_disk("res://mruby/runtime.rb");

		mrb_value fiber = compile_to_fibre(mruby_state, p_path);
		if (mrb_nil_p(fiber)) {
			return;
		}

		current_fiber = fiber;
		mrb_value result = mrb_fiber_resume(mruby_state, fiber, 0, nullptr);
		handle_fiber_result(result, fiber);
	} else {
		print_error("Failed to initialize mruby");
	}
}

void RubyEvalContext::terminate() {
	// Clean up mruby runtime
	if (mruby_state) {
		current_fiber = mrb_nil_value();
		mrb_close(mruby_state);
		mruby_state = nullptr;
	}
}

void RubyEvalContext::resume() {
	mrb_value result = mrb_fiber_resume(mruby_state, current_fiber, 0, nullptr);
	handle_fiber_result(result, current_fiber);
}

void RubyEvalContext::choose(StringName option_id) {
	mrb_value argv = string_to_ruby(mruby_state, option_id);
	mrb_value result = mrb_fiber_resume(mruby_state, current_fiber, 1, &argv);
	handle_fiber_result(result, current_fiber);
}

mrb_value RubyEvalContext::compile_from_disk(const String &p_path, mrb_ccontext *p_compile_context) {
	return compile_from_disk(mruby_state, p_path, p_compile_context);
}

void RubyEvalContext::handle_fiber_result(mrb_value result, mrb_value fiber) {
	do {
		mrb_print_error(mruby_state);
		if (mrb_nil_p(result)) {
			result = mrb_fiber_resume(mruby_state, fiber, 0, nullptr);
		} else if (mrb_array_p(result)) {
			mrb_value *results = RARRAY_PTR(result);
			int results_len = RARRAY_LEN(result);
			for (int i = 0; i < results_len; i++) {
			}
			break;
		} else if (mrb_object_p(result)) {
			CharString classname = mrb_obj_classname(mruby_state, result);
			if (classname == "Choice") {
				Ref<StoryChoice> choice;
				choice.instantiate();
				choice->init_from_mrb(mruby_state, result);
				_on_choice(choice);
				break;
			} else if (classname == "VoiceLine") {
				Ref<StoryVoiceLine> voice_line;
				voice_line.instantiate();
				voice_line->init_from_mrb(mruby_state, result);
				_on_say(voice_line);
				break;
			} else if (classname == "Scene") {
				mrb_value id_val = mrb_iv_get(mruby_state, result, mrb_intern_lit(mruby_state, "@name"));
				StringName name = ruby_to_string(mruby_state, id_val);
				_on_show_scene(name);
				break;
			} else if (classname == "Character") {
				Ref<StoryCharacter> character;
				character.instantiate();
				character->init_from_mrb(mruby_state, result);
				_on_show_character(character);
				break;
			} else {
				print_error(vformat("unrecognized classname from ruby: %s", classname.ptr()));
				result = mrb_fiber_resume(mruby_state, fiber, 0, nullptr);
			}
		} else {
			print_error(vformat("unrecognized result from ruby: %s", ruby_to_string(mruby_state, result)));
			result = mrb_fiber_resume(mruby_state, fiber, 0, nullptr);
		}
	} while (mrb_test(mrb_fiber_alive_p(mruby_state, fiber)));
}

Ref<RubyFile> RubyEvalContext::try_load_ruby_file(const String &p_path) {
	Error err = OK;
	String lib_file = FileAccess::get_file_as_string(p_path, &err);
	Ref<RubyFile> file = ResourceLoader::load(p_path, "RubyFile", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	if (err != OK) {
		print_error(vformat("Failed to load ruby file '%s': %s", p_path, err ? error_names[err] : "unknown error - check log."));
		return {};
	}
	return file;
}

mrb_value RubyEvalContext::compile_from_disk(mrb_state *mrb, const String &p_path, mrb_ccontext *p_compile_context) {
	mrb_value result = mrb_nil_value();
	String path = p_path;
	ensure_absolute_ruby_file(path);
	Ref<RubyFile> file = try_load_ruby_file(path);
	if (!file.is_valid()) {
		return result;
	}

	print_verbose(vformat("Loading ruby file %s", path));
	HashSet<StringName> &already_required_files_ref = get_owning_eval_context(mrb)->already_required_files;
	if (already_required_files_ref.has(path)) {
		return result;
	}

	already_required_files_ref.insert(path);
	const int arena_index = mrb_gc_arena_save(mrb);
	if (!p_compile_context) {
		mrb_ccontext *compile_context = mrb_ccontext_new(mrb);
		CharString filename = path.ascii(true);
		mrb_ccontext_filename(mrb, compile_context, filename.ptr());
		compile_context->capture_errors = true;
		result = mrb_load_string_cxt(mrb, file->get_text().ascii(true).get_data(), compile_context);
		mrb_print_error(mrb);
		mrb_ccontext_free(mrb, compile_context);
	} else {
		result = mrb_load_string_cxt(mrb, file->get_text().ascii(true).get_data(), p_compile_context);
	}
	mrb_gc_arena_restore(mrb, arena_index);
	return result;
}

mrb_value RubyEvalContext::compile_to_fibre(mrb_state *mrb, const String &p_path) {
	mrb_value result = mrb_nil_value();
	String path = p_path;
	ensure_absolute_ruby_file(path);
	Ref<RubyFile> file = try_load_ruby_file(path);
	if (!file.is_valid()) {
		return result;
	}

	CharString file_content = string_to_c(file->get_text());
	CharString filename = string_to_c(path);
	RProc *proc = mrb_create_proc_from_string(mrb, file_content.ptr(), file_content.length(), mrb_nil_value(), filename.ptr(), 1);

	result = mrb_fiber_new(mrb, proc);
	mrb_print_error(mrb);
	return result;
}

RubyEvalContext *RubyEvalContext::get_owning_eval_context(mrb_state *mrb) {
	RubyEvalContext *ruby_eval_context = static_cast<RubyEvalContext *>(mrb->ud);
	if (ruby_eval_context->mruby_state == mrb) {
		return ruby_eval_context;
	}
	return nullptr;
}

void RubyEvalContext::_on_say(Ref<StoryVoiceLine> p_voiceline) {
	emit_signal("on_say", p_voiceline);
}

void RubyEvalContext::_on_choice(Ref<StoryChoice> p_choice) {
	emit_signal("on_choice", p_choice);
}


void RubyEvalContext::_on_show_character(Ref<StoryCharacter> p_character) {
	emit_signal("on_show_character", p_character);
}


void RubyEvalContext::_on_show_scene(StringName p_scene_name) {
	emit_signal("on_show_scene", p_scene_name);
}

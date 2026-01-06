#pragma once
#include "mruby.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "modules/ruby/resource/ruby_file.h"

struct mrb_value;
struct mrb_state;
struct mrb_ccontext;
struct RFiber;
struct RProc;

class StoryCharacter : public RefCounted {
	GDCLASS(StoryCharacter, RefCounted)
public:
	static void _bind_methods();
	void init_from_mrb(mrb_state *mrb, mrb_value self);

	StringName get_emotion() const;
	void set_emotion(const StringName &p_emotion);
	StringName get_name() const;
	void set_name(const StringName &p_name);

private:
	StringName emotion;
	StringName name;
};

class StoryVoiceLine : public RefCounted {
	GDCLASS(StoryVoiceLine, RefCounted)
public:
	static void _bind_methods();
	void init_from_mrb(mrb_state *mrb, mrb_value self);

	Ref<StoryCharacter> get_character() const;
	void set_character(Ref<StoryCharacter> p_character);
	String get_text() const;
	void set_text(const String &p_text);

private:
	Ref<StoryCharacter> character;
	String text;
};

class StoryOption : public RefCounted {
	GDCLASS(StoryOption, RefCounted)
public:
	static void _bind_methods();
	void init_from_mrb(mrb_state *mrb, mrb_value self);

	StringName get_id() const;
	String get_text() const;
	bool is_available() const;

private:
	bool available = true;
	StringName id;
	String text;
};

class StoryChoice : public RefCounted {
	GDCLASS(StoryChoice, RefCounted)
public:
	static void _bind_methods();
	void init_from_mrb(mrb_state *mrb, mrb_value self);

	StringName get_id() const;
	TypedArray<Ref<StoryOption>> get_options() const;
	String get_question() const;

private:
	StringName id;
	TypedArray<Ref<StoryOption>> options;
	String question;
};


class RubyEvalContext : public Object {
	GDCLASS(RubyEvalContext, Object)
public:
	static void _bind_methods();

	void handle_fiber_result(mrb_value result, mrb_value fiber);
	void eval_file(const String &p_path);
	void terminate();

	void resume();
	void choose(StringName option_id);

	mrb_value compile_from_disk(const String &p_path, mrb_ccontext *p_compile_context = nullptr);

	static Ref<RubyFile> try_load_ruby_file(const String &p_path);
	static mrb_value compile_from_disk(mrb_state *mrb, const String &p_path, mrb_ccontext *p_compile_context = nullptr);
	static mrb_value compile_to_fibre(mrb_state *mrb, const String &p_path);

	static RubyEvalContext *get_owning_eval_context(mrb_state *mrb);

private:
	void _on_say(Ref<StoryVoiceLine> p_voiceline);
	void _on_choice(Ref<StoryChoice> p_choice);

	mrb_state *mruby_state;
	mrb_value current_fiber;

	HashSet<StringName> already_required_files;
};

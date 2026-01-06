#pragma once
#include "core/object/object.h"

class RubyEvalContext : public Object{
	GDCLASS(RubyEvalContext, Object)
public:
	static void _bind_methods();

	void eval_file(const String& p_path);

	void compile_from_disk(const String& p_path, struct mrb_ccontext* p_compile_context = nullptr);
	static void compile_from_disk(struct mrb_state* mrb, const String& p_path, struct mrb_ccontext* p_compile_context = nullptr);

private:


	struct mrb_state* mrb;
};

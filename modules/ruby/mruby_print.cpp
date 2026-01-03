/* Custom print implementations for MRB_NO_STDIO
 * Redirects all mruby output to Godot's printing system
 */

#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

namespace {
	/* Custom puts implementation - prints each argument on a new line */
	mrb_value godot_puts(mrb_state *mrb, mrb_value self) {
		mrb_int argc;
		mrb_value *argv;

		mrb_get_args(mrb, "*", &argv, &argc);

		if (argc == 0) {
			print_line("");
			return mrb_nil_value();
		}

		for (mrb_int i = 0; i < argc; i++) {
			mrb_value str = mrb_obj_as_string(mrb, argv[i]);
			const char *cstr = RSTRING_PTR(str);
			mrb_int len = RSTRING_LEN(str);

			char *buffer = (char *)memalloc(len + 1);
			memcpy(buffer, cstr, len);
			buffer[len] = '\0';
			print_line(buffer);
			memfree(buffer);
		}

		return mrb_nil_value();
	}

	void print(const char *str, size_t len) {
		Span s = Span<char>(str, (uint64_t)len);
		String godot_str;
		godot_str.append_ascii(s);
		__print_line_rich(godot_str);
	}
}

extern "C" void (*print_str_hook)(const char *str, size_t len);

/* Register custom puts method with mruby */
void mrb_init_godot_print(mrb_state *mrb) {
	struct RClass *kernel = mrb->kernel_module;
	mrb_define_method(mrb, kernel, "puts", godot_puts, MRB_ARGS_ANY());
	print_str_hook = &print;
}

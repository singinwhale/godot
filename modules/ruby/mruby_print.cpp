/* Custom print implementations for MRB_NO_STDIO
 * Redirects all mruby output to Godot's printing system
 */

#include "mruby_print.h"
#include "core/string/print_string.h"
#include "core/string/string_builder.h"
#include "core/variant/variant.h"

#include <mruby.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/internal.h>
#include "mruby/array.h"
#include "mruby/error.h"
#include "mruby/throw.h"

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

		Span s = Span(cstr, (uint64_t)len);
		String godot_str;
		godot_str.append_ascii(s);
		print_line(godot_str);
	}

	return mrb_nil_value();
}

void print(const char *str, size_t len, int stream) {
	Span s = Span(str, (uint64_t)len);
	String godot_str;
	godot_str.append_ascii(s);
	if (stream == 0) {
		__print_line_rich(godot_str);
	}
	else {
		print_error(godot_str);
	}
}
}


/*
#define UNKNOWN_LOCATION "(unknown):0"

static void
print_backtrace(mrb_state *mrb, struct RObject *exc, struct RBasic *ptr) {
	struct RArray *ary = NULL;
	struct RBacktrace *bt = NULL;
	mrb_int n = 0;

	if (ptr) {
		if (ptr->tt == MRB_TT_ARRAY) {
			ary = (struct RArray *)ptr;
			n = ARY_LEN(ary);
		} else {
			bt = (struct RBacktrace *)ptr;
			n = (mrb_int)bt->len;
		}
	}

	String builder;

	if (n != 0) {
		mrb_value btline;

		builder.append_ascii("trace (most recent call last):\n");
		for (mrb_int i = n - 1; i > 0; i--) {
			if (ary)
				btline = ARY_PTR(ary)[i];
			else
				btline = decode_location(mrb, &bt->locations[i]);
			if (mrb_string_p(btline)) {
				builder += vformat("\t[%d] ", (int)i);

				builder.append_ascii(Span(RSTRING_PTR(btline), (int)RSTRING_LEN(btline)));
				builder.append_ascii("\n");
			}
		}
		if (ary)
			btline = ARY_PTR(ary)[0];
		else
			btline = decode_location(mrb, &bt->locations[0]);
		if (mrb_string_p(btline)) {
			builder.append_ascii(Span(RSTRING_PTR(btline), (int)RSTRING_LEN(btline)));
			builder.append_ascii(": ");
		}
	} else {
		builder.append_ascii(UNKNOWN_LOCATION ": ");
	}

	if (exc == mrb->nomem_err) {
		static const char nomem[] = "Out of memory (NoMemoryError)\n";
		builder.append_ascii("Out of memory (NoMemoryError)\n");
	} else {
		mrb_value output = mrb_exc_get_output(mrb, exc);
		builder.append_ascii(Span(RSTRING_PTR(output), RSTRING_LEN(output)));
		builder += '\n';
	}
	if (builder.length() > 0) {
		print_error(builder);
	}
}

MRB_API void
mrb_print_backtrace(mrb_state *mrb) {
	if (!mrb->exc || mrb->exc->tt != MRB_TT_EXCEPTION) {
		return;
	}

	print_backtrace(mrb, mrb->exc, ((struct RException *)mrb->exc)->backtrace);
}*/

extern "C" void (*print_str_hook)(const char *str, size_t len, int stream);

/* Register custom puts method with mruby */
void mrb_init_godot_print(mrb_state *mrb) {
	struct RClass *kernel = mrb->kernel_module;
	mrb_define_method(mrb, kernel, "puts", godot_puts, MRB_ARGS_ANY());
	print_str_hook = &print;
}

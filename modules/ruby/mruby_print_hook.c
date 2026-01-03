/* Custom print implementations for MRB_NO_STDIO
 * Redirects all mruby output to Godot's printing system
 */

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifndef stdout
#define stdout ((void*)0)
#define STDOUT_OVERRIDE
#endif

#ifndef stderr
#define stderr ((void*)1)
#define STDERR_OVERRIDE
#endif

void (*print_str_hook)(const char *str, size_t len) = NULL;

/* Print an mruby value as a string */
void godot_print_str(mrb_state *mrb, mrb_value obj) {
	if (!mrb_string_p(obj)) {
		obj = mrb_obj_as_string(mrb, obj);
	}
	if (print_str_hook) {
		print_str_hook(RSTRING_PTR(obj), RSTRING_LEN(obj));
	}
}

void
printcstr(mrb_state *mrb, const char *str, size_t len, void *stream) {
	if (stream == stdout) {
	} else if (stream == stderr) {
	}
	if (print_str_hook) {
		print_str_hook(str, len);
	}
}

void
printstr(mrb_state *mrb, mrb_value obj, void *stream) {
	if (!mrb_string_p(obj)) {
		obj = mrb_obj_as_string(mrb, obj);
	}
	printcstr(mrb, RSTRING_PTR(obj), RSTRING_LEN(obj), stream);
}

void
printstrln(mrb_state *mrb, mrb_value obj, void *stream) { // NOLINT(clang-diagnostic-unused-function)
	printstr(mrb, obj, stream);
	printcstr(mrb, "\n", 1, stdout);
}

void
mrb_core_init_printabort(mrb_state *mrb) {
	static const char str[] = "Failed mruby core initialization";
	printcstr(mrb, str, sizeof(str), stdout);
}

/* Kernel#print implementation */
mrb_value mrb_print_m(mrb_state *mrb, mrb_value self) {
	mrb_int argc;
	mrb_value *argv;

	mrb_get_args(mrb, "*", &argv, &argc);

	for (mrb_int i = 0; i < argc; i++) {
		mrb_value str = mrb_obj_as_string(mrb, argv[i]);
		const char *cstr = RSTRING_PTR(str);
		mrb_int len = RSTRING_LEN(str);
		printcstr(mrb, cstr, len, stdout);
	}

	return mrb_nil_value();
}

/* Kernel#p implementation - prints inspect() output */
void mrb_p(mrb_state *mrb, mrb_value obj) {
	if (mrb_type(obj) == MRB_TT_EXCEPTION && mrb_obj_ptr(obj) == mrb->nomem_err) {
		const char OOMstr[] = "Out of memory";
		printcstr(mrb, OOMstr, sizeof(OOMstr), stderr);
	} else {
		mrb_value inspected = mrb_inspect(mrb, obj);
		godot_print_str(mrb, inspected);
	}
}

/* Show mruby version */
void mrb_show_version(mrb_state *mrb) {
	mrb_value desc = mrb_const_get(mrb, mrb_obj_value(mrb->object_class),
			mrb_intern_lit(mrb, "MRUBY_DESCRIPTION"));
	if (mrb_string_p(desc)) {
		godot_print_str(mrb, desc);
	}
}

/* Show mruby copyright */
void mrb_show_copyright(mrb_state *mrb) {
	mrb_value copyright = mrb_const_get(mrb, mrb_obj_value(mrb->object_class),
			mrb_intern_lit(mrb, "MRUBY_COPYRIGHT"));
	if (mrb_string_p(copyright)) {
		godot_print_str(mrb, copyright);
	}
}

#ifdef STDOUT_OVERRIDE
#undef stdout
#undef STDOUT_OVERRIDE
#endif

#ifdef STDERR_OVERRIDE
#undef stderr
#undef STDERR_OVERRIDE
#endif

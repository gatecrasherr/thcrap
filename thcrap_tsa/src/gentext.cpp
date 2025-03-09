/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Translation of generic plaintext with a fixed number of lines.
  */

#include <thcrap.h>

typedef struct {
	const json_t *file;
	size_t key_len;
	char *key;
	size_t line;
} gentext_cache_t;

THREAD_LOCAL(gentext_cache_t, gc_tls, nullptr, nullptr);

int gentext_cache_key_set(gentext_cache_t *gc, const char *key, size_t key_len)
{
	if(gc->key && !strncmp(gc->key, key, gc->key_len)) {
		return 1;
	}
	if(key_len > gc->key_len) {
		char *key_realloc = (char *)realloc(gc->key, key_len);
		if(key_realloc) {
			gc->key = key_realloc;
			gc->key_len = key_len;
		} else {
			SAFE_FREE(gc->key);
			gc->key_len = 0;
		}
	}
	if(gc->key) {
		strncpy(gc->key, key, key_len);
	}
	gc->line = 0;
	return 0;
}

size_t BP_gentext(x86_reg_t *regs, json_t *bp_info)
{
	gentext_cache_t *gc = gc_tls_get();

	// Parameters
	// ----------
	json_t *strs = json_object_get(bp_info, "str");
	const char *file = json_object_get_string(bp_info, "file");
	json_t *ids = json_object_get(bp_info, "ids");
	json_t *line_obj = json_object_get(bp_info, "line");
	// ----------
	if(file) {
		gc->file = jsondata_game_get(file);
	}
	if(ids) {
		constexpr size_t value_len = DECIMAL_DIGITS_BOUND(size_t);
		size_t key_new_len = value_len * json_flex_array_size(ids) + 1;
		VLA(char, key_new, key_new_len);
		char *p = key_new;
		json_t *id;
		json_flex_array_foreach_scoped(size_t, i, ids, id) {
			char id_str[value_len + 2];
			size_t id_val = json_immediate_value(id, regs);
			sprintf(id_str, "%zd", id_val);
			const char *q = id_str;
			if(i > 0) {
				*p++ = '_';
			}
			while ((*p = *q++)) ++p;
		}
		gentext_cache_key_set(gc, key_new, key_new_len);
		VLA_FREE(key_new);
	}
	if(line_obj) {
		gc->line = json_immediate_value(line_obj, regs);
	}
	if(strs) {
		json_t *id_val = json_object_get(gc->file, gc->key);
		json_t *str;

		// More straightforward if we ensure that everything below is valid.
		if(json_flex_array_size(id_val) == 0) {
			return 1;
		}

		json_flex_array_foreach_scoped(size_t, i, strs, str) {
			const char **target = (const char **)json_pointer_value(str, regs);
			const char *line = json_flex_array_get_string_safe(id_val, gc->line++);
			assert(line);
			*target = line;
		}

		if (size_t eip_jump_dist = json_object_get_immediate(bp_info, regs, "eip_jump_dist")) {
			regs->retaddr += eip_jump_dist;
			return 0;
		}

		return breakpoint_cave_exec_flag_eval(regs, bp_info);
	}
	return 1;
}

int gentext_mod_init(void)
{
	// Resolve all necessary files in advance
	constexpr char prefix[] = "gentext";
	size_t prefix_len = strlen(prefix);
	json_t *breakpoints = json_object_get(runconfig_json_get(), "breakpoints");
	const char *key;
	json_t *val;
	json_object_foreach_fast(breakpoints, key, val) {
		if(!strncmp(key, prefix, prefix_len)) {
			const char *file = json_object_get_string(val, "file");
			if(!jsondata_game_get(file)) {
				jsondata_game_add(file);
			}
		}
	}
	return 0;
}

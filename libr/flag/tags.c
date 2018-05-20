/* radare - LGPL - Copyright 2018 - pancake */

#include <r_flag.h>

R_API RList *r_flag_tags_set(RFlag *f, const char *name, const char *words) {
	const char *k = sdb_fmt ("tag.%s", name);
	sdb_set (f->tags, k, words, -1);
	return NULL;
}

R_API RList *r_flag_tags_list(RFlag *f) {
	RList *res = r_list_newf (free);
	SdbList *o = sdb_foreach_list (f->tags, false);
	SdbListIter *iter;
	const char *tag;
	SdbKv *kv;
	ls_foreach (o, iter, kv) {
		const char *tag = kv->key;
		if (strlen (tag) < 5) {
			continue;
		}
		char *tagName = strdup (tag + 4);
		r_list_append (res, (void *)strdup (tagName));
	}
	ls_free (o);
	return res;
}

R_API void r_flag_tags_reset(RFlag *f, const char *name) {
	sdb_reset (f->tags);
}

R_API RList *r_flag_tags_get(RFlag *f, const char *name) {
	const char *k = sdb_fmt ("tag.%s", name);
	RListIter *iter, *iter2;
	const char *word;
	RFlagItem *flag;
	char *words = sdb_get (f->tags, k, NULL);
	RList *res = r_list_newf (NULL);
	RList *list = r_str_split_list (words, " ");
	r_list_foreach (f->flags, iter2, flag) {
		r_list_foreach (list, iter, word) {
			if (r_str_glob (flag->name, word)) {
				r_list_append (res, flag);
			}
		}
	}
	r_list_free (list);
	return res;
}

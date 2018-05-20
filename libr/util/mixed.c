/* radare - LGPL - Copyright 2011 pancake<nopcode.org> */

#include <r_util.h>

#define STR64OFF(x) sdb_fmt ("%"PFMT64x, x)
#define STR32OFF(x) sdb_fmt ("%"PFMT32x, x)

R_API RMixed *r_mixed_new () {
	RMixed *m = R_NEW (RMixed);
	if (!m) {
		return NULL;
	}
	memset (m->keys, 0, sizeof (m->keys));
	m->list = r_list_new ();
	return m;
}

R_API void r_mixed_free (RMixed *m) {
	int i;
	for (i = 0; i < RMIXED_MAXKEYS; i++) {
		if (m->keys[i]) {
			switch (m->keys[i]->size) {
			case 1: case 2: case 4:
				ht_free (m->keys[i]->hash.ht);
				break;
			case 8: 
				ht_free (m->keys[i]->hash.ht64);
				break;
			}
			free (m->keys[i]);
			m->keys[i] = NULL;
		}
	}
	r_list_purge (m->list);
	free (m);
}

R_API int r_mixed_key_check(RMixed *m, int key, int sz) {
	if (key >= 0 && key < RMIXED_MAXKEYS) {
		if (sz == 1 || sz == 2 || sz == 4 || sz == 8)
			return true;
	}
	return false;
}

static void _mixed_free_kv(HtKv *kv) {
	free (kv->key);
	r_list_free (kv->value);
	free (kv);
}

#define R_MIXED_KEY(m,x,y,z) r_mixed_key(m, r_offsetof(x,z), sizeof(y->z))
R_API int r_mixed_key(RMixed *m, int key, int size) {
	if (size > 0 && r_mixed_key_check (m, key, size)) {
		if (m->keys[key]) {
			m->keys[key]->size = size;
		} else {
			m->keys[key] = R_NEW (RMixedData);
			if (!m->keys[key]) {
				return false;
			}
			m->keys[key]->size = size;
			switch (size) {
			case 1: case 2: case 4:
				m->keys[key]->hash.ht = ht_new (NULL, _mixed_free_kv, NULL);
				return true;
			case 8: m->keys[key]->hash.ht64 = ht_new (NULL, _mixed_free_kv, NULL);
				return true;
			}
		}
	}
	return false;
}

// static?
R_API ut64 r_mixed_get_value(int key, int sz, const void *p) {
	switch (sz) {
	case 1: return (ut64) *((ut8*)((ut8*)p + key));
	case 2: return (ut64) *((ut16*)((ut8*)p + key));
	case 4: return (ut64) *((ut32*)((ut8*)p + key));
	case 8: return (ut64) *((ut32*)((ut8*)p + key));
	}
	return 0LL;
}

R_API RList *r_mixed_get (RMixed *m, int key, ut64 value) {
	if (key >= 0 && key < RMIXED_MAXKEYS) {
		if (m->keys[key]) {
			switch (m->keys[key]->size) {
			case 1: 
			case 2: 
			case 4:
				return ht_find (m->keys[key]->hash.ht, STR32OFF (value), NULL);
			case 8: 
				return ht_find (m->keys[key]->hash.ht64, STR64OFF (value), NULL);
			}
		}
	}
	return NULL;
}

R_API void *r_mixed_get0 (RMixed *m, int key, ut64 value) {
	RList *list = r_mixed_get (m, key, value);
	if (list && !r_list_empty (list)) {
		RListIter *head = r_list_head (list);
		if (head) {
			return head->data;
		}
	}
	return NULL;
}


R_API int r_mixed_add(RMixed *m, void *p) {
	SdbHash *ht;
	SdbHash *ht64;
	RList *list = NULL;
	ut64 value;
	int i, size, ret = false;;
	r_list_append (m->list, p);
	for (i = 0; i < RMIXED_MAXKEYS; i++) {
		if (!m->keys[i]) {
			continue;
		}
		size = m->keys[i]->size;
		value = r_mixed_get_value (i, size, p);
		switch (size) {
		case 1: case 2: case 4:
			ht = m->keys[i]->hash.ht;
			list = ht_find (ht, STR32OFF (value), NULL);
			if (!list) {
				list = r_list_new ();
				ht_insert (ht, STR32OFF (value), list);
			}
			r_list_append (list, p);
			ret = true;
			break;
		case 8:
			ht64 = m->keys[i]->hash.ht64;
			list = ht_find (ht64, STR64OFF (value), NULL);
			if (!list) {
				list = r_list_new ();
				ht_insert (ht64, STR64OFF (value), list);
			}
			r_list_append (list, p);
			ret = true;
			break;
		}
	}
	return ret;
}

R_API int r_mixed_del (RMixed *m, void *p) {
	int i;
	r_list_delete_data (m->list, p);
	for (i = 0; i < RMIXED_MAXKEYS; i++) {
		ut64 value = r_mixed_get_value (i, m->keys[i]->size, p);
		if (!m->keys[i]) continue;
		switch (m->keys[i]->size) {
		case 1: case 2: case 4:
			ht_delete (m->keys[i]->hash.ht, STR32OFF (value));
			break;
		case 8: 
			ht_delete (m->keys[i]->hash.ht64, STR64OFF (value));
			break;
		}
	}
	return false;
}

R_API void r_mixed_change_begin(RMixed *m, void *p) {
	int i;
	for (i = 0; i < RMIXED_MAXKEYS; i++)
		if (m->keys[i]) {
			m->state[i] = r_mixed_get_value (i, m->keys[i]->size, p);
			eprintf ("store state %d (0x%08"PFMT64x")\n", i, m->state[i]);
		}
}

R_API bool r_mixed_change_end(RMixed *m, void *p) {
	int i;
	void *q;
	for (i = 0; i < RMIXED_MAXKEYS; i++) {
		if (m->keys[i]) {
			SdbHash *ht = m->keys[i]->hash.ht;
			SdbHash *ht64 = m->keys[i]->hash.ht64;
			ut64 newstate = r_mixed_get_value (i, m->keys[i]->size, p);
			if (newstate != m->state[i]) {
				// rehash this pointer
				RListIter *iter;
				RList *list = r_mixed_get (m, i, m->state[i]);
				if (!list) {
					eprintf ("RMixed internal corruption?\n");
					return false;
				}
				/* No _safe loop necessary because we return immediately after the delete. */
				r_list_foreach (list, iter, q) {
					if (q == p) {
						r_list_delete (list, iter);
						break;
					}
				}
				if (r_list_empty (list)) {
					// delete hash entry
					r_list_free (list);
					switch (m->keys[i]->size) {
					case 1: case 2: case 4:
						ht_delete (ht, STR32OFF (m->state[i]));
						break;
					case 8: 
					   	ht_delete (ht64, STR64OFF (m->state[i]));
						break;
					}
				}
				switch (m->keys[i]->size) {
				case 1: case 2: case 4:
					list = ht_find (ht, STR32OFF (newstate), NULL);
					if (!list) {
						list = r_list_new ();
						ht_insert (ht, STR32OFF (newstate), list);
					}
					r_list_append (list, p);
					break;
				case 8:
					list = ht_find (ht64, STR64OFF (newstate), NULL);
					if (!list) {
						list = r_list_new ();
						ht_insert (ht64, STR64OFF (newstate), list);
					}
					r_list_append (list, p);
					break;
				}
			}
		}
	}
	return true;
}

#if TEST
typedef struct {
	char *name;
	ut32 hashname;
	int length;
	ut64 offset;
} TestStruct;

TestStruct *test_struct_new(const char *name, int length, ut64 offset) {
	TestStruct *ts = R_NEW (TestStruct);
	ts->name = strdup (name);
	ts->hashname = r_str_hash (name);
	ts->length = length;
	ts->offset = offset;
	return ts;
}

void test_struct_free(TestStruct *ts) {
	free (ts->name);
	free (ts);
}

int main () {
	RList *list;
	RListIter *iter;
	TestStruct *ts, *ts2;
	RMixed *mx = r_mixed_new ();
	R_MIXED_KEY (mx, TestStruct, ts, hashname);
	R_MIXED_KEY (mx, TestStruct, ts, length);
	R_MIXED_KEY (mx, TestStruct, ts, offset);

	ts2 = test_struct_new ("FOOD", 444, 0x123456789);
	r_mixed_add (mx, ts2);

	r_mixed_add (mx, test_struct_new ("food", 12, 0x839481222000));
	r_mixed_add (mx, test_struct_new ("food", 12, 0x839481222000));
	r_mixed_add (mx, test_struct_new ("baar", 12, 0x441242910));
	r_mixed_add (mx, test_struct_new ("cocktel", 12, 0x224944));
	r_mixed_add (mx, test_struct_new ("cocktel2", 16, 0x224944));
	r_mixed_add (mx, test_struct_new ("cocktel3", 17, 0x224944));

	ts = r_mixed_get0 (mx, r_offsetof (TestStruct, hashname), (ut64)r_str_hash ("food"));
	if (ts) {
		printf ("NAM: %s\n", ts->name);
		printf ("LEN: %d\n", ts->length);
		printf ("OFF: %llx\n", ts->offset);
	} else eprintf ("oops. cannot find 'food'\n");

	eprintf ("--\n");
	list = r_mixed_get (mx, r_offsetof (TestStruct, offset), 0x224944);
	r_list_foreach (list, iter, ts) {
		printf ("NAM: %s\n", ts->name);
		printf ("LEN: %d\n", ts->length);
		printf ("OFF: %llx\n", ts->offset);
	}

	r_mixed_change_begin (mx, ts2);
	ts2->length = 666;
	if (!r_mixed_change_end (mx, ts2)) {
		eprintf ("MixedChange failed\n");
	}
	r_mixed_free (mx);
}
#endif

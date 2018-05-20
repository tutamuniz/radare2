/* radare - LGPL - Copyright 2009-2017 - pancake */

#include <r_debug.h>
#include <r_list.h>

R_API void r_debug_map_list(RDebug *dbg, ut64 addr, int rad) {
	int i;
	char buf[128];
	bool notfirst = false;
	RListIter *iter;
	RDebugMap *map;
	if (!dbg) {
		return;
	}

	if (rad == 'j') {
		dbg->cb_printf ("[");
	}

	for (i = 0; i < 2; i++) {
		RList *maps = (i == 0) ? dbg->maps : dbg->maps_user;
		r_list_foreach (maps, iter, map) {
			switch (rad) {
			case 'j':
				dbg->cb_printf ("%s{", notfirst? ",": "");
				if (map->name && *map->name) {
					char *escaped_name = r_str_escape (map->name);
					dbg->cb_printf ("\"name\":\"%s\",", escaped_name);
					free (escaped_name);
				}
				if (map->file && *map->file) {
					char *escaped_path = r_str_escape (map->file);
					dbg->cb_printf ("\"file\":\"%s\",", escaped_path);
					free (escaped_path);
				}
				dbg->cb_printf ("\"addr\":%"PFMT64u",", map->addr);
				dbg->cb_printf ("\"addr_end\":%"PFMT64u",", map->addr_end);
				dbg->cb_printf ("\"type\":\"%c\",", map->user?'u':'s');
				dbg->cb_printf ("\"perm\":\"%s\"", r_str_rwx_i (map->perm));
				dbg->cb_printf ("}");
				notfirst = true;
				break;
			case '*':
				{
					char *name = (map->name && *map->name)
						? r_str_newf ("%s.%s", map->name, r_str_rwx_i (map->perm))
						: r_str_newf ("%08"PFMT64x".%s", map->addr, r_str_rwx_i (map->perm));
					r_name_filter (name, 0);
					dbg->cb_printf ("f map.%s 0x%08"PFMT64x" 0x%08"PFMT64x"\n",
						name, map->addr_end - map->addr + 1, map->addr);
					free (name);
				}
				break;
			case 'q':
				{
					char *name = (map->name && *map->name)
						? r_str_newf ("%s.%s", map->name, r_str_rwx_i (map->perm))
						: r_str_newf ("%08"PFMT64x".%s", map->addr, r_str_rwx_i (map->perm));
					r_name_filter (name, 0);
					dbg->cb_printf ("0x%016"PFMT64x" - 0x%016"PFMT64x" %6s %5s %s\n",
						map->addr, map->addr_end,
						r_num_units (buf, map->addr_end - map->addr),
						r_str_rwx_i (map->perm), name);
					free (name);
				}
				break;
			default:
				{
					const char *fmtstr = dbg->bits & R_SYS_BITS_64
						? "0x%016"PFMT64x" # 0x%016"PFMT64x" %c %s %6s %c %s %s %s%s%s\n"
						: "0x%08"PFMT64x" # 0x%08"PFMT64x" %c %s %6s %c %s %s %s%s%s\n";
					const char *type = map->shared? "sys": "usr";
					const char *flagname = dbg->corebind.getName
						? dbg->corebind.getName (dbg->corebind.core, map->addr) : NULL;
					if (!flagname) {
						flagname = "";
					} else if (map->name) {
						char *filtered_name = strdup (map->name);
						r_name_filter (filtered_name, 0);
						if (!strncmp (flagname, "map.", 4) && \
							!strcmp (flagname + 4, filtered_name)) {
							flagname = "";
						}
						free (filtered_name);
					}
					dbg->cb_printf (fmtstr,
						map->addr,
						map->addr_end,
						(addr>=map->addr && addr<map->addr_end)?'*':'-',
						type, r_num_units (buf, map->size),
						map->user?'u':'s',
						r_str_rwx_i (map->perm),
						map->name?map->name:"?",
						map->file?map->file:"?",
						*flagname? " ; ": "",
						flagname);
				}
				break;
			}
		}
	}

	if (rad == 'j') {
		dbg->cb_printf ("]\n");
	}
}

static int cmp(const void *a, const void *b) {
	RDebugMap *ma = (RDebugMap*) a;
	RDebugMap *mb = (RDebugMap*) b;
	return ma->addr - mb->addr;
}

static int findMinMax(RList *maps, ut64 *min, ut64 *max, int skip, int width) {
	RDebugMap *map;
	RListIter *iter;
	*min = UT64_MAX;
	*max = 0;
	r_list_foreach (maps, iter, map) {
		if (skip > 0) {
			skip--;
			continue;
		}
		if (map->addr < *min) {
			*min = map->addr;
		}
		if (map->addr_end > *max) {
			*max = map->addr_end;
		}
	}
	return (*max - *min) / width;
}

static void print_debug_map_ascii_art(RList *maps, ut64 addr, int use_color, PrintfCallback cb_printf, int bits, int cons_width) {
	ut64 mul, min = -1, max = 0;
	int width = cons_width - 90;
	RListIter *iter;
	RDebugMap *map;
	if (width < 1) {
		width = 30;
	}
	r_list_sort (maps, cmp);
	mul = findMinMax (maps, &min, &max, 0, width);
	ut64 last = min;
	if (min != -1 && mul != 0) {
		const char *c = "", *c_end = "";
		const char *fmtstr;
		char buf[56];
		int j;
		int count = 0;
		r_list_foreach (maps, iter, map) {
			r_num_units (buf, map->size);
			if (use_color) {
				c_end = Color_RESET;
				if (map->perm & 2) {
					c = Color_RED;
				} else if (map->perm & 1) {
					c = Color_GREEN;
				} else {
					c = "";
					c_end = "";
				}
			} else {
				c = "";
				c_end = "";
			}
			if ((map->addr - last) > UT32_MAX) {
				mul = findMinMax (maps, &min, &max, count, width);
			}
			count++;
			fmtstr = bits & R_SYS_BITS_64 ?
				"map %.8s %c %s0x%016"PFMT64x"%s |" :
				"map %.8s %c %s0x%08"PFMT64x"%s |";
			cb_printf (fmtstr, buf,
				(addr >= map->addr && \
				addr < map->addr_end) ? '*' : '-',
				c, map->addr, c_end);
			for (j = 0; j < width; j++) {
				ut64 pos = min + (j * mul);
				ut64 npos = min + ((j + 1) * mul);
				if (map->addr < npos && map->addr_end > pos) {
					cb_printf ("#");
				} else {
					cb_printf ("-");
				}
			}
			fmtstr = bits & R_SYS_BITS_64 ?
				"| %s0x%016"PFMT64x"%s %s %s\n" :
				"| %s0x%08"PFMT64x"%s %s %s\n";
			cb_printf (fmtstr, c, map->addr_end, c_end,
				r_str_rwx_i (map->perm), map->name);
			last = map->addr;
		}
	}
}

R_API void r_debug_map_list_visual(RDebug *dbg, ut64 addr, int use_color, int cons_cols) {
	if (dbg) {
		if (dbg->maps) {
			print_debug_map_ascii_art (dbg->maps, addr,
				use_color, dbg->cb_printf,
				dbg->bits, cons_cols);
		}
		if (dbg->maps_user) {
			print_debug_map_ascii_art (dbg->maps_user,
				addr, use_color, dbg->cb_printf,
				dbg->bits, cons_cols);
		}
	}
}

R_API RDebugMap *r_debug_map_new(char *name, ut64 addr, ut64 addr_end, int perm, int user) {
	RDebugMap *map;
	/* range could be 0k on OpenBSD, it's a honeypot */
	if (!name || addr > addr_end) {
		eprintf ("r_debug_map_new: error (\
			%"PFMT64x">%"PFMT64x")\n", addr, addr_end);
		return NULL;
	}
	map = R_NEW0 (RDebugMap);
	if (!map) {
		return NULL;
	}
	map->name = strdup (name);
	map->addr = addr;
	map->addr_end = addr_end;
	map->size = addr_end-addr;
	map->perm = perm;
	map->user = user;
	return map;
}

R_API RList *r_debug_modules_list(RDebug *dbg) {
	return (dbg && dbg->h && dbg->h->modules_get)?
		dbg->h->modules_get (dbg): NULL;
}

R_API int r_debug_map_sync(RDebug *dbg) {
	bool ret = false;
	if (dbg && dbg->h && dbg->h->map_get) {
		RList *newmaps = dbg->h->map_get (dbg);
		if (newmaps) {
			r_list_free (dbg->maps);
			dbg->maps = newmaps;
			ret = true;
		}
	}
	return (int)ret;
}

R_API RDebugMap* r_debug_map_alloc(RDebug *dbg, ut64 addr, int size) {
	RDebugMap *map = NULL;
	if (dbg && dbg->h && dbg->h->map_alloc) {
		map = dbg->h->map_alloc (dbg, addr, size);
	}
	return map;
}

R_API int r_debug_map_dealloc(RDebug *dbg, RDebugMap *map) {
	bool ret = false;
	ut64 addr = map->addr;
	if (dbg && dbg->h && dbg->h->map_dealloc) {
		if (dbg->h->map_dealloc (dbg, addr, map->size)) {
			ret = true;
		}
	}
	return (int)ret;
}

R_API RDebugMap *r_debug_map_get(RDebug *dbg, ut64 addr) {
	RDebugMap *map, *ret = NULL;
	RListIter *iter;
	r_list_foreach (dbg->maps, iter, map) {
		if (addr >= map->addr && addr <= map->addr_end) {
			ret = map;
			break;
		}
	}
	return ret;
}

R_API void r_debug_map_free(RDebugMap *map) {
	free (map->name);
	free (map);
}

R_API RList *r_debug_map_list_new() {
	RList *list = r_list_new ();
	if (!list) {
		return NULL;
	}
	list->free = (RListFree)r_debug_map_free;
	return list;
}

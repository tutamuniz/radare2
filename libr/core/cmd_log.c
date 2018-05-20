/* radare - LGPL - Copyright 2009-2014 - pancake */
#include <string.h>

#include "r_config.h"
#include "r_cons.h"
#include "r_core.h"

// TODO #7967 help refactor: move to another place
static const char *help_msg_L[] = {
	"Usage:", "L", "[-name][ file] # see oL, iL, dL, ...",
	"L", "", "List all plugins loaded by RCore.lib",
	"L-", "duk", "Unload plugin matching in filename",
	"L", " blah."R_LIB_EXT, "Load plugin file",
	"Ls", "", "list RCore plugins",
	"Ls", " java", "run RCore java plugin",
	NULL
};

static const char *help_msg_T[] = {
	"Usage:", "T", "[-][ num|msg]",
	"T", "", "List all Text log messages",
	"T", " message", "Add new log message",
	"T", " 123", "List log from 123",
	"T", " 10 3", "List 3 log messages starting from 10",
	"T*", "", "List in radare commands",
	"T-", "", "Delete all logs",
	"T-", " 123", "Delete logs before 123",
	"Tl", "", "Get last log message id",
	"Tj", "", "List in json format",
	"Tm", " [idx]", "Display log messages without index",
	"Ts", "", "List files in current directory (see pwd, cd)",
	"TT", "", "Enter into the text log chat console",
	NULL
};

// TODO #7967 help refactor: move L to another place
static void cmd_log_init(RCore *core) {
	DEFINE_CMD_DESCRIPTOR (core, L);
	DEFINE_CMD_DESCRIPTOR (core, T);
}

static int textlog_chat(RCore *core) {
	char prompt[64];
	char buf[1024];
	int lastmsg = 0;
	const char *me = r_config_get (core->config, "cfg.user");
	char msg[1024];

	eprintf ("Type '/help' for commands:\n");
	snprintf (prompt, sizeof (prompt) - 1, "[%s]> ", me);
	r_line_set_prompt (prompt);
	for (;;) {
		r_core_log_list (core, lastmsg, 0, 0);
		lastmsg = core->log->last;
		if (r_cons_fgets (buf, sizeof (buf) - 1, 0, NULL) < 0) {
			return 1;
		}
		if (!*buf) {
			continue;
		}
		if (!strcmp (buf, "/help")) {
			eprintf ("/quit           quit the chat (same as ^D)\n");
			eprintf ("/name <nick>    set cfg.user name\n");
			eprintf ("/log            show full log\n");
			eprintf ("/clear          clear text log messages\n");
		} else if (!strncmp (buf, "/name ", 6)) {
			snprintf (msg, sizeof (msg) - 1, "* '%s' is now known as '%s'", me, buf + 6);
			r_core_log_add (core, msg);
			r_config_set (core->config, "cfg.user", buf + 6);
			me = r_config_get (core->config, "cfg.user");
			snprintf (prompt, sizeof (prompt) - 1, "[%s]> ", me);
			r_line_set_prompt (prompt);
			return 0;
		} else if (!strcmp (buf, "/log")) {
			r_core_log_list (core, 0, 0, 0);
			return 0;
		} else if (!strcmp (buf, "/clear")) {
			// r_core_log_del (core, 0);
			r_core_cmd0 (core, "T-");
			return 0;
		} else if (!strcmp (buf, "/quit")) {
			return 0;
		} else if (*buf == '/') {
			eprintf ("Unknown command: %s\n", buf);
		} else {
			snprintf (msg, sizeof (msg) - 1, "[%s] %s", me, buf);
			r_core_log_add (core, msg);
		}
	}
	return 1;
}

static int cmd_log(void *data, const char *input) {
	RCore *core = (RCore *) data;
	const char *arg, *input2;
	int n, n2;

	if (!input) {
		return 1;
	}

	input2 = (input && *input)? input + 1: "";
	arg = strchr (input2, ' ');
	n = atoi (input2);
	n2 = arg? atoi (arg + 1): 0;

	switch (*input) {
	case 'e': // shell: less
	{
		char *p = strchr (input, ' ');
		if (p) {
			char *b = r_file_slurp (p + 1, NULL);
			if (b) {
				r_cons_less_str (b, NULL);
				free (b);
			} else {
				eprintf ("File not found\n");
			}
		} else {
			eprintf ("Usage: less [filename]\n");
		}
	}
	break;
	case 'l':
		r_cons_printf ("%d\n", core->log->last - 1);
		break;
	case '-':
		r_core_log_del (core, n);
		break;
	case '?':
		r_core_cmd_help (core, help_msg_T);
		break;
	case 'T':
		if (r_config_get_i (core->config, "scr.interactive")) {
			textlog_chat (core);
		} else {
			eprintf ("Only available when the screen is interactive\n");
		}
		break;
	case ' ':
		if (n > 0) {
			r_core_log_list (core, n, n2, *input);
		} else {
			r_core_log_add (core, input + 1);
		}
		break;
	case 'm':
		if (n > 0) {
			r_core_log_list (core, n, 1, 't');
		} else {
			r_core_log_list (core, n, 0, 't');
		}
		break;
	case 'j':
	case '*':
	case '\0':
		r_core_log_list (core, n, n2, *input);
		break;
	}
	return 0;
}

static int cmd_plugins(void *data, const char *input) {
	RCore *core = (RCore *) data;
	switch (input[0]) {
	case 0:
		r_lib_list (core->lib);
		break;
	case '-':
		r_lib_close (core->lib, input + 2);
		break;
	case ' ':
		r_lib_open (core->lib, input + 2);
		break;
	case '?':
		r_core_cmd_help (core, help_msg_L);
		break;
	case 's': {
		RListIter *iter;
		RCorePlugin *cp;
		switch (input[1]) {
		case 'j': {
			r_cons_printf("[");
			bool is_first_element = true;
			r_list_foreach (core->rcmd->plist, iter, cp) {
				r_cons_printf ("%s{\"Name\":\"%s\",\"Description\":\"%s\"}",
					is_first_element? "" : ",", cp->name, cp->desc);
				is_first_element = false;
			}
			r_cons_printf("]\n");
			break;
			}
		case 0 :
			r_list_foreach (core->rcmd->plist, iter, cp) {
				r_cons_printf ("%s: %s\n", cp->name, cp->desc);
			}
			break;
		default:
			return r_core_cmd0 (core, input + 2);
		}
		}
		break;
	}
	return 0;
}

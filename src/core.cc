/*
 *
 * Conky, a system monitor, based on torsmo
 *
 * Any original torsmo code is licensed under the BSD license
 *
 * All code written since the fork of torsmo is licensed under the GPL
 *
 * Please see COPYING for details
 *
 * Copyright (c) 2004, Hannu Saransaari and Lauri Hakkarainen
 * Copyright (c) 2005-2024 Brenden Matthews, Philip Kovacs, et. al.
 *	(see AUTHORS)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

/* local headers */
#include "algebra.h"
#include "core.h"

#include "bsdapm.h"
#include "build.h"
#include "colour-settings.h"
#include "colours.h"
#include "combine.h"
#include "diskio.h"
#include "entropy.h"
#include "exec.h"
#include "i8k.h"
#include "misc.h"
#include "proc.h"
#include "text_object.h"
#ifdef BUILD_IMLIB2
#include "conky-imlib2.h"
#endif /* BUILD_IMLIB2 */
#ifdef BUILD_MYSQL
#include "mysql.h"
#endif /* BUILD_MYSQL */
#ifdef BUILD_ICAL
#include "ical.h"
#endif /* BUILD_ICAL */
#ifdef BUILD_IRC
#include "irc.h"
#endif /* BUILD_IRC */
#ifdef BUILD_GUI
#include "fonts.h"
#include "gui.h"
#endif /* BUILD_GUI */
#include "fs.h"
#ifdef BUILD_IBM
#include "ibm.h"
#include "smapi.h"
#endif /* BUILD_IBM */
#ifdef BUILD_ICONV
#include "iconv_tools.h"
#endif /* BUILD_ICONV */
#include "llua.h"
#include "logging.h"
#include "mail.h"
#include "mboxscan.h"
#include "mixer.h"
#include "nc.h"
#include "net_stat.h"
#ifdef BUILD_NVIDIA
#include "nvidia.h"
#endif /* BUILD_NVIDIA */
#include <inttypes.h>
#include "cpu.h"
#include "read_tcpip.h"
#include "scroll.h"
#include "specials.h"
#include "tailhead.h"
#include "temphelper.h"
#include "template.h"
#include "timeinfo.h"
#include "top.h"
#include "user.h"
#include "users.h"
#ifdef BUILD_CURL
#include "ccurl_thread.h"
#endif /* BUILD_CURL */
#ifdef BUILD_RSS
#include "rss.h"
#endif /* BUILD_RSS */
#ifdef BUILD_AUDACIOUS
#include "audacious.h"
#endif /* BUILD_AUDACIOUS */
#ifdef BUILD_CMUS
#include "cmus.h"
#endif /* BUILD_CMUS */
#ifdef BUILD_JOURNAL
#include "journal.h"
#endif /* BUILD_JOURNAL */
#ifdef BUILD_PULSEAUDIO
#include "pulseaudio.h"
#endif /* BUILD_PULSEAUDIO */
#ifdef BUILD_INTEL_BACKLIGHT
#include "intel_backlight.h"
#endif /* BUILD_INTEL_BACKLIGHT */

/* check for OS and include appropriate headers */
#if defined(__linux__)
#include "linux.h"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include "freebsd.h"
#elif defined(__DragonFly__)
#include "dragonfly.h"
#elif defined(__OpenBSD__)
#include "openbsd.h"
#elif defined(__APPLE__) && defined(__MACH__)
#include "darwin.h"
#endif

#define STRNDUP_ARG strndup(arg ? arg : "", text_buffer_size.get(*state))

#include <cctype>
#include <cstring>

/* strip a leading /dev/ if any, following symlinks first
 *
 * BEWARE: this function returns a pointer to static content
 *         which gets overwritten in consecutive calls. I.e.:
 *         this function is NOT reentrant.
 */
const char *dev_name(const char *path) {
  static char buf[PATH_MAX];

  if (path == nullptr) { return nullptr; }

#define DEV_NAME(x)                                                         \
  ((x) != nullptr && strlen(x) > 5 && strncmp(x, "/dev/", 5) == 0 ? (x) + 5 \
                                                                  : (x))
  if (realpath(path, buf) == nullptr) { return DEV_NAME(path); }
  return DEV_NAME(buf);
#undef DEV_NAME
}

static struct text_object *new_text_object_internal() {
  auto *obj = static_cast<text_object *>(malloc(sizeof(struct text_object)));
  memset(obj, 0, sizeof(struct text_object));
  return obj;
}

static struct text_object *create_plain_text(const char *s) {
  struct text_object *obj;

  if (s == nullptr || *s == '\0') { return nullptr; }

  obj = new_text_object_internal();

  obj_be_plain_text(obj, s);
  return obj;
}

#ifdef BUILD_CURL
void stock_parse_arg(struct text_object *obj, const char *arg) {
  char stock[8];
  char data[16];

  obj->data.s = nullptr;
  if (sscanf(arg, "%7s %15s", stock, data) != 2) {
    NORM_ERR("wrong number of arguments for $stock");
    return;
  }
  if (!strcasecmp("ask", data)) {
    strncpy(data, "a", 3);
  } else if (!strcasecmp("adv", data)) {
    strncpy(data, "a2", 3);
  } else if (!strcasecmp("asksize", data)) {
    strncpy(data, "a5", 3);
  } else if (!strcasecmp("bid", data)) {
    strncpy(data, "b", 3);
  } else if (!strcasecmp("askrt", data)) {
    strncpy(data, "b2", 3);
  } else if (!strcasecmp("bidrt", data)) {
    strncpy(data, "b3", 3);
  } else if (!strcasecmp("bookvalue", data)) {
    strncpy(data, "b4", 3);
  } else if (!strcasecmp("bidsize", data)) {
    strncpy(data, "b6", 3);
  } else if (!strcasecmp("change", data)) {
    strncpy(data, "c1", 3);
  } else if (!strcasecmp("commission", data)) {
    strncpy(data, "c3", 3);
  } else if (!strcasecmp("changert", data)) {
    strncpy(data, "c6", 3);
  } else if (!strcasecmp("ahcrt", data)) {
    strncpy(data, "c8", 3);
  } else if (!strcasecmp("ds", data)) {
    strncpy(data, "d", 3);
  } else if (!strcasecmp("ltd", data)) {
    strncpy(data, "d1", 3);
  } else if (!strcasecmp("tradedate", data)) {
    strncpy(data, "d2", 3);
  } else if (!strcasecmp("es", data)) {
    strncpy(data, "e", 3);
  } else if (!strcasecmp("ei", data)) {
    strncpy(data, "e1", 3);
  } else if (!strcasecmp("epsecy", data)) {
    strncpy(data, "e7", 3);
  } else if (!strcasecmp("epseny", data)) {
    strncpy(data, "e8", 3);
  } else if (!strcasecmp("epsenq", data)) {
    strncpy(data, "e9", 3);
  } else if (!strcasecmp("floatshares", data)) {
    strncpy(data, "f6", 3);
  } else if (!strcasecmp("dayslow", data)) {
    strncpy(data, "g", 3);
  } else if (!strcasecmp("dayshigh", data)) {
    strncpy(data, "h", 3);
  } else if (!strcasecmp("52weeklow", data)) {
    strncpy(data, "j", 3);
  } else if (!strcasecmp("52weekhigh", data)) {
    strncpy(data, "k", 3);
  } else if (!strcasecmp("hgp", data)) {
    strncpy(data, "g1", 3);
  } else if (!strcasecmp("ag", data)) {
    strncpy(data, "g3", 3);
  } else if (!strcasecmp("hg", data)) {
    strncpy(data, "g4", 3);
  } else if (!strcasecmp("hgprt", data)) {
    strncpy(data, "g5", 3);
  } else if (!strcasecmp("hgrt", data)) {
    strncpy(data, "g6", 3);
  } else if (!strcasecmp("moreinfo", data)) {
    strncpy(data, "i", 3);
  } else if (!strcasecmp("obrt", data)) {
    strncpy(data, "i5", 3);
  } else if (!strcasecmp("mc", data)) {
    strncpy(data, "j1", 3);
  } else if (!strcasecmp("mcrt", data)) {
    strncpy(data, "j3", 3);
  } else if (!strcasecmp("ebitda", data)) {
    strncpy(data, "j4", 3);
  } else if (!strcasecmp("c52wlow", data)) {
    strncpy(data, "j5", 3);
  } else if (!strcasecmp("pc52wlow", data)) {
    strncpy(data, "j6", 3);
  } else if (!strcasecmp("cprt", data)) {
    strncpy(data, "k2", 3);
  } else if (!strcasecmp("lts", data)) {
    strncpy(data, "k3", 3);
  } else if (!strcasecmp("c52whigh", data)) {
    strncpy(data, "k4", 3);
  } else if (!strcasecmp("pc52whigh", data)) {
    strncpy(data, "k5", 3);
  } else if (!strcasecmp("ltp", data)) {
    strncpy(data, "l1", 3);
  } else if (!strcasecmp("hl", data)) {
    strncpy(data, "l2", 3);
  } else if (!strcasecmp("ll", data)) {
    strncpy(data, "l3", 3);
  } else if (!strcasecmp("dr", data)) {
    strncpy(data, "m", 3);
  } else if (!strcasecmp("drrt", data)) {
    strncpy(data, "m2", 3);
  } else if (!strcasecmp("50ma", data)) {
    strncpy(data, "m3", 3);
  } else if (!strcasecmp("200ma", data)) {
    strncpy(data, "m4", 3);
  } else if (!strcasecmp("c200ma", data)) {
    strncpy(data, "m5", 3);
  } else if (!strcasecmp("pc200ma", data)) {
    strncpy(data, "m6", 3);
  } else if (!strcasecmp("c50ma", data)) {
    strncpy(data, "m7", 3);
  } else if (!strcasecmp("pc50ma", data)) {
    strncpy(data, "m8", 3);
  } else if (!strcasecmp("name", data)) {
    strncpy(data, "n", 3);
  } else if (!strcasecmp("notes", data)) {
    strncpy(data, "n4", 3);
  } else if (!strcasecmp("open", data)) {
    strncpy(data, "o", 3);
  } else if (!strcasecmp("pc", data)) {
    strncpy(data, "p", 3);
  } else if (!strcasecmp("pricepaid", data)) {
    strncpy(data, "p1", 3);
  } else if (!strcasecmp("cip", data)) {
    strncpy(data, "p2", 3);
  } else if (!strcasecmp("ps", data)) {
    strncpy(data, "p5", 3);
  } else if (!strcasecmp("pb", data)) {
    strncpy(data, "p6", 3);
  } else if (!strcasecmp("edv", data)) {
    strncpy(data, "q", 3);
  } else if (!strcasecmp("per", data)) {
    strncpy(data, "r", 3);
  } else if (!strcasecmp("dpd", data)) {
    strncpy(data, "r1", 3);
  } else if (!strcasecmp("perrt", data)) {
    strncpy(data, "r2", 3);
  } else if (!strcasecmp("pegr", data)) {
    strncpy(data, "r5", 3);
  } else if (!strcasecmp("pepsecy", data)) {
    strncpy(data, "r6", 3);
  } else if (!strcasecmp("pepseny", data)) {
    strncpy(data, "r7", 3);
  } else if (!strcasecmp("symbol", data)) {
    strncpy(data, "s", 3);
  } else if (!strcasecmp("sharesowned", data)) {
    strncpy(data, "s1", 3);
  } else if (!strcasecmp("shortratio", data)) {
    strncpy(data, "s7", 3);
  } else if (!strcasecmp("ltt", data)) {
    strncpy(data, "t1", 3);
  } else if (!strcasecmp("tradelinks", data)) {
    strncpy(data, "t6", 3);
  } else if (!strcasecmp("tt", data)) {
    strncpy(data, "t7", 3);
  } else if (!strcasecmp("1ytp", data)) {
    strncpy(data, "t8", 3);
  } else if (!strcasecmp("volume", data)) {
    strncpy(data, "v", 3);
  } else if (!strcasecmp("hv", data)) {
    strncpy(data, "v1", 3);
  } else if (!strcasecmp("hvrt", data)) {
    strncpy(data, "v7", 3);
  } else if (!strcasecmp("52weekrange", data)) {
    strncpy(data, "w", 3);
  } else if (!strcasecmp("dvc", data)) {
    strncpy(data, "w1", 3);
  } else if (!strcasecmp("dvcrt", data)) {
    strncpy(data, "w4", 3);
  } else if (!strcasecmp("se", data)) {
    strncpy(data, "x", 3);
  } else if (!strcasecmp("dy", data)) {
    strncpy(data, "y", 3);
  } else {
    NORM_ERR(
        "\"%s\" is not supported by $stock. Supported: 1ytp, 200ma, 50ma, "
        "52weeklow, 52weekhigh, 52weekrange, adv, ag, ahcrt, ask, askrt, "
        "asksize, bid, bidrt, bidsize, bookvalue, c200ma, c50ma, c52whigh, "
        "c52wlow, change, changert, cip, commission, cprt, dayshigh, dayslow, "
        "dpd, dr, drrt, ds, dvc, dvcrt, dy, ebitda, edv, ei, epsecy, epsenq, "
        "epseny, es, floatshares, hg, hgp, hgprt, hl, hv, hvrt, ll, ltd, ltp, "
        "lts, ltt, mc, mcrt, moreinfo, name, notes, obrt, open, pb, pc, "
        "pc200ma, pc50ma, pc52whigh, pc52wlow, pegr, pepsecy, pepseny, per, "
        "perrt, pricepaid, ps, se, sharesowned, shortratio, symbol, tradedate, "
        "tradelinks, tt, volume",
        data);
    return;
  }
#define MAX_FINYAH_URL_LENGTH 75
  obj->data.s = static_cast<char *>(malloc(MAX_FINYAH_URL_LENGTH));
  snprintf(obj->data.s, MAX_FINYAH_URL_LENGTH,
           "http://download.finance.yahoo.com/d/quotes.csv?s=%s&f=%s", stock,
           data);
}
#endif /* BUILD_CURL */

legacy_cb_handle *create_cb_handle(int (*fn)()) {
  if (fn != nullptr) {
    return new legacy_cb_handle(conky::register_cb<legacy_cb>(1, fn));
  }
  { return nullptr; }
}

/* construct_text_object() creates a new text_object */
struct text_object *construct_text_object(char *s, const char *arg, long line,
                                          void **ifblock_opaque,
                                          void *free_at_crash) {
  // struct text_object *obj = new_text_object();
  struct text_object *obj = new_text_object_internal();

  obj->line = line;

  return obj;
}

/*
 * - assumes that *string is '#'
 * - removes the part from '#' to the end of line ('\n' or '\0')
 * - it removes the '\n'
 * - copies the last char into 'char *last' argument, which should be a pointer
 *   to a char rather than a string.
 */
static size_t remove_comment(char *string, char *last) {
  char *end = string;
  while (*end != '\0' && *end != '\n') { ++end; }
  if (last != nullptr) { *last = *end; }
  if (*end == '\n') { end++; }
  strfold(string, end - string);
  return end - string;
}

size_t remove_comments(char *string) {
  char *curplace;
  size_t folded = 0;
  for (curplace = string; *curplace != 0; curplace++) {
    if (*curplace == '\\' && *(curplace + 1) == '#') {
      // strcpy can't be used for overlapping strings
      strfold(curplace, 1);
      folded += 1;
    } else if (*curplace == '#') {
      folded += remove_comment(curplace, nullptr);
    }
  }
  return folded;
}

int extract_variable_text_internal(struct text_object *retval,
                                   const char *const_p) {
  struct text_object *obj;
  char *p, *s, *orig_p;
  long line;
  void *ifblock_opaque = nullptr;
  char *tmp_p;
  char *arg = nullptr;
  size_t len = 0;

  p = strndup(const_p, max_user_text.get(*state) - 1);
  while (text_contains_templates(p) != 0) {
    char *tmp;
    tmp = find_and_replace_templates(p);
    free(p);
    p = tmp;
  }
  s = orig_p = p;

  if (static_cast<int>(strcmp(p, const_p) != 0) != 0) {
    DBGP2("replaced all templates in text: input is\n'%s'\noutput is\n'%s'",
          const_p, p);
  } else {
    DBGP2("no templates to replace");
  }

  memset(retval, 0, sizeof(struct text_object));

  line = global_text_lines;

  while (*p != 0) {
    if (*p == '\n') { line++; }
    if (*p == '$') {
      *p = '\0';
      obj = create_plain_text(s);
      if (obj != nullptr) { append_object(retval, obj); }
      *p = '$';
      p++;
      s = p;

      if (*p != '$') {
        auto *buf = static_cast<char *>(malloc(text_buffer_size.get(*state)));
        const char *var;

        /* variable is either $foo or ${foo} */
        if (*p == '{') {
          unsigned int brl = 1, brr = 0;

          p++;
          s = p;
          while ((*p != 0) && brl != brr) {
            if (*p == '{') { brl++; }
            if (*p == '}') { brr++; }
            p++;
          }
          p--;
        } else {
          s = p;
          if (*p == '#') { p++; }
          while ((*p != 0) && ((isalnum(static_cast<unsigned char>(*p)) != 0) ||
                               *p == '_')) {
            p++;
          }
        }

        /* copy variable to buffer */
        len = (p - s > static_cast<int>(text_buffer_size.get(*state)) - 1)
                  ? static_cast<int>(text_buffer_size.get(*state)) - 1
                  : (p - s);
        strncpy(buf, s, len);
        buf[len] = '\0';

        if (*p == '}') { p++; }
        s = p;

        /* search for variable in environment */

        var = getenv(buf);
        if (var != nullptr) {
          obj = create_plain_text(var);
          if (obj != nullptr) { append_object(retval, obj); }
          free(buf);
          continue;
        }

        /* if variable wasn't found in environment, use some special */

        arg = nullptr;

        /* split arg */
        if (strchr(buf, ' ') != nullptr) {
          arg = strchr(buf, ' ');
          *arg = '\0';
          arg++;
          while (isspace(static_cast<unsigned char>(*arg)) != 0) { arg++; }
          if (*arg == 0) { arg = nullptr; }
        }

        /* lowercase variable name */
        tmp_p = buf;
        while (*tmp_p != 0) {
          *tmp_p = tolower(static_cast<unsigned char>(*tmp_p));
          tmp_p++;
        }

        try {
          obj = construct_text_object(buf, arg, line, &ifblock_opaque, orig_p);
        } catch (obj_create_error &e) {
          free(buf);
          free(orig_p);
          throw;
        }
        if (obj != nullptr) { append_object(retval, obj); }
        free(buf);
        continue;
      }
      obj = create_plain_text("$");
      s = p + 1;
      if (obj != nullptr) { append_object(retval, obj); }

    } else if (*p == '\\' && *(p + 1) == '#') {
      strfold(p, 1);
    } else if (*p == '#') {
      char c;
      if ((remove_comment(p, &c) != 0u) && p >= orig_p && c == '\n') {
        /* if remove_comment removed a newline, we need to 'back up' with p */
        p--;
      }
    }
    p++;
  }
  obj = create_plain_text(s);
  if (obj != nullptr) { append_object(retval, obj); }

  if (ifblock_stack_empty(&ifblock_opaque) == 0) {
    NORM_ERR("one or more $endif's are missing");
  }

  free(orig_p);
  return 0;
}

void extract_object_args_to_sub(struct text_object *obj, const char *args) {
  obj->sub =
      static_cast<struct text_object *>(malloc(sizeof(struct text_object)));
  memset(obj->sub, 0, sizeof(struct text_object));
  extract_variable_text_internal(obj->sub, args);
}

/* Frees the list of text objects root points to. */
void free_text_objects(struct text_object *root) {
  struct text_object *obj;

  if ((root != nullptr) && (root->prev != nullptr)) {
    for (obj = root->prev; obj != nullptr; obj = root->prev) {
      root->prev = obj->prev;
      if (obj->callbacks.free != nullptr) { (*obj->callbacks.free)(obj); }
      free_text_objects(obj->sub);
      free_and_zero(obj->sub);
      free_and_zero(obj->special_data);
      delete obj->cb_handle;

      free(obj);
    }
  }
}

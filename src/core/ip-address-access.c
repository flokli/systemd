/***
  This file is part of systemd.

  Copyright 2016 Daniel Mack

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdio.h>
#include <stdlib.h>

#include "alloc-util.h"
#include "bpf-firewall.h"
#include "extract-word.h"
#include "hostname-util.h"
#include "ip-address-access.h"
#include "parse-util.h"
#include "string-util.h"

int config_parse_ip_address_access(
                const char *unit,
                const char *filename,
                unsigned line,
                const char *section,
                unsigned section_line,
                const char *lvalue,
                int ltype,
                const char *rvalue,
                void *data,
                void *userdata) {

        IPAddressAccessItem **list = data;
        const char *p;
        int r;

        assert(list);

        if (isempty(rvalue)) {
                *list = ip_address_access_free_all(*list);
                return 0;
        }

        p = rvalue;

        for (;;) {
                _cleanup_free_ IPAddressAccessItem *a = NULL;
                _cleanup_free_ char *word = NULL;

                r = extract_first_word(&p, &word, NULL, 0);
                if (r == 0)
                        break;
                if (r == -ENOMEM)
                        return log_oom();
                if (r < 0) {
                        log_syntax(unit, LOG_WARNING, filename, line, r, "Invalid syntax, ignoring: %s", rvalue);
                        break;
                }

                a = new0(IPAddressAccessItem, 1);
                if (!a)
                        return log_oom();

                if (streq(word, "any")) {
                        /* "any" is a shortcut for 0.0.0.0/0 and ::/0 */

                        a->family = AF_INET;
                        LIST_APPEND(items, *list, a);

                        a = new0(IPAddressAccessItem, 1);
                        if (!a)
                                return log_oom();

                        a->family = AF_INET6;

                } else if (is_localhost(word)) {
                        /* "localhost" is a shortcut for 127.0.0.0/8 and ::1/128 */

                        a->family = AF_INET;
                        a->address.in.s_addr = htobe32(0x7f000000);
                        a->prefixlen = 8;
                        LIST_APPEND(items, *list, a);

                        a = new0(IPAddressAccessItem, 1);
                        if (!a)
                                return log_oom();

                        a->family = AF_INET6;
                        a->address.in6 = (struct in6_addr) IN6ADDR_LOOPBACK_INIT;
                        a->prefixlen = 128;

                } else if (streq(word, "link-local")) {

                        /* "link-local" is a shortcut for 169.254.0.0/16 and fe80::/64 */

                        a->family = AF_INET;
                        a->address.in.s_addr = htobe32((UINT32_C(169) << 24 | UINT32_C(254) << 16));
                        a->prefixlen = 16;
                        LIST_APPEND(items, *list, a);

                        a = new0(IPAddressAccessItem, 1);
                        if (!a)
                                return log_oom();

                        a->family = AF_INET6;
                        a->address.in6 = (struct in6_addr) {
                                .__in6_u.__u6_addr32[0] = htobe32(0xfe800000)
                        };
                        a->prefixlen = 64;

                } else if (streq(word, "multicast")) {

                        /* "multicast" is a shortcut for 224.0.0.0/4 and ff00::/8 */

                        a->family = AF_INET;
                        a->address.in.s_addr = htobe32((UINT32_C(224) << 24));
                        a->prefixlen = 4;
                        LIST_APPEND(items, *list, a);

                        a = new0(IPAddressAccessItem, 1);
                        if (!a)
                                return log_oom();

                        a->family = AF_INET6;
                        a->address.in6 = (struct in6_addr) {
                                .__in6_u.__u6_addr32[0] = htobe32(0xff000000)
                        };
                        a->prefixlen = 8;

                } else {
                        r = in_addr_prefix_from_string_auto(word, &a->family, &a->address, &a->prefixlen);
                        if (r < 0) {
                                log_syntax(unit, LOG_WARNING, filename, line, r, "Address prefix is invalid, ignoring assignment: %s", word);
                                return 0;
                        }
                }

                LIST_APPEND(items, *list, a);
                a = NULL;
        }

        *list = ip_address_access_reduce(*list);

        if (*list) {
                r = bpf_firewall_supported();
                if (r < 0)
                        return r;
                if (r == 0)
                        log_warning("File %s:%u configures an IP firewall (%s=%s), but the local system does not support BPF/cgroup based firewalling.\n"
                                    "Proceeding WITHOUT firewalling in effect!", filename, line, lvalue, rvalue);
        }

        return 0;
}

IPAddressAccessItem* ip_address_access_free_all(IPAddressAccessItem *first) {
        IPAddressAccessItem *next, *p = first;

        while (p) {
                next = p->items_next;
                free(p);

                p = next;
        }

        return NULL;
}

IPAddressAccessItem* ip_address_access_reduce(IPAddressAccessItem *first) {
        IPAddressAccessItem *a, *b, *tmp;
        int r;

        /* Drops all entries from the list that are covered by another entry in full, thus removing all redundant
         * entries. */

        LIST_FOREACH_SAFE(items, a, tmp, first) {

                /* Drop irrelevant bits */
                (void) in_addr_mask(a->family, &a->address, a->prefixlen);

                LIST_FOREACH(items, b, first) {

                        if (a == b)
                                continue;

                        if (a->family != b->family)
                                continue;

                        if (b->prefixlen > a->prefixlen)
                                continue;

                        r = in_addr_prefix_covers(b->family,
                                                  &b->address,
                                                  b->prefixlen,
                                                  &a->address);
                        if (r <= 0)
                                continue;

                        /* b covers a fully, then let's drop a */

                        LIST_REMOVE(items, first, a);
                        free(a);
                }
        }

        return first;
}

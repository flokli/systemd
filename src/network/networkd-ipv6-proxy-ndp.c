/***
  This file is part of systemd.

  Copyright 2017 Florian Klink <flokli@flokli.de>

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

#include <netinet/ether.h>
#include <linux/if.h>
#include <unistd.h>

#include "fileio.h"
#include "netlink-util.h"
#include "networkd-ipv6-proxy-ndp.h"
#include "networkd-link.h"
#include "networkd-manager.h"
#include "string-util.h"

bool ipv6_proxy_ndp_is_enabled(Link *link) {
        assert(link);

        if (link->flags & IFF_LOOPBACK)
                return false;

        if (!link->network)
                return false;

        if (link->network->ipv6_proxy_ndp < 0)
                return false;

        return true;
}

int ipv6_proxy_ndp_set(Link *link) {
        const char *p = NULL;
        int r;

        assert(link);

        if (!ipv6_proxy_ndp_is_enabled(link))
                return 0;

        p = strjoina("/proc/sys/net/ipv6/conf/", link->ifname, "/proxy_ndp");

        r = write_string_file(p, one_zero(link->network->ipv6_proxy_ndp), WRITE_STRING_FILE_VERIFY_ON_FAILURE);
        if (r < 0)
                log_link_warning_errno(link, r, "Cannot configure proxy NDP for interface: %m");

        return 0;
}

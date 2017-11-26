with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ pkgconfig intltool gperf libxslt gettext docbook_xsl docbook_xml_dtd_42 docbook_xml_dtd_45
  coreutils # meson calls date, state etc.
  ninja meson
  ];
  buildInputs = [ linuxHeaders libcap kmod xz pam acl libuuid m4 glib libgcrypt libgpgerror libmicrohttpd kexectools libseccomp libffi audit lz4 libapparmor iptables gnu-efi ];
}

# $Id$
#-------------------------------------------------------------------------------
# spec file for the openttd rpm package
#
# Copyright (c) 2007-2011 The OpenTTD developers
#
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself
#
# Note: for (at least) CentOS '#' comments end '\' continue command on new line.
#       So place all '#' commented parameters of e.g. configure to the end.
#
#-------------------------------------------------------------------------------

Name:          openttd
Version:       1.1.0
Release:       1%{?dist}

Group:         Amusements/Games
License:       GPLv2
URL:           http://www.openttd.org
Summary:       OpenTTD is an Open Source clone of Chris Sawyer's Transport Tycoon Deluxe

Source:        %{name}-%{version}-source.tar.bz2

Requires:      fontconfig
Requires:      SDL
Requires:      zlib
Requires:      xz-devel
BuildRequires: gcc-c++
BuildRequires: fontconfig-devel
BuildRequires: libpng-devel
BuildRequires: libicu-devel
BuildRequires: SDL-devel
BuildRequires: zlib-devel
# vendor specific dependencies
%if %{_vendor}=="alt"
Requires:      freetype
BuildRequires: freetype-devel
%endif
%if %{_vendor}=="redhat" || %{_vendor}=="fedora"
Requires:      freetype
BuildRequires: freetype-devel
BuildRequires: desktop-file-utils
%endif
%if %{_vendor}=="suse" || %{_vendor}=="mandriva"
Requires:      freetype2
BuildRequires: freetype2-devel
%endif
%if %{_vendor}=="suse"
BuildRequires: update-desktop-files
%endif

# recommends works for suse (not sles9) and mandriva, only
%if 0%{?suse_version} > 910  || %{_vendor}=="mandriva"
Recommends:	opengfx
# for 0.8.0
#Recommends:	opensfx
%endif

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or install the recommend package OpenGFX.

OpenTTD is licensed under the GNU General Public License version 2.0. For more
information, see the file 'COPYING' included with every release and source
download of the game.

%prep
%setup -q

%build
# suse sle <10 has no support for makedepend
%if 0%{?sles_version} == 9 || 0%{?sles_version} == 10
%define 	do_makedepend	0
%else
%define 	do_makedepend	1
%endif
./configure \
	--prefix-dir="%{_prefix}" \
	--binary-name="%{name}" \
	--enable-strip \
	--binary-dir="bin" \
	--data-dir="share/%{name}" \
	--with-makedepend="%{do_makedepend}" \
#	--revision="%{ver}%{?prever:-%{prever}}" \
#	--enable-debug=0 \
#	--with-sdl \
#	--with-zlib \
#	--with-png \
#	--with-freetype \
#	--with-fontconfig \
#	--with-icu \
#	--menu_group="Game;" \
#	--menu-name="OpenTTD" \
#	--doc-dir="share\doc\%{name}" \
#	--icon-dir="share/pixmaps" \
#	--icon-theme-dir="share/icons/hicolor" \
#	--man-dir="share/man/man6" \
#	--menu-dir="share/applications"

make %{?_smp_mflags}

%install
make install INSTALL_DIR="%{buildroot}"

# Validate menu entrys (vendor specific)
%if %{_vendor} == "redhat" || %{_vendor}=="fedora"
desktop-file-install \
	--vendor="%{_vendor}" \
	--remove-key Version \
	--dir="%{buildroot}/%{_datadir}/applications/" \
	"%{buildroot}/%{_datadir}/applications/%{name}.desktop" \
#	--delete-original
%endif
%if %{_vendor}=="suse"
%__cat > %{name}.desktop << EOF
[Desktop Entry]
Encoding=UTF-8
Name=OpenTTD
Comment=OpenTTD - A clone of the Microprose game 'Transport Tycoon Deluxe'
GenericName=OpenTTD
Type=Application
Terminal=false
Exec=%{name}
Icon=%{name}
Categories=Game;StrategyGame;
EOF
%suse_update_desktop_file -i %{name} Game StrategyGame
%endif

%clean
#rm -rf "%{buildroot}"

%post
# Update the icon cache (vendor specific)
%if %{_vendor}=="mandriva"
%update_icon_cache hicolor
%endif

%if %{_vendor} == "redhat" || %{_vendor}=="fedora"
touch --no-create %{_datadir}/icons/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
	%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi
%endif

%postun
# Update the icon cache (vendor specific)
%if %{_vendor}=="mandriva"
%update_icon_cache hicolor
%endif

%if %{_vendor} == "redhat" || %{_vendor}=="fedora"
touch --no-create %{_datadir}/icons/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
	%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor || :
fi
%endif

%files
%defattr(-, root, games, -)
%dir %{_datadir}/doc/%{name}
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/lang
%dir %{_datadir}/%{name}/data
%dir %{_datadir}/%{name}/gm
%dir %{_datadir}/%{name}/scripts
%attr(755, root, games) %{_bindir}/%{name}
%{_datadir}/doc/%{name}/*
%{_datadir}/%{name}/lang/*
%{_datadir}/%{name}/data/*
%{_datadir}/%{name}/scripts/*
%{_datadir}/applications/*%{name}.desktop
%{_datadir}/pixmaps/*
%{_datadir}/icons/*
%doc %{_mandir}/man6/%{name}.6.*

%changelog
* Sat Sep 26 2009 Marcel Gmür <ammler@openttdcoop.org> - 0.7.2
- no subfolder games for datadir
- cleanup: no post and postun anymore
- Recommends: opengfx (for suse and mandriva)
- add SUSE support

* Mon Oct 20 2008 Benedikt Brüggemeier <skidd13@openttd.org>
- Added libicu dependency

* Thu Sep 23 2008 Benedikt Brüggemeier <skidd13@openttd.org>
- Merged both versions of the spec file

* Fri Aug 29 2008 Jonathan Coome <maedhros@openttd.org>
- Rewrite spec file from scratch.

* Sat Aug 02 2008 Benedikt Brüggemeier <skidd13@openttd.org>
- Updated spec file

* Thu Mar 27 2008 Denis Burlaka <burlaka@yandex.ru>
- Universal spec file

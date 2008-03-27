#
# spec file for package openttd (trunk)
#
# Copyright (c) 2007 The OpenTTD team.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself
#
Name:          openttd
Version:       svn
Release:       head
Group:         Applications/Games
Source:        %{name}-%{version}-%{release}.tar.gz
License:       GPL
URL:           http://www.openttd.org
Packager:      Denis Burlaka <burlaka@yandex.ru>
Summary:       OpenTTD is an Open Source clone of Chris Sawyer's Transport Tycoon Deluxe
Requires:      SDL zlib libpng freetype2 fontconfig
BuildRequires: gcc SDL-devel zlib-devel libpng-devel fontconfig-devel
%if %{_vendor}=="suse"
BuildRequires: freetype2-devel
%endif
%if %{_vendor}=="fedora"
BuildRequires: freetype-devel
%endif
%if %{_vendor}=="mandriva"
BuildRequires: libfreetype6-devel
%endif
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix:        /usr

%description
OpenTTD is a clone of the Microprose game "Transport Tycoon Deluxe", a popular game originally written by Chris Sawyer. It attempts to mimic the original game as closely as possible while extending it with new features.

OpenTTD is licensed under the GNU General Public License version 2.0. For more information, see the file 'COPYING' included with every release and source download of the game.

%prep
%setup

%build
./configure --prefix-dir=%{prefix} --binary-dir=bin --install-dir="$RPM_BUILD_ROOT"
make

%install
make ROOT="$RPM_BUILD_ROOT" install

mkdir -p $RPM_BUILD_ROOT/%{_datadir}/applications
cat << EOF > $RPM_BUILD_ROOT/%{_datadir}/applications/%{name}.desktop
[Desktop Entry]
Categories=Games;
Encoding=UTF-8
Exec=/usr/bin/openttd
Name=OpenTTD
Icon=openttd.32
Terminal=false
Type=Application
EOF

%clean
rm -Rf "$RPM_BUILD_ROOT"

%files
%dir %{_datadir}/games/%{name}
%dir %{_datadir}/games/%{name}/lang
%dir %{_datadir}/games/%{name}/data
%dir %{_datadir}/games/%{name}/gm
%dir %{_datadir}/games/%{name}/docs
%dir %{_datadir}/pixmaps
%defattr(644, root, games, 755)
%attr(755, root, games) %{_bindir}/%{name}
%{_datadir}/games/%{name}/lang/*
%{_datadir}/games/%{name}/data/*
%{_datadir}/games/%{name}/docs/*
%{_datadir}/pixmaps/*
%{_datadir}/applications/%{name}.desktop


#
# spec file for package openttd
#
# Copyright (c) 2012 SUSE LINUX Products GmbH, Nuernberg, Germany.
# Copyright (c) 2007-2017 The OpenTTD developers
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

Name:           openttd
Version:        1.8.beta1
Release:        0
%define srcver  1.8.0-beta1
Summary:        An open source reimplementation of Chris Sawyer's Transport Tycoon Deluxe
License:        GPL-2.0
Group:          Amusements/Games/Strategy/Other
Url:            http://www.openttd.org

Source:         http://binaries.openttd.org/releases/%{srcver}/%{name}-%{srcver}-source.tar.gz

%if 0%{?suse_version} || 0%{?mdkversion}
Recommends:     %{name}-gui
%endif

BuildRequires:  gcc-c++
BuildRequires:  libpng-devel
BuildRequires:  zlib-devel

%if 0%{?suse_version} || 0%{?mdkversion}
BuildRequires:  update-alternatives
Requires:       update-alternatives
%else
BuildRequires:  chkconfig
Requires:       chkconfig
%endif

%if 0%{?mdkversion}
BuildRequires:  liblzma-devel
BuildRequires:  liblzo-devel
%else
BuildRequires:  lzo-devel
BuildRequires:  xz-devel
%endif

# OBS workaround: needed by libdrm
%if 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
BuildRequires:  kernel
%endif

# for lzma detection
%if 0%{?suse_version}
BuildRequires:  pkg-config
%endif

# bulding openttd.grf is not required as it is a) part of source and
# b) required only, if you want to use the original set
%if 0%{?with_grfcodec}
BuildRequires:  grfcodec
%endif
# Recommends would fit better but not well supported...
Requires:       openttd-opengfx >= 0.4.2

Obsoletes:      %{name}-data < %{version}
Provides:       %{name}-data = %{version}

BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or install the recommend subackages OpenGFX for
free graphics, OpenSFX for free sounds and OpenMSX for free music.

OpenTTD is licensed under the GNU General Public License version 2.0. For more
information, see the file 'COPYING' included with every release and source
download of the game.

%package gui
Summary:        OpenTTD GUI/Client (requires SDL)
Group:          Amusements/Games/Strategy/Other

Requires:       %{name}
Conflicts:      %{name}-dedicated

BuildRequires:  SDL-devel
BuildRequires:  fontconfig-devel

%if 0%{?rhel_version} != 600
BuildRequires:  libicu-devel
%endif
%if 0%{?rhel_version} || 0%{?fedora}
BuildRequires:  freetype-devel
%endif
%if 0%{?suse_version} || 0%{?mdkversion}
BuildRequires:  freetype2-devel
%endif
%if 0%{?suse_version}
BuildRequires:  update-desktop-files
%else
BuildRequires:  desktop-file-utils
Requires:       hicolor-icon-theme
%endif

%if 0%{?suse_version} || 0%{?mdkversion}
Recommends:     openttd-openmsx
Recommends:     openttd-opensfx
%endif

%description gui
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or install the recommend subackages OpenGFX for
free graphics, OpenSFX for free sounds and OpenMSX for free music.

This subpackage provides the binary which needs SDL.

%package dedicated
Summary:        OpenTTD Dedicated Server binary (without SDL)
Group:          Amusements/Games/Strategy/Other

Requires:       %{name}
Conflicts:      %{name}-gui

%description dedicated
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or the required package OpenGFX and OpenSFX.

This subpackage provides the binary without dependency of SDL.

%prep
%setup -qn openttd%{?branch:-%{branch}}-%{srcver}

# we build the grfs from sources but validate the result with the existing data
%if 0%{?with_grfcodec}
md5sum bin/data/* > validate.data
%endif

%build
# first, we build the dedicated binary and copy it to dedicated/
./configure \
        --prefix-dir="%{_prefix}" \
        --binary-dir="bin" \
        --data-dir="share/%{name}" \
        --enable-dedicated
make %{?_smp_mflags} BUNDLE_DIR="dedicated" bundle

# then, we build the common gui version which we install the usual way
./configure \
        --prefix-dir="%{_prefix}" \
        --binary-name="%{name}" \
        --binary-dir="bin" \
        --data-dir="share/%{name}" \
        --doc-dir="share/doc/%{name}" \
        --menu-name="OpenTTD%{?branch: %{branch}}" \
        --menu-group="Game;StrategyGame;"

make %{?_smp_mflags}

%install
# install the dedicated binary
install -D -m0755 dedicated/openttd %{buildroot}%{_bindir}/%{name}-dedicated
# install the gui binary and rename to openttd-gui
make install INSTALL_DIR=%{buildroot}
mv %{buildroot}%{_bindir}/%{name} %{buildroot}%{_bindir}/%{name}-gui
# we need a dummy target for /etc/alternatives/openttd
mkdir -p %{buildroot}%{_sysconfdir}/alternatives
touch %{buildroot}%{_sysconfdir}/alternatives/%{name}
ln -s -f /etc/alternatives/%{name} %{buildroot}%{_bindir}/%{name}

%if 0%{?suse_version}
%suse_update_desktop_file -r %{name} Game StrategyGame
%else
%if 0%{?fedora} || 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600
desktop-file-install --dir=%{buildroot}%{_datadir}/applications \
        --add-category=StrategyGame \
        media/openttd.desktop
%endif
%endif

%if 0%{?with_grfcodec}
%check
md5sum -c validate.data
%endif

%post gui
/usr/sbin/update-alternatives --install %{_bindir}/%{name} %{name} %{_bindir}/%{name}-gui 10
touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%post dedicated
/usr/sbin/update-alternatives --install %{_bindir}/%{name} %{name} %{_bindir}/%{name}-dedicated 0

%preun gui
if [ "$1" = 0 ] ; then
    /usr/sbin/update-alternatives --remove %{name} %{_bindir}/%{name}-gui
fi

%preun dedicated
if [ "$1" = 0 ] ; then
    /usr/sbin/update-alternatives --remove %{name} %{_bindir}/%{name}-dedicated
fi

%postun gui
if [ "$1" -eq 0 ] ; then
    touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans gui
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :

# we need a file in the main package so it will be made
%files
%defattr(-, root, root)
%dir %{_datadir}/doc/%{name}
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/lang
%dir %{_datadir}/%{name}/baseset
%dir %{_datadir}/%{name}/scripts
%dir %{_datadir}/%{name}/ai
%dir %{_datadir}/%{name}/game
%{_datadir}/doc/%{name}/*
%{_datadir}/%{name}/lang/*
%{_datadir}/%{name}/baseset/*
%{_datadir}/%{name}/scripts/*
%{_datadir}/%{name}/ai/*
%{_datadir}/%{name}/game/*
%doc %{_mandir}/man6/%{name}.6.*

%files gui
%defattr(-, root, root)
%ghost %{_sysconfdir}/alternatives/%{name}
%ghost %{_bindir}/%{name}
%{_bindir}/%{name}-gui
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/16x16
%dir %{_datadir}/icons/hicolor/16x16/apps
%dir %{_datadir}/icons/hicolor/32x32
%dir %{_datadir}/icons/hicolor/32x32/apps
%dir %{_datadir}/icons/hicolor/48x48
%dir %{_datadir}/icons/hicolor/48x48/apps
%dir %{_datadir}/icons/hicolor/64x64
%dir %{_datadir}/icons/hicolor/64x64/apps
%dir %{_datadir}/icons/hicolor/128x128
%dir %{_datadir}/icons/hicolor/128x128/apps
%dir %{_datadir}/icons/hicolor/256x256
%dir %{_datadir}/icons/hicolor/256x256/apps
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/16x16/apps/%{name}.png
%{_datadir}/icons/hicolor/32x32/apps/%{name}.png
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png
%{_datadir}/pixmaps/%{name}.32.xpm

%files dedicated
%defattr(-, root, root)
%ghost %{_bindir}/%{name}
%ghost %{_sysconfdir}/alternatives/%{name}
%{_bindir}/%{name}-dedicated

%changelog

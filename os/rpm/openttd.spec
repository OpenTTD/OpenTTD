%define dedicated       0

%define binname         openttd

%define srcver          1.1.3

%if %{dedicated}
Name:           %{binname}-dedicated
%else
Name:           %{binname}
%endif
Version:        %{srcver}
Release:        1%{?dist}
Group:          Amusements/Games/Strategy/Other
License:        GPLv2
URL:            http://www.openttd.org
Summary:        An open source clone of Chris Sawyer's Transport Tycoon Deluxe

Source:         openttd%{?branch:-%{branch}}-%{srcver}-source.tar.bz2

# the main package works with the exact same data package version only
Requires:       %{binname}-data = %{version}

BuildRequires:  gcc-c++
BuildRequires:  libpng-devel
BuildRequires:  zlib-devel

%if 0%{?mdkversion}
BuildRequires:  liblzo-devel
BuildRequires:  liblzma-devel
%else
BuildRequires:  lzo-devel
BuildRequires:  xz-devel
%endif

#needed by libdrm
%if 0%{?rhel_version} >= 600
BuildRequires:  kernel
%endif

# for lzma detection
%if 0%{?suse_version}
BuildRequires:  pkg-config
%endif

# Desktop specific tags, not needed for dedicated
%if !%{dedicated}
BuildRequires:  fontconfig-devel
BuildRequires:  SDL-devel

BuildRequires:  grfcodec

# vendor specific dependencies
 %if !0%{?rhel_version}
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
 %endif
%endif

%if %{dedicated}
Conflicts:      %{binname} %{binname}-gui
%else
Provides:       %{binname}-gui
Conflicts:      %{binname}-dedicated
Requires:       openttd-opensfx
# recommends works for suse (not sles9) and mandriva, only
 %if 0%{?suse_version} || 0%{?mdkversion}
# require timidity is part of openmsx
Recommends:     openttd-openmsx
 %endif
%endif
# Recommends would fit better but not well supported...
Requires:       openttd-opengfx >= 0.3.2

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or install the recommend subackages OpenGFX for
free graphics, OpenSFX for free sounds and OpenMSX for free music.

OpenTTD is licensed under the GNU General Public License version 2.0. For more
information, see the file 'COPYING' included with every release and source
download of the game.

# the subpackage data needs only to build once, the dedicated version
# can reuse the data package of the gui package
%if !%{dedicated}
%package data
Summary:        Data package for OpenTTD
Group:          Amusements/Games/Strategy/Other
 %if 0%{?suse_version} >= 1120 || 0%{?fedora} || 0%{?mdkversion}
BuildArch:      noarch
 %endif
BuildRequires:  grfcodec

%description data
OpenTTD is a reimplementation of the Microprose game "Transport Tycoon Deluxe"
with lots of new features and enhancements. To play the game you need either
the original data from the game or the required package OpenGFX and OpenSFX.

This package is required by openttd gui and openttd dedicated package. This
way it is possible to install a openttd version without SDL requirement.

%endif

%prep
%setup -qn openttd%{?branch:-%{branch}}-%{srcver}

# we build the grfs from sources but validate the result with the existing data
md5sum bin/data/* > validate.data

%build
./configure \
        --prefix-dir="%{_prefix}" \
        --binary-name="%{binname}" \
        --binary-dir="bin" \
        --data-dir="share/%{binname}" \
        --doc-dir="share/doc/%{binname}" \
        --menu-name="OpenTTD%{?branch: %{branch}}" \
        --menu-group="Game;StrategyGame;" \
        --enable-dedicated="%{dedicated}" \

make %{?_smp_mflags}

%install
%if %{dedicated}
# dedicated package needs binary only
install -D -m0755 bin/openttd %{buildroot}/%{_bindir}/%{binname}
%else
make install INSTALL_DIR="%{buildroot}"
 %if 0%{?suse_version}
%suse_update_desktop_file -r %{binname} Game StrategyGame
 %endif
%endif

%clean
rm -rf "%{buildroot}"

%check
md5sum -c validate.data

%files
%attr(755, root, root) %{_bindir}/%{binname}

# all other files are for the gui version only, also no
# subpackage needed for the dedicated version
%if !%{dedicated}
%defattr(-, root, root)
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
%{_datadir}/applications/%{binname}.desktop
%{_datadir}/icons/hicolor/16x16/apps/%{binname}.png
%{_datadir}/icons/hicolor/32x32/apps/%{binname}.png
%{_datadir}/icons/hicolor/48x48/apps/%{binname}.png
%{_datadir}/icons/hicolor/64x64/apps/%{binname}.png
%{_datadir}/icons/hicolor/128x128/apps/%{binname}.png
%{_datadir}/icons/hicolor/256x256/apps/%{binname}.png
%{_datadir}/pixmaps/%{binname}.32.xpm

%files data
%defattr(-, root, root)
%dir %{_datadir}/doc/%{binname}
%dir %{_datadir}/%{binname}
%dir %{_datadir}/%{binname}/lang
%dir %{_datadir}/%{binname}/data
%dir %{_datadir}/%{binname}/gm
%dir %{_datadir}/%{binname}/scripts
%dir %{_datadir}/%{binname}/ai
%{_datadir}/doc/%{binname}/*
%{_datadir}/%{binname}/lang/*
%{_datadir}/%{binname}/data/*
%{_datadir}/%{binname}/scripts/*
%{_datadir}/%{binname}/ai/*
%{_datadir}/%{binname}/gm/*
%doc %{_mandir}/man6/%{binname}.6.*
%endif

%changelog

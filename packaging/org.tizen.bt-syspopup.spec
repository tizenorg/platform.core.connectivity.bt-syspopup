%define _usrdir /usr
%define _appdir %{_usrdir}/apps

%bcond_with wayland

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version:    0.3.2
Release:    0
Group:      main
License:    Flora-1.1
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(evas)
%if "%{?profile}" == "wearable"
BuildRequires:  pkgconfig(syspopup-caller)
%else
BuildRequires:  pkgconfig(efl-assist)
BuildRequires:  pkgconfig(notification)
%endif
BuildRequires: pkgconfig(efl-extension)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  edje-tools
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(capi-system-device)
BuildRequires:  pkgconfig(capi-media-player)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(vconf)

BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q


%build
%if "%{?profile}" == "wearable"
export CFLAGS="$CFLAGS -DTIZEN_ENGINEER_MODE -DTIZEN_WEARABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ENGINEER_MODE"
export FFLAGS="$FFLAGS -DTIZEN_ENGINEER_MODE"
%else
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE -DTIZEN_MOBILE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

cmake . \
    -DCMAKE_INSTALL_PREFIX=%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup \
%if %{with wayland}
    -DWAYLAND_SUPPORT=On \
%else
    -DWAYLAND_SUPPORT=Off \
%endif
    #eol

make %{?jobs:-j%jobs}

%cmake . \
	-DTZ_SYS_RO_APP=%{TZ_SYS_RO_APP} \
	-DTZ_SYS_SHARE=%{TZ_SYS_SHARE}
make

%install
rm -rf %{buildroot}
%make_install


%files
%manifest org.tizen.bt-syspopup.manifest
%defattr(-,root,root,-)
%{TZ_SYS_RO_PACKAGES}/org.tizen.bt-syspopup.xml
%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup/bin/bt-syspopup
%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup/res/edje/*.edj
%{TZ_SYS_SHARE}/icons/default/small/org.tizen.bt-syspopup.png
%if "%{?profile}" == "wearable"
%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_ChangeableColorTable.xml
%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_FontInfoTable.xml
%{TZ_SYS_RO_APP}/org.tizen.bt-syspopup/res/images/*
%endif

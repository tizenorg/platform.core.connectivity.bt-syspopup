%bcond_with wayland

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version:    0.2.56
Release:    0
Group:      main
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(devman_haptic)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(bluetooth-api)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  sysman-internal-devel
BuildRequires:  edje-tools
BuildRequires:  pkgconfig(libtzplatform-config)

BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q


%build
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

cmake . \
    -DCMAKE_INSTALL_PREFIX=%{TZ_SYS_RW_APP}/org.tizen.bt-syspopup \
%if %{with wayland}
    -DWAYLAND_SUPPORT=On \
%else
    -DWAYLAND_SUPPORT=Off \
%endif
    #eol

make %{?jobs:-j%jobs}

%cmake . \
	-DTZ_SYS_RW_APP=%{TZ_SYS_RW_APP} \
	-DTZ_SYS_SHARE=%{TZ_SYS_SHARE}
make

%install
rm -rf %{buildroot}
%make_install


%files
%manifest org.tizen.bt-syspopup.manifest
%defattr(-,root,root,-)
%{TZ_SYS_RO_PACKAGES}/org.tizen.bt-syspopup.xml
%{TZ_SYS_RW_APP}/org.tizen.bt-syspopup/bin/bt-syspopup
%{TZ_SYS_RW_APP}/org.tizen.bt-syspopup/res/edje/*.edj
%{TZ_SYS_SHARE}/icons/default/small/org.tizen.bt-syspopup.png

%bcond_with wayland

%define _optdir /opt
%define _usrdir /usr
%define _appdir %{_optdir}/apps

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version:    0.2.56
Release:    0
Group:      main
License:    Flora Software License
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
    -DCMAKE_INSTALL_PREFIX=%{_appdir}/org.tizen.bt-syspopup \
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
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_appdir}/org.tizen.bt-syspopup/res/edje/*.edj
%{_optdir}/share/icons/default/small/org.tizen.bt-syspopup.png

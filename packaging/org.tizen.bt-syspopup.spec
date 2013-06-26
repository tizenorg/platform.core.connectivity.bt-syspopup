%define _optdir /opt
%define _usrdir /usr
%define _appdir %{_optdir}/apps

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version: 0.2.52
Release:    1
Group:      main
License:    Flora Software License
Source0:    %{name}-%{version}.tar.gz
Source1001: 	org.tizen.bt-syspopup.manifest
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(devman_haptic)
BuildRequires:  pkgconfig(aul)
BuildRequires:  sysman-internal-devel

BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q
cp %{SOURCE1001} .


%build

cmake . -DCMAKE_INSTALL_PREFIX=%{_appdir}/org.tizen.bt-syspopup
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_optdir}/share/icons/default/small/org.tizen.bt-syspopup.png
%{_optdir}/share/process-info/bt-syspopup.ini

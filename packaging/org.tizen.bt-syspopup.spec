Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version:    0.2.37
Release:    2
Group:      TO_BE_FILLED
License:    Flora Software License
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/org.tizen.bt-syspopup.manifest 
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(devman_haptic)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)

BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q


%build
cp %{SOURCE1001} .

cmake . -DCMAKE_INSTALL_PREFIX=/usr
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%files
%manifest org.tizen.bt-syspopup.manifest
%defattr(-,root,root,-)
/opt/share/applications/org.tizen.bt-syspopup.desktop
/usr/bin/bt-syspopup
/usr/share/icon/01_header_icon_cancel.png
/usr/share/icon/01_header_icon_done.png
/usr/share/process-info/bt-syspopup.ini


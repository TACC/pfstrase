Summary: Parallel Filesystem Monitoring Agent
Name: pfstrased
Version: 0.0.1
Release: 1%{?dist}
License: GPL
Vendor: Texas Advanced Computing Center
Group: System Environment/Base
Packager: TACC - rtevans@tacc.utexas.edu
Source: pfstrased-%{version}.tar.gz
BuildRequires: systemd

%{?systemd_requires}

%description
This package provides the pfstrase daemon, \
along with a systemd script to provide control.

%prep
%setup

%build
./configure
make
sed -i 's/CONFIGFILE/\%{_sysconfdir}\/pfstrase\/pfstrase.conf/' pfstrase.service

%install
mkdir -p  %{buildroot}/%{_sbindir}/
mkdir -p  %{buildroot}/%{_sysconfdir}/pfstrase/
mkdir -p  %{buildroot}/%{_unitdir}/
install -m 744 pfstrased %{buildroot}/%{_sbindir}/pfstrased
install -m 644 pfstrase.conf %{buildroot}/%{_sysconfdir}/pfstrase/pfstrase.conf
install -m 644 pfstrase.service %{buildroot}/%{_unitdir}/pfstrase.service

%files
%{_sbindir}/pfstrased
%{_sysconfdir}/pfstrase/pfstrase.conf
%{_unitdir}/pfstrase.service

%post
%systemd_post pfstrase.service

%preun
%systemd_preun pfstrase.service

%postun
%systemd_postun_with_restart pfstrase.service

Summary: Parallel Filesystem Monitoring Agent Server
Name: pfstrase_server
Version: 0.0.1
Release: 1%{?dist}
License: GPL
Vendor: Texas Advanced Computing Center
Group: System Environment/Base
Packager: TACC - rtevans@tacc.utexas.edu
Source: pfstrase_server-%{version}.tar.gz
BuildRequires: systemd

%{?systemd_requires}

%description
This package provides the pfstrase server, \
along with a systemd script to provide control.

%prep
%setup

%build
./configure
make
sed -i 's/CONFIGFILE/\%{_sysconfdir}\/pfstrase\/pfstrase_server.conf/' pfstrase_server.service

%install
mkdir -p  %{buildroot}/%{_sbindir}/
mkdir -p  %{buildroot}/%{_sysconfdir}/pfstrase/
mkdir -p  %{buildroot}/%{_unitdir}/
mkdir -p  %{buildroot}/%{_mandir}/man1/
install -m 744 pfstrase_server %{buildroot}/%{_sbindir}/pfstrase_server
install -m 744 pfstop %{buildroot}/%{_sbindir}/pfstop
install -m 744 map_nids.py %{buildroot}/%{_sbindir}/map_nids.py
install -m 744 qhost.py %{buildroot}/%{_sbindir}/map_nids.py
install -m 644 pfstrase_server.conf %{buildroot}/%{_sysconfdir}/pfstrase/pfstrase_server.conf
install -m 644 pfstrase_server.service %{buildroot}/%{_unitdir}/pfstrase_server.service
install -m 644 man/pfstop.1.gz %{buildroot}/%{_mandir}/man1/pfstop.1.gz
install -m 644 man/pfstrase_server.1.gz %{buildroot}/%{_mandir}/man1/pfstrase_server.1.gz

%files
%{_sbindir}/pfstrase_server
%{_sbindir}/pfstop
%{_sbindir}/map_nids.py
%{_sbindir}/qhost.py
%{_sysconfdir}/pfstrase/pfstrase_server.conf
%{_unitdir}/pfstrase_server.service
%{_mandir}/man1/pfstop.1.gz
%{_mandir}/man1/pfstrase_server.1.gz

%post
%systemd_post pfstrase_server.service

%preun
%systemd_preun pfstrase_server.service

%postun
%systemd_postun_with_restart pfstrase_server.service

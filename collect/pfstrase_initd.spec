Summary: Parallel Filesystem Monitoring Agent
Name: pfstrased_initd
Version: 0.0.1
Release: 1%{?dist}
License: GPL
Vendor: Texas Advanced Computing Center
Group: System Environment/Base
Packager: TACC - rtevans@tacc.utexas.edu
Source: pfstrased-%{version}.tar.gz

%description
This package provides the pfstrase daemon, \
along with an init.d script to provide control.

%prep
%setup

%build
./configure
make

%install
mkdir -p  %{buildroot}/%{_sbindir}/
mkdir -p  %{buildroot}/%{_sysconfdir}/pfstrase/
mkdir -p  %{buildroot}/%{_sysconfdir}/rc.d/init.d/
install -m 744 pfstrased %{buildroot}/%{_sbindir}/pfstrased
install -m 644 pfstrase.conf %{buildroot}/%{_sysconfdir}/pfstrase/pfstrase.conf
install -m 644 pfstrase_initd.sh %{buildroot}/%{_sysconfdir}/rc.d/init.d/pfstrase

%files
%{_sbindir}/pfstrased
%{_sysconfdir}/pfstrase/pfstrase.conf
%{_sysconfdir}/rc.d/init.d/pfstrase.service

%post
service pfstrase start

%preun
service pfstrase stop


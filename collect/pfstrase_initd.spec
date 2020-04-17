Summary: Parallel Filesystem Monitoring Agent
Name: pfstrased
Version: 0.0.1
Release: 1%{?dist}
License: GPL
Vendor: Texas Advanced Computing Center
Group: System Environment/Base
Packager: TACC - rtevans@tacc.utexas.edu
Source: pfstrased-%{version}.tar.gz

%define debug_package %{nil}

%description
This package provides the pfstrase collection daemon, \
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
install -m 744 pfstrase_client %{buildroot}/%{_sbindir}/pfstrase_client
install -m 644 pfstrase.conf %{buildroot}/%{_sysconfdir}/pfstrase/pfstrase.conf
install -m 744 pfstrase_initd.sh %{buildroot}/%{_sysconfdir}/rc.d/init.d/pfstrase

%files
%attr(0755,root,root) %{_sbindir}/pfstrase_client
%attr(0644,root,root) %{_sysconfdir}/pfstrase/pfstrase.conf
%attr(0744,root,root) %{_sysconfdir}/rc.d/init.d/pfstrase

%post
service pfstrase start

%preun
service pfstrase stop


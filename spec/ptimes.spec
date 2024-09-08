Name: ptimes
Version: 1.2
Release: 2%{?dist}
License: GPLv2+
Group: Shells
Summary: Muslim prayer times alert program.
Url: https://github.com/ilnli/Prayer-Times
Source0: %{name}-%{version}.tar.gz
Requires: alsa-utils
%global debug_package %{nil}

%description
The program "ptimes" is a linux daemon which waits for the next prayer 
and sounds Azan when it's time for it.

%prep
%setup -q

%build
g++ -o ptimes ptimes.cpp prayertimes.hpp cmdline.c

%install
mkdir -p $RPM_BUILD_ROOT/usr/bin/
mkdir -p $RPM_BUILD_ROOT/etc/systemd/system/
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig/
mkdir -p $RPM_BUILD_ROOT/usr/share/sounds/ptimes/

install -m 755 ptimes $RPM_BUILD_ROOT/usr/bin/
install -m 755 systemd/system/ptimes.service $RPM_BUILD_ROOT/etc/systemd/system/
install -m 644 sysconfig/ptimes $RPM_BUILD_ROOT/etc/sysconfig/
install -m 644 audio/azan.wav $RPM_BUILD_ROOT/usr/share/sounds/ptimes/

%post
systemctl daemon-reload
systemctl enable ptimes.service

%files
%doc README
%{_bindir}/%{name}
/etc/systemd/system/ptimes.service
/etc/sysconfig/ptimes
/usr/share/sounds/ptimes/azan.wav

%changelog
* Sun Sep 8 2024 ilnli 1.2%{dist}
- Switched to systemd
* Thu Apr 23 2015 ilnli 1.1%{?dist}
- Fixed bug in custom argument options.
* Sat Apr 12 2014 ilnli 1.0%{?dist}
- Created RPM

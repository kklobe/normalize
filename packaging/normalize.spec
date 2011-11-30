Summary: A tool for adjusting the volume of audio files to a standard level.
%define name	normalize
%define version	0.7.7
%define release	1

%ifos Linux
    %define _prefix /usr
%else
    %define _prefix /usr/local
%endif
%define uown root
%define gown root

%define installexec %{_bindir}/install

Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

Name:    %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
%if %(rpm -q redhat-release > /dev/null 2>&1 && echo 1 || echo 0)
%define	mygroup Applications/Sound
%endif
%if %(rpm -q mandrake-release > /dev/null 2>&1 && echo 1 || echo 0)
%define	mygroup Applications/Multimedia
%else
%define	mygroup Applications/Multimedia
%endif
Group: %{mygroup}
Source: http://savannah.nongnu.org/download/%{name}/%{name}-%{version}.tar.bz2

%description
normalize is a tool for adjusting the volume of audio files to a
standard level.  This is useful for things like creating mixed CD's
and mp3 collections, where different recording levels on different
albums can cause the volume to vary greatly from song to song.

%package        xmms
Summary:        Normalize - XMMS plugin to apply volume adjustments
Group:		%{mygroup}
Requires:       %{name} = %{version}, gtk+ >= 1.2.2, xmms >= 1.0.0
BuildPrereq:    gtk+-devel >= 1.2.2, xmms-devel

%description    xmms
Plugin for XMMS to honor volume adjustments (RVA2 frames) in ID3 tags

%prep
%setup -q

%build
./configure \
    --disable-helper-search \
    --enable-xmms \
    --libdir=%{_libdir} \
    --mandir=%{_mandir} \
    --prefix=%{_prefix} \
    --with-mad \
    --without-audiofile

make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
[ "%{buildroot}" != "/" ] && [ -d %{buildroot} ] && rm -rf %{buildroot};
mkdir -p $RPM_BUILD_ROOT
strip -x xmms-rva/.libs/librva.so
make install-strip DESTDIR=$RPM_BUILD_ROOT

find %{buildroot} \! -type d -print \
    | sed "s@^%{buildroot}@@g" \
    > %{name}-%{version}-filelist

%clean
[ "%{buildroot}" != "/" ] && [ -d %{buildroot} ] && rm -rf %{buildroot};

%files
%defattr(-,%{uown},%{gown},-)
%doc README COPYING
%{_bindir}/normalize
%{_bindir}/normalize-mp3
%{_bindir}/normalize-ogg
%{_mandir}/man1/normalize.1*
%{_mandir}/man1/normalize-mp3.1*
%{_datadir}/locale/en_GB/LC_MESSAGES/normalize.mo
%{_datadir}/locale/fr/LC_MESSAGES/normalize.mo

%files xmms
%defattr(-,%{uown},%{gown},-)
/usr/lib/xmms/Effect/librva.la
/usr/lib/xmms/Effect/librva.so

%changelog
* Mon Aug 26 2002 Chris Vaill
- Upgrade to 0.7.5
- Take out stuff specific to Kevin's setup :-)
* Tue Jul 16 2002 Kevin E Cosgrove <kevinc@dOink.COM>
- Upgrade to 0.7.4.
- Mandrake 8.0 build.

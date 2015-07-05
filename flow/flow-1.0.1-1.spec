Name:		flow
Version:	1.0.1
Release:	1%{?dist}
Summary:	Flow meter for command line pipes

Group:		Applications/File
License:	GPL
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


%description

Flow provides a flowmeter for command-line pipes. It can display
bargraphs and counts, and can estimate completion time if the length
of the stream is known.

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc

/usr/bin/flow
/usr/share/man/man1/flow.1.gz


%changelog

* Thu May 07 2009 Robert Billing <freeware@tnglwood.demon.co.uk> 1.0.1-1
- Initial build.

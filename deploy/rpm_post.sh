find $RPM_INSTALL_PREFIX/madlib/bin -type d -exec cp -RPf {} $RPM_INSTALL_PREFIX/madlib/old_bin \; 2>/dev/null
find $RPM_INSTALL_PREFIX/madlib/bin -depth -type d -exec rm -r {} \; 2>/dev/null

find $RPM_INSTALL_PREFIX/madlib/doc -type d -exec cp -RPf {} $RPM_INSTALL_PREFIX/madlib/old_doc \; 2>/dev/null
find $RPM_INSTALL_PREFIX/madlib/doc -depth -type d -exec rm -r {} \; 2>/dev/null

ln -nsf $RPM_INSTALL_PREFIX/madlib/Versions/%{_madlib_version} $RPM_INSTALL_PREFIX/madlib/Current
ln -nsf $RPM_INSTALL_PREFIX/madlib/Current/bin $RPM_INSTALL_PREFIX/madlib/bin
ln -nsf $RPM_INSTALL_PREFIX/madlib/Current/doc $RPM_INSTALL_PREFIX/madlib/doc

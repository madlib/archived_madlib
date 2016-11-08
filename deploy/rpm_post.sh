find $RPM_INSTALL_PREFIX/madlib/bin -type d -exec cp -RPf {} $RPM_INSTALL_PREFIX/madlib/old_bin \; 2>/dev/null
find $RPM_INSTALL_PREFIX/madlib/bin -depth -type d -exec rm -r {} \; 2>/dev/null

find $RPM_INSTALL_PREFIX/madlib/doc -type d -exec cp -RPf {} $RPM_INSTALL_PREFIX/madlib/old_doc \; 2>/dev/null
find $RPM_INSTALL_PREFIX/madlib/doc -depth -type d -exec rm -r {} \; 2>/dev/null


# RPM version is setup with underscore replaced for hyphen but
# the actual version contains a hyphen. The symlink created on disk
# is with a hyphen instead of the underscore.
MADLIB_VERSION_NO_HYPHEN=%{_madlib_version}
MADLIB_VERSION="${MADLIB_VERSION_NO_HYPHEN/_/-}"
ln -nsf $RPM_INSTALL_PREFIX/madlib/Versions/$MADLIB_VERSION $RPM_INSTALL_PREFIX/madlib/Current
ln -nsf $RPM_INSTALL_PREFIX/madlib/Current/bin $RPM_INSTALL_PREFIX/madlib/bin
ln -nsf $RPM_INSTALL_PREFIX/madlib/Current/doc $RPM_INSTALL_PREFIX/madlib/doc

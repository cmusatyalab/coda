## Process this file with automake to produce Makefile.in

EXTRA_DIST = setup.py \
	codafs/__init__.py codafs/cfs.py codafs/structs.py

# configure gets some installation paths wrong
# - Debian uses a non-standard installation location
# - our python module is not named $PACKAGE
if DEBIAN_DERIVED
SETUP_PY_ARGS = --install-layout deb
endif
pkgpythondir = ${pythondir}/codafs

all-local:
	(cd $(srcdir); $(PYTHON) setup.py build \
		--build-base $(shell readlink -f $(builddir))/build \
		--verbose)

clean-local:
	rm -rf build codafs.egg-info

install-exec-local:
	$(PYTHON) $(srcdir)/setup.py install \
		--prefix $(DESTDIR)$(prefix) \
		--single-version-externally-managed \
		--record $(DESTDIR)$(pkgpythondir)/installed_files.txt \
		$(SETUP_PY_ARGS) --verbose

uninstall-local:
	cat $(DESTDIR)$(pkgpythondir)/installed_files.txt | xargs rm -f
	rm -rf $(DESTDIR)$(pkgpythondir)

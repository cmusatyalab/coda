## Process this file with automake to produce Makefile.in

dist_bin_SCRIPTS = coda-make-certs
dist_man_MANS =
if BUILD_CLIENT
dist_bin_SCRIPTS += coda-sync-acls volmunge
dist_man_MANS += volmunge.1
endif

EXTRA_DIST = setup.py \
	codafs/__init__.py \
	codafs/cfs.py \
	codafs/optional.py \
	codafs/structs.py \
	codafs/util.py \
	codafs/walk.py

# configure gets some installation paths wrong
# - Debian uses a non-standard installation location
# - our python module is not named $PACKAGE
if DEBIAN_DERIVED
SETUP_PY_ARGS = --install-layout deb
endif
pkgpythondir = ${pythondir}/codafs

all-local:
	@(cd $(srcdir); if $(AM_V_P); then PYARG=--verbose; set -x; \
	  else echo "BUILD [python]"; PYARG=--quiet; fi; \
	  $(PYTHON) setup.py $$PYARG build \
		--build-base $(shell readlink -f $(builddir))/build)

clean-local:
	rm -rf build codafs.egg-info

install-exec-local:
	@if $(AM_V_P); then PYARG=--verbose; set -x; \
	 else echo "INSTALL [python]"; fi; \
	 $(PYTHON) $(srcdir)/setup.py install $$PYARG \
		--prefix $(DESTDIR)$(prefix) \
		--single-version-externally-managed \
		--record $(DESTDIR)$(pkgpythondir)/installed_files.txt \
		$(SETUP_PY_ARGS)

uninstall-local:
	cat $(DESTDIR)$(pkgpythondir)/installed_files.txt | xargs rm -f
	rm -rf $(DESTDIR)$(pkgpythondir)

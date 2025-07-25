# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
TOP = $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

RPMTOPDIR := $(HOME)/rpmbuild
SOURCEDIR := $(RPMTOPDIR)/SOURCES
SPECDIR := $(RPMTOPDIR)/SPECS
SPECFILE := $(SPECDIR)/vespa-$(VESPA_VERSION).spec

outdir ?= $(TOP)

deps:
	rpm -q git &>/dev/null || dnf install -y git
	rpm -q rpmdevtools &>/dev/null || dnf install -y rpmdevtools
	git config --global --add safe.directory $$(realpath $(TOP)/..)

srpm: VESPA_VERSION = $(shell git tag --points-at HEAD | grep -oP "\d+\.\d+\.\d+" | sort -V | tail -1)
srpm: deps
	$(TOP)/../dist.sh $(VESPA_VERSION)
	spectool -g -C $(SOURCEDIR) $(SPECDIR)/vespa-$(VESPA_VERSION).spec
	rpmbuild -bs --define "_topdir $(RPMTOPDIR)" --define "_source_payload w10T4.zstdio" $(SPECDIR)/vespa-$(VESPA_VERSION).spec
	cp -a $(RPMTOPDIR)/SRPMS/vespa-$(VESPA_VERSION)-* $(outdir)

rpms: srpm
	rpmbuild --rebuild $(outdir)/*.src.rpm
	cp -a $(RPMTOPDIR)/RPMS/*/*.rpm $(outdir)

nocheck: srpm
	rpmbuild --nocheck --rebuild $(outdir)/*.src.rpm
	cp -a $(RPMTOPDIR)/RPMS/*/*.rpm $(outdir)

clean:
	-rm -rf $(RPMTOPDIR)

.PHONY: clean deps srpm rpms

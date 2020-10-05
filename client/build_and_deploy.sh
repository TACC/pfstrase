#!/bin/bash

make dist
cp pfstrased-1.0.0.tar.gz ~/rpmbuild/SOURCES/
cp pfstrase.spec ~/rpmbuild/SPECS/

rpmbuild -bb ~/rpmbuild/SPECS/pfstrase.spec
ansible-playbook ../deploy/install_pfstrase.yaml

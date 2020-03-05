#!/bin/bash

make dist
cp pfstrased-0.0.1.tar.gz ~/rpmbuild/SOURCES/
cp pfstrase.spec ~/rpmbuild/SPECS/

rpmbuild -bb ~/rpmbuild/SPECS/pfstrase.spec
ansible-playbook ../deploy/install_pfstrase.yaml

#!/bin/bash

make dist
rpmbuild -bb ~/rpmbuild/SPECS/pfstrase.spec
ansible-playbook ../deploy/install_pfstrase.yaml

# PFSTRASE
## About
As the name suggests PFSTRASE (Parallel FileSystem TRacing and Analysis SErvice) provides monitoring of parallel file system servers and clients in order to assist with diagnosing performance issues. Data is collected on throughput and IOPS of clients as well as server load, and can be filtered by server host, client host, user ID or job ID to quickly isolate and identify undesirable activity. Usage can be viewed in real-time via the interactive viewer ‘pfstop’ while the web based interface provides options for analysis of in-depth historical data.

## Installation
### 1. Server setup
The pfstrase server collects usage data from clients (OSSs and MDSs) and makes it available to the monitoring viewer pfstop and sends it to the database.
Build and install pfstrase_server RPM:
```
git clone https://github.com/TACC/pfstrase.git 
cd pfstrase/server
./configure
make dist
mkdir -p ~/rpmbuild/SOURCES
mkdir -p ~/rpmbuild/SPECS
cp pfstrase_server.spec ~/rpmbuild/SPECS
cp *.tar.gz ~/rpmbuild/SOURCES
rpmbuild -bb ~/rpmbuild/SPECS/pfstrase_server.spec
rpm -i ~/rpmbuild/RPMS/pfstrase_server.rpm
```
Edit `pfstrase_server.conf` with aprsopriate values and start the server (remove the `-d` argument for troubleshooting):
```
pfstrase_server -c pfstrase_server.conf  -d
```
In order to map the Lustre client names extracted from the servers to something more readable, create a mapping file and import this into the server. Currently this must be done everytime the server is restarted.
```
nid_file:
###
LustreNodeID  FQDN  hostname 
###

map_nids.py [pfstrase_server] [nid_filename]
```
In order to import current job data from SLURM either run the script `qhost.py` periodically (crontab) or add as a SLURM prolog script.

### 2. Client setup
The client daemon collects usage data and sends it to the server via JSON reports at the specified polling rate.
Build and install pfstrased RPM:
```
cd ../client
automake --add-missing
autoreconf
./configure
make dist
cp *.tar.gz ~/rpmbuild/SOURCES
cp  pfstrase.spec ~/rpmbuild/SPECS

rpmbuild -bb ~/rpmbuild/SPECS/pfstrase.spec 

rpm -i ~/rpmbuild/RPMS/pfstrase_server.rpm
```
Deployment:
Copy the client RPM to the OSSs and MDSs to be monitored, install and start the pfstrase service
An Ansible playbook is available to implement this:
```
ansible-playbook ../deploy/install_pfstrase.yaml
```

## Copyright
(C) 2018 University of Texas at Austin

## License
This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


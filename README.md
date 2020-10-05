# PFSTRASE
## About
As the name suggests PFSTRASE (Parallel FileSystem TRacing and Analysis SErvice) provides monitoring of parallel file system servers and clients in order to assist with diagnosing performance issues. Data is collected on throughput and IOPS of clients as well as server load, and can be filtered by server host, client host, user ID or job ID to quickly isolate and identify undesirable activity. Usage can be viewed in real-time via the interactive viewer ‘pfstop’ while the web based interface provides options for analysis of in-depth historical data.

## Installation
### 1. Server setup
The pfstrase server aggregates usage data from clients (OSSs and MDSs) and makes it available to the monitoring viewer pfstop and optionally sends it to a backend database.
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
rpm -i ~/rpmbuild/RPMS/pfstrase_server-1.0.0.rpm
```
Edit `/etc/pfstrase/pfstrase_server.conf` with appropriate values and start the server (remove the `-d` argument for troubleshooting):
```
systemctl start pfstrase_server
```
In order to map the Lustre client names extracted from the servers to something more readable, and map the jobids and usernames to the clients, create a mapping file and import this into the server. Currently this must be done everytime the server is restarted.
```
nid_file:
###
LustreNodeID  FQDN  hostname 
###

map_nids.py [pfstrase_server] [nid_filename]
```
The nids directory can be examined for example mapping files. This step must be done every time the server is restarted.
In order to import current job data from SLURM either run the script `qhost.py` periodically (crontab) or add as a SLURM prolog script.

#### pfstop
Run the 
```
pfstop
```
command to view the data in an top-like interface. It will filter the data to the user running it unless they are root or part of an administrative group
currently set in line 74 of server/screen.c (this will be configurable in the future using the conf file).

#### PostgreSQL Backend
The server will send data to a database backend if it is configured at build time with
```
./configure --enable-psql
```
The data will be send to the database [dbname] on host [dbserver] at the interval [db_interval] specified in
```
$ cat /etc/pfstrase/pfstrase_server.conf
dbserver    dbserver.tacc.utexas.edu
dbname      pfstrase_dbx
dbuser      postgres
db_interval 30
sharedmem_interval 1
port     5672
```

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

rpm -i ~/rpmbuild/RPMS/pfstrased-1.0.0.rpm
```
Deployment:
Copy the client RPM to the OSSs and MDSs to be monitored, install and start the pfstrase service
```
systemctl start pfstrase
```

An Ansible playbook is available as an example:
```
ansible-playbook ../deploy/install_pfstrase.yaml
```

## Copyright
(C) 2018 University of Texas at Austin

This material is based upon work supported by the National Science Foundation under Grant Number 1835135.

Any opinions, findings, and conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the National Science Foundation.

## License
This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


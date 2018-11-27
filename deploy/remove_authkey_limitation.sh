#!/bin/bash

# Remove the first 155 characters of root's authorized keys file to allow ssh entry
sed -i 's/^.\{155\}//' /root/.ssh/authorized_keys

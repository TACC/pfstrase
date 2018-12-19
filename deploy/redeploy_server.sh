#!/bin/bash



if [ "$#" -ne 1 ]; then
    echo "Input server name to redeploy"
    exit -1
fi

python destroy_server.py "${1}"
python deploy_server.py "${1}"

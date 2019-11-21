#!/bin/bash
echo {\"jid\" : \"$1\" } | nc $2 8888


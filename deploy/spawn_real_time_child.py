#!/usr/bin/env python

## @file experiments/python_util/spawn_real_time_child.py
#  @author W. Cyrus Proctor
#  @date Tuesday October 16 18:28:32
#  @note Copywrite (C) 2012 W. Cyrus Proctor
#  @brief Use of subprocess to create child processes

import sys
import subprocess

import dateortime

## Create subprocesses based on a command string and number of processes
#
# Given command "command" and number of processors "total_num_processes"
# execute the command piping the standard error and standard out pipes to the
# screen for real-time viewing from the user.
#
# @param total_num_processes integer number of processes to be given to the command
# @param commmand string of the command-line task to be executed
#
# Returns the status of the child process that is started. If the status is 0, then
# the command was successful. If status != 0 then there was an error attempting to
# run the command.
#
# @note Assumes that "mpirun" is on the user's PATH variable in the event of a parallel run.
@dateortime.timing
def spawn_real_time_child(total_num_processes, command, logger=sys.stdout):
  
  # Run the command alone if # of processes requested is equal to 1 or using "mpirun"
  # command if more than 1 processes are requested.
  if total_num_processes == 1:
    child = subprocess.Popen(command + ' 2>&1' , shell=True, stdout = subprocess.PIPE)

  elif total_num_processes > 1:
    child = subprocess.Popen('mpirun -np ' + str(total_num_processes) + ' ' \
        + command + ' 2>&1' , shell=True, stdout = subprocess.PIPE)

  else:
    print 'ERROR: spawn_real_time_child: total_num_processes must be > 0'
    print 'Received:',total_num_processes
    exit(-1)

  # Redirect output to the screen for real-time viewing
  complete = False
  line_buffer = ""
  while True:
    out = child.stdout.read(1)
    if out == '' and child.poll() != None:
      break
    if out != '':
      line_buffer += out
      #sys.stdout.write(out)
      if out == '\n':
        logger.info(line_buffer.strip("\n"))
        line_buffer = ""
      sys.stdout.flush()

  # Obtain exit status of command
  status = child.returncode

  return status




## Create subprocesses based on a command string and number of processes
#
# Given command "command", the number of processors "total_num_processes",
# and the host filename "host_filename",
# execute the command piping the standard error and standard out pipes to the
# screen for real-time viewing from the user.
#
# @param total_num_processes integer number of processes to be given to the command
# @param host_filename string of the MPI host filename to be used in distributed computing
# @param commmand string of the command-line task to be executed
#
# Returns the status of the child process that is started. If the status is 0, then
# the command was successful. If status != 0 then there was an error attempting to
# run the command.
#
# @note Assumes that "mpirun" is on the user's PATH variable in the event of a parallel run.
@dateortime.timing
def spawn_real_time_child_distributed(total_num_processes,host_filename,command):
  
  # Use "mpirun" regardless of the number of processes along with the given
  # host_filename
  if total_num_processes >= 1:
    child = subprocess.Popen('mpirun -np ' + str(total_num_processes) + ' ' \
       + ' -f ' + host_filename + ' ' + command + ' 2>&1' , shell=True,\
       stdout = subprocess.PIPE)

  else:
    print 'ERROR: spawn_real_time_child: total_num_processes must be > 0'
    print 'Received:',total_num_processes
    exit(-1)

  # Redirect output to the screen for real-time viewing
  complete = False
  while True:
    out = child.stdout.read(1)
    if out == '' and child.poll() != None:
      break
    if out != '':
      sys.stdout.write(out)
      sys.stdout.flush()

  # Obtain exit status of command
  status = child.returncode

  return status




## Create subprocesses based on a command string and number of processes
#
# Given command "command" and number of processors "total_num_processes"
# execute the command piping the standard error and standard out pipes to the
# screen for real-time viewing from the user.
#
# @param total_num_processes integer number of processes to be given to the command
# @param commmand string of the command-line task to be executed
# @param filename_base string to be used in the log file's name as a base
#
# Returns the status of the child process that is started. If the status is 0, then
# the command was successful. If status != 0 then there was an error attempting to
# run the command.
#
# @note Assumes that "mpirun" is on the user's PATH variable in the event of a parallel run.
@dateortime.timing
def spawn_real_time_child_logged(total_num_processes,command,filename_base):

  # Create a datetime string for the log file
  dtstr = dateortime.datetime_to_string()
  
  # Run the command alone if # of processes requested is equal to 1 or using "mpirun"
  # command if more than 1 processes are requested.
  if total_num_processes == 1:
    child = subprocess.Popen(command + ' 2>&1 | tee ' + dtstr + '_' + \
        filename_base + '.log' , shell=True, stdout = subprocess.PIPE)

  elif total_num_processes > 1:
    child = subprocess.Popen('mpirun -np ' + str(total_num_processes) + ' ' \
        + command + ' 2>&1 | tee ' + dtstr + '_' + filename_base + '.log' ,\
        shell=True, stdout = subprocess.PIPE)

  else:
    print 'ERROR: spawn_real_time_child: total_num_processes must be > 0'
    print 'Received:',total_num_processes
    exit(-1)

  # Redirect output to the screen for real-time viewing
  complete = False
  while True:
    out = child.stdout.read(1)
    if out == '' and child.poll() != None:
      break
    if out != '':
      sys.stdout.write(out)
      sys.stdout.flush()

  # Obtain exit status of command
  status = child.returncode

  return status

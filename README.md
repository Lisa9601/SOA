# SOA
This project implements a Linux kernel subsystem
that allows exchanging messages across threads.
The service has 32 levels (namely, tags) by default,
and it's driven by the following system calls:

* <b>int tag_get(int key, int command, int permission)</b>,
  this system call instantiates or opens the TAG service
  associated with key depending on the value of command.
  The IPC_PRIVATE can be used for key to instantiate the service
  in such a way that it cannot be re-opened by the same system call.
  The return value indicates the TAG service descriptor (-1 is the return error)
  for handling the actual operations on the TAG service.
  Also, the permission value indicates whether the TAG service is created
  for operations by threads running a program on behalf of the same user
  installing the service, or by any thread.
  
* <b>int tag_send(int tag, int level, char* buffer, size_t size)</b>,
  this service delivers to the TAG service with tag as the descriptor the message
  currently located in the buffer at address and made of size bytes.
  All the threads that are currently waiting for such a message on the
  corresponding value of level will be resumed for execution and will receive
  the message (zero lenght messages are anyhow allowed).
  The service does not keep the log of messages that have been sent,
  hence if no receiver is waiting for the message this is simply discarded.
  
* <b>int tag_receive (int tag, int level, char* buffer, size_t size)</b>,
  this service allows a thread to call the blocking receive operation of the message
  to be taken from the corresponding tag descriptor at a given level.
  The operation can fail also because of the delivery of a Posix signal to
  the thread while the thread is waiting for the message.
  
* <b>int tag_ctl(int tag, int command)</b>, this system call allows the caller to
  control the TAG service with tag as descriptor according to command that can be
  either AWAKE_ALL (for awaking all the threads waiting for messages, independently of the level),
  or REMOVE (for removing the TAG service from the system).
  A TAG service cannot be removed if there are threads waiting for messages on it. 

By default, at least 256 TAG services are allowed to be handled by software, and
the maximum size of the handled message is of at least 4 KB.

Also, a device driver was implemented in order to check with the current state, namely the TAG service
current keys and the number of threads currently waiting for messages.
Each line of the corresponding device file it's structured as
"TAG-key TAG-creator TAG-level Waiting-threads".

## Requirements

To run this project you need to install the linux headers for your linux distribution. To check which version
of the kernel you have currently installed you can use:

    uname -r

> :warning: The headers must have the **same version** of the kernel

## Configuration

You can configure the maximum number of tag services, the maximum number of levels
for each service and the size of the buffer used to pass the message. 

To do so open config.h file
in the root of the project and change them to your liking.

## Deployment
1. Create all needed files
```
make all
```
2. Insert new module into the kernel
```
insmod soa.ko
```

## Demo
You can run a demo of this project which lets you call each of the 
system calls defined in the top of this file.

    cd demo
    
    gcc demo.c

    ./a.out

#### Available commands

  * **get key command permission** calls tag_get with the specified key, command and permission
    * **command = 1** CREATE 
    * **command = 2** OPEN

  * **send tag level buffer** calls tag_send on the specified tag and level to send the buffer

  * **receive tag level size** calls tag_receive on the specified tag and level to receive a message of the specified size

  * **ctl tag command** calls tag_ctl on the specified tag service to execute the command
    * **command = 3** AWAKE ALL
    * **command = 4** REMOVE

  * **help** shows the list of available commands

  * **quit** quits the demo

## Directory tree
```
/
  demo/
      include/
          threads.h
      lib/
          threads.c    
      demo.c
  include/
      level.h
      tag.h
      vtpmo.h
  lib/
      level.c
      tag.c
      usctm.c
      vtpmo.c
  
  config.h
  Makefile
  README.md
```
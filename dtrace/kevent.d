#!/usr/sbin/dtrace -s
 
 uint64_t checkpoint;
 
 fbt::sys_kevent:entry
 {
     self->traceme = 1;
     trace(timestamp);
 }
 
 fbt:::
 /self->traceme/
 {
     trace(timestamp);
 }
 
 fbt::sys_kevent:return
 /self->traceme/
 {
     self->traceme = 0;
     trace(timestamp);
 }

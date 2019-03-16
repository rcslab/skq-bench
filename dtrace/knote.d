#!/usr/sbin/dtrace -s
 
 uint64_t checkpoint;
 
 fbt::knote:entry
 {
     self->traceme = 1;
     trace(timestamp);
 }
 
 fbt:::
 /self->traceme/
 {
     trace(timestamp);
 }
 
 fbt::knote:return
 /self->traceme/
 {
     self->traceme = 0;
     trace(timestamp);
 }

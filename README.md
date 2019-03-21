# isolate

**isolate** is a minimalistic demonstration of Linux namespaces. It drops you into a shell in an isolated environment, with an isolated PID, UID, and filesystem.

Example:

```
$ make
$ ./build/isolate foo
bash-4.4# ls
bin  lib  lib64  proc  sbin  usr
```

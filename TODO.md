Todo
====

C library
---------

Word count matrix ("dfm" in quanteda lingo).


Windows portability
-------------------

`mmap` calls are unix-specfic. Use the appropriate alternatives for
windows builds

`syslog` is unix-specific. Add custom logging? This would make
error reporting better for, e.g., the R interface.

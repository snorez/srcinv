# SRCINV

[中文版](https://github.com/snorez/srcinv/tree/master/doc/README.zh)

[English](https://github.com/snorez/srcinv/tree/master/doc/README.en)

# Build this project
To build this project, you need several libraries:
+	libncurses
+	libreadline
+	libcapstone
+	clib
+	gcc-plugin

Modify CLIB_PATH and gcc plugin include path in ./Makefile ./plugins/Makefile ./collect/Makefile

Run `make` or `make ver=[quick_dbg|release]`

# LICENSE
This project is under GPL v3 license. See the LICENSE for more details.

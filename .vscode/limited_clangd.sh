#!/bin/bash
ulimit -v 4194304
/usr/bin/clangd "$@"
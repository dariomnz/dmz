#!/bin/bash
ulimit -v 6291456
/usr/bin/clangd "$@"
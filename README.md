Tiny FreeBSD Azure tooling
==========================

This repository is intended to contain minimal tooling for deploying FreeBSD in Azure.
The Azure agent has a Python dependency, which is not part of the base system and so brings in a lot of things that are not required for a FreeBSD system.
This repository contains a simple tool (no dependencies outside of the base system, only UCL and libc++) that compiles to a ~40KiB executable (dynamically linked) that can provide information to rc scripts that manage the system.

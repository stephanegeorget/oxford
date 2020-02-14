#!/bin/bash -x
# Export a variable that tells us where is the root of this project
# This is used by other scripts in this project.
# /!\ CALL WITH source, so the environment variable applies to the running shell:
# source make_iso_0.sh
export PROJECT_HOME="$(pwd)"

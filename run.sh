#!/bin/bash

set -e

make

./timeout_strategy "$@"

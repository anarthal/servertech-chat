#!/bin/bash

grep -rh '#include <' src/server/  | sort | uniq > src/pch.hpp
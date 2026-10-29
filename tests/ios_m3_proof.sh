#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
swift tools/ios-m3-proof.swift

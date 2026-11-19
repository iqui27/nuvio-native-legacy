#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
rg -q 'guia_canal_logo_url' src/guia.c src/discord.c
echo ok guia_discord_logo

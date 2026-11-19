#!/usr/bin/env bash
set -e
mkdir -p "$HOME/.vst3" "$HOME/.clap"
rm -rf "$HOME/.vst3/StepBloom.vst3" "$HOME/.clap/StepBloom.clap"
cp -r StepBloom.vst3 "$HOME/.vst3/"
cp StepBloom.clap "$HOME/.clap/"
echo "StepBloom VST3 and CLAP installed to ~/.vst3 and ~/.clap"

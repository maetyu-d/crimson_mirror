#!/bin/zsh
set -eu
prism_root="${0:A:h}"
if [[ ! -d "$prism_root/Prism Currents.app" ]]; then
  cmake -S "$prism_root" -B "$prism_root/build-native" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$prism_root/build-native" -j 6
  ditto "$prism_root/build-native/PrismCurrents.app" "$prism_root/Prism Currents.app"
fi
open "$prism_root/Prism Currents.app"

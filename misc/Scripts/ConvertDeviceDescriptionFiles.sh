#!/bin/bash
SCRIPTDIR="$( cd "$(dirname $0)" && pwd )"
cd $SCRIPTDIR/Original
for f in *.xml; do
  echo "Processing file $f..."
  homegear -o $f ../Patched/$f
done

#!/bin/bash

MACPORTS=$OSXCROSS_TARGET_DIR/macports/pkgs
MACOS_PREFIX=$(uname -m)-apple-$OSXCROSS_TARGET

patch_binary() {
    local LIBRARIES=$($MACOS_PREFIX-otool -L "$2" | tail -n +2 | awk '{print $1}')
    for i in $LIBRARIES; do
        if [[ $i == /opt/local/lib/* ]]; then
            if [[ $(echo $2 | sed 's/.*\///') == $(echo $i | sed 's/.*\///') ]]; then
                continue
            fi
            echo "Patching $i"
            $MACOS_PREFIX-install_name_tool -change "$i" "$(echo $i | sed 's/\/opt\/local\/lib/@executable_path/')" "$2"
            local DESTINATION=$1/$(echo $i | sed 's/\/opt\/local\/lib\///')
            cp "$MACPORTS$i" "$DESTINATION"
            patch_binary "$1" "$DESTINATION"
        fi
    done
}

patch_binary "$1" "$2"
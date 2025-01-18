#!/bin/sh


if [ -z "$BURN_ONTO" ]; then
    echo "$0: Missing path to install onto (Please specify BURN_ONTO=)"
    exit 1
fi

set -xe


cp -v ./bin/OS.iso ./bin/OS_Bios.iso
./kernel/vendor/limine/limine bios-install ./bin/OS_Bios.iso
sudo dd if=./bin/OS_Bios.iso of=$BURN_ONTO status=progress oflag=sync bs=1M
sync
sudo eject $BURN_ONTO

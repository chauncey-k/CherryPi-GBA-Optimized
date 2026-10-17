# CherryPi GBA optimized runtime · 2026-10-17

Binary SHA256: 923fb7fff27a3f0eb2bfc4b3599a62dd0149a478444ee8a521e49a563589f250

Board-tested smc-live build: safe snapshots, quick menu, settings and runtime audio-instruction variants. Installer accepts the native-settings baseline for fresh deployment, smc-guards for upgrade, or this exact binary for reinstall. It changes only /root/gpsp/gpsp, with its own initial backup under /root/cherrypi-optimized-20261017/state.

Stop gpsp/cherrypi-launcher, then run sh install.sh. For rollback stop both processes and run sh uninstall.sh. run-profile.sh is optional diagnosis; normal boot does not enable sampling.

Corresponding source and tests are in this repository components/gpsp/source and tests, and also inside the generated runtime archive. No game, BIOS or personal snapshot is included. See repository docs for fresh SD card installation and launching games.

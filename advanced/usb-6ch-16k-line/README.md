# USB 6-channel 16 kHz LINE Debug

Diagnostic capture firmware forced to LINE microphone geometry. It exposes the
four physical microphone paths in a six-channel USB stream so developers can
confirm microphone health and ordering.

This is not a normal processed product mode. Use a multichannel recorder that
can explicitly open all six channels. The `_dfu_upgrade.bin` image is the DFU
update file; the `.xe` file is for XTAG development.

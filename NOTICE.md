# Licenses and publication scope

RASPIAUDIO's original source code and documentation in this V2 branch are
published under [MIT](LICENSE). Third-party notices take precedence for their
files; MIT does not relicense vendor software.

The PAJ7620U2 initialization table derives from DFRobot's
[DFRobot_PAJ7620U2](https://github.com/DFRobot/DFRobot_PAJ7620U2), pinned revision
`57edc1d`. Its [MIT notice](Raspberry%20Pi/Sources/licenses/PAJ-LICENSE.txt) is retained.

The portable host/UI files use the historical `esphome::voicedsp_v2` namespace.
The Pi controller has no ESPHome framework dependency. USB/Home Assistant
releases will receive their own dependency notices when published.

XMOS XVF3800 firmware uses proprietary VocalFusion/BeClear components.
The [vendor package licence](Raspberry%20Pi/Firmware/LICENSE.xmos-package.rst)
remains applicable to those components. Firmware is provided for compatible
RASPIAUDIO hardware containing the supported licensed XMOS device. The MIT
license covers our original host/integration code, not proprietary DSP libraries.
The complete XMOS SDK is not mirrored here.

V2 starts from the existing public main history. Older public artifacts in that
history are not part of, or valid substitutes for, this V2 release.

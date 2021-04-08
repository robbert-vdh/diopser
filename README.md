# Diopser

[![Automated builds](https://github.com/robbert-vdh/diopser/workflows/Automated%20builds/badge.svg?branch=master&event=push)](https://github.com/robbert-vdh/diopser/actions?query=workflow%3A%22Automated+builds%22+branch%3Amaster)

You were expecting Disperser[¹](#disperser), but it was me, Diopser!

Diopser lets you rotate the phase of a signal around a specific frequency
without affecting its spectral content. This effect can be used to emphasize
transients and other parts of a sound that in a way that isn't possible with
regular equalizers or dynamics processors, especially when applied to low
pitched or wide band sounds. More extreme settings will make everything sound
like a cartoon laser beam, or a psytrance kickdrum. If you are experimenting
with those kinds of settings, then you may want to consider temporarily placing
a peak limiter after the plugin in case loud resonances start building up.

<sup id="disperser">
  *Disperser is a trademark of Kilohearts AB. Diopser is in no way related to
  Disperser or Kilohearts AB.
</sup>

## Download

You can download the latest development binaries for Linux, Windows and macOS
from the [automated
builds](https://github.com/robbert-vdh/diopser/actions?query=workflow%3A%22Automated+builds%22+branch%3Amaster)
page. GitHub requires you to be signed in to be able to download these files. I
sadly cannot provide any support for the Windows and macOS versions at the
moment, but the plugins should work!

## Building

To build the VST3 plugin, you'll need [CMake 3.15 or
higher](https://cliutils.gitlab.io/modern-cmake/chapters/intro/installing.html)
and a recent C++ compiler.

```shell
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --config Release
```

You'll find the compiled plugin in `build/Diopser_artefacts/Release/VST3`.

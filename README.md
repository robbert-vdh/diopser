# Diopser

You were expecting Disperser[¹](#disperser) but it was me, Diopser!

<sup id="disperser">
  *Disperser is a trademark of Kilohearts AB, Diopser is in no way related to Disperser or Kilohearts.
</sup>

## Building

To build the VST3 plugin, you'll need [CMake 3.15 or
higher](https://cliutils.gitlab.io/modern-cmake/chapters/intro/installing.html)
and a recent C++ compiler.

```shell
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

You'll find the compiled plugin in `build/Diopser_artefacts/Release/VST3`.

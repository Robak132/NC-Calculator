Web version: https://leu-235.com/ (Built using emscripten. See [web/compile.bat](compile.bat) for details.)

To build the benchmark,

```sh
git submodule update --init

mkdir build
cd build
cmake ..
```

If you get errors that `xtensor` or `xtl` include files cannot be found, you have likely not run `git submodule update --init`.

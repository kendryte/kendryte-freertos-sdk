Kendryte K210 SDK
======

This SDK is for Kendryte K210 which contains FreeRTOS support.  
If you have any questions, please be free to contact us.

# Usage

If you want to start a new project, for instance, `hello_world`, you only need to:

`mkdir` your project in `src/`, `cd src && mkdir hello_world`, then put your codes in it, and build it.

```shell
mkdir build && cd build
cmake .. -DPROJ=<ProjectName> -DTOOLCHAIN=/opt/riscv-toolchain/bin && make
```

You will get 2 key files, `hello_world` and `hello_world.bin`.

1. If you are using JLink to run or debug your program, use `hello_world`
2. If you want to flash it in UOG, using `hello_world.bin`, then using flash-tool(s) burn <ProjectName>.bin to your flash.

This is very important, don't make a mistake in files.

*If you don't like place code inside SDK, see `CMakeLists.txt.example.cmake`*

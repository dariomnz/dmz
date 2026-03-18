<p align="center">
  <img src=".vscode/extensions/dmz-language-support/assets/dmz_logo.svg" width="128" />
</p>

# dmz Programming Language

`dmz` is a modern, statically-typed compiled programming language designed with simplicity, predictability, and performance in mind. Inspired by elements from languages like Zig and Rust, `dmz` aims to provide powerful features with straightforward syntax, minimal runtime overhead, and seamless C interoperability.

## Features

- **Modern Syntax:** Intuitive syntax with `fn`, `const`, `let`, `if`, and `while` loops.
- **Strong Typing:** Support for core primitives including `i32`, `u32`, `f32`, `bool`, etc.
- **Memory Control:** Native pointers, arrays, and slices.
- **Safety and Ergonomics:** Integrated error handling (`error`), `defer`, and `errdefer` for robust resource management.
- **C Interoperability:** Easily link and call external C functions (`extern fn`).
- **Generics & Meta-programming:** Generic constructs and compile-time features such as `@typeinfo` and `@hasMethod`.
- **SIMD Support:** Capabilities for expressing Single Instruction, Multiple Data (SIMD) operations.
- **Standard Library:** A rapidly growing core library located in the `std` folder (`import("std")`).
- **Tooling:** Comprehensive built-in tooling featuring a Language Server Protocol (LSP) implementation, a code formatter, and an integrated testing infrastructure.

## Examples

### Hello, World!
```dmz
const std = import("std");

fn main() -> void {
    std.io.print("Hello world!\n", .{});
}
```

### Fibonacci Sequence
```dmz
extern fn printf(fmt: *u8, ...) -> i32;

fn fib(n: i32) -> i32 {
    if (n == 0 || n == 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() -> void {
    let i = 0;
    while (i < 20) {
        printf("%d\n", fib(i));
        i = i + 1;
    }
}
```
## Editor Support

`dmz` comes with a dedicated VSCode extension for syntax highlighting and LSP integration. You can find it in [.vscode/extensions/dmz-language-support](.vscode/extensions/dmz-language-support)

## Prerequisites

To build the `dmz` compiler from source, you will need:
- **C++ Compiler** with support for **C++20**.
- **CMake** (3.12 or newer).
- **LLVM** development packages (the compiler uses LLVM as its backend).

## Building from source

1. Clone the repository and navigate to it:
   ```bash
   git clone <repository-url> dmz
   cd dmz
   ```

2. Create a build directory and configure the project:
   ```bash
   mkdir build && cd build
   cmake ..
   ```

3. Compile:
   ```bash
   make
   ```
   The compiler binary will be placed under the `build/bin/` directory.

## For developers

When making modifications or contributing to the project, use the provided build script to compile the compiler and automatically run the regression test suite:

```bash
./dev/build.sh
```

## License

Check the included [LICENSE](LICENSE) file for detailed rules and permissions.
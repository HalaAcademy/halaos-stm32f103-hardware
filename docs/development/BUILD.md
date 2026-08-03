# Build HalaOS cho Blue Pill

## Công cụ

- Python 3.
- Clang/LLVM hỗ trợ target `armv7m-none-eabi`.
- LLD.
- LLVM objcopy hoặc ARM GNU objcopy.
- `nm` và `size`.

## Build chuẩn

```bash
make build
```

## Build qualification instrumentation

```bash
make qualification
```

## Build image lỗi để kiểm tra Stage-0 trên board

```bash
make bad-manifest
make bad-dtb
```

## Kiểm tra tài nguyên và tái lập

```bash
make audit
make reproducible
```

Build output chỉ được tạo trong `out/`.

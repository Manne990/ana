# Vendored host-only dependencies

## stb_image.h

`stb_image.h` is used only by `tools/ana-convert` for PNG decoding on the host
machine. It is not included in `libana.a` and is not linked into Amiga runtime
code.

Source:

- https://github.com/nothings/stb

License:

- public domain or MIT, as stated in `stb_image.h`

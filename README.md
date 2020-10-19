# justc-installer

A library for making self-extracting tar files.

This was created specifically for the
[`s6-overlay`](https://github.com/just-containers/s6-overlay) project.
It is not currently recommended for creating generic, self-extracting
tar files - it has behavior that is specific to the `s6-overlay` project,
is only tested with GNU tar, and only tested with the `s6-overlay` tar
file.

## Usage

The library is compiled as a static archive with a `main` function
already defined. The `main` function will extract the contents
of a tar file, it expects for three symbols to exist:

* `unsigned char _binary_payload_tar_start[]`
* `unsigned char _binary_payload_tar_end[]`
* `size_t _binary_payload_tar_size`

`main` will extract the payload to a given destination.

It will only create directories on the filesystem if they do not exist. This
is primarily to support distributions that have replaced `/bin` with a
symlink to `/usr/bin`. Most tar implementations will remove `/bin` and replace
it with a directory (assuming `/bin` is a directory inside the tarfile).

To produce the payload object you can use `ld`:

```
ld -r -b binary payload.tar -o payload.o
```

Then link everything together into an executable:

```
gcc -static -o installer libjustc_installer.a payload.o -lskarnet
```

You'll now have a self-extracting executable.


# dwarf

## remove debug info

```bash
gcc -g a.c -o a
# readelf -WS a
```

```bash
objcopy --only-keep-debug a a.debug
objcopy --strip-debug a a.stripped
mv a.stripped a
# readelf -WS a
```

create debug info link `.gnu_debuglink`

```bash
objcopy --add-gnu-debuglink=a.debug a

# readelf --string-dump=.gnu_debuglink a
```

readelf --notes a

```bash
o
```

objdump --dwarf=info lgo
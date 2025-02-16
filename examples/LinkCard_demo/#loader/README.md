# DLC Loader

This loader scans cards with the **e-Reader** (or in Japan, the **e-Reader+**) and sends the contents to `LinkCard`.

## Required tools for compiling

- _devkitARM_ with _GCC 14.1.0_
- `nevpk.exe`, `neflmake.exe`, `nedcmake.exe`, `nedcenc.exe`, `raw2bmp.exe`
- _Git Bash_ or some way to run Unix commands like `dd`

## Compile

``` bash
# verify tool paths in the `Makefile`!
make clean
make       # USA
make JAP=1 # JAP
#    ^ check that NAME = ローダー and `Makefile` is encoded in Shift JIS
```

This will generate files named `main.bin.0*.bin` (the card parts) and also a `main.loader` file, which you can use to feed `LinkCard` as the _loader_.

## Encode cards

```bash
nedcmake -i testcard.txt -o testcard.bin -type 3 -bin -raw -fill 1 -region 1 -name "Game name"
# region 1 = USA
# region 2 = JAP
```



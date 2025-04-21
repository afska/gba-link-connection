# DLC Loader

This loader scans cards with the **e-Reader** (or in Japan, the **e-Reader+**) and sends the contents to `LinkCard`.

## Required tools for compiling

- Windows
- _devkitARM_ with _GCC 14.1.0_
- `nedcmake.exe`, `nevpk.exe`, `nedcenc.exe`, `raw2bmp.exe`, `neflmake.exe`
  * Cross-platform versions are available [here](https://github.com/AkBKukU/e-reader-dev) and [here](https://github.com/breadbored/nedclib)
- _Git Bash_ or some way to run Unix commands like `dd`

## Compile

``` bash
# verify tool paths in the `Makefile`!
make clean
make       # USA region, English language
make JAP=1 # JAP region, Japanese language
make JAP=1 ENG=1 # JAP region, English language
#    ^ check that NAME = ローダー and `Makefile` is encoded in Shift JIS
```

This will generate files named `main.bin.0*.bin` (the card parts) and also a `main.loader` file, which you can use to feed `LinkCard` as the _loader_.

## Encode cards

```bash
nedcmake -i testcard.txt -o testcard.bin -type 3 -bin -raw -fill 1 -region 1 -name "Game name"
# region 1 = USA
# region 2 = JAP
```

## Convert cards to BMP for distribution

```bash
raw2bmp -i testcard.bin-01.raw -o testcard.bin-01
# generates testcard.bin-01.bmp
```

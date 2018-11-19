# firmimg
Is a tools for iDRAC family firmware update image

# Supported iDRAC family

*iDRAC 9 use directly uImage*

| Family  | Supported | Info | Extract | Pack |
| ------- |:---------:|:----:|:-------:|-----:|
| iDRAC 6 | Yes       | Yes  | Yes     | No   |
| iDRAC 7 | No        | No   | No      | No   |
| iDRAC 8 | No        | No   | No      | No   |
| iDRAC 9 | No        | -    | -       | -    |

# Example

## Info

```
firmimg --info=firmimg.d6
```

## Extract

```
firmimg --extract=firmimg.d6
```

## Compact

```
firmimg --compact=firmimg.d6 --version=2.91 --build=2 --uboot_version=1.2.0 --avct_uboot_version=1.19.8 --platform_id=WHOV image_0.dat image_1.dat image_2.dat
```

# Parallel Image Filter

Simple image filter with threads using neighborhood average on pixel for parallel and distributed programming class.

# Input<br/>
512x512 image.rgba

# Output<br/>
image.rgba.new

To run:
```
./smooth image_filename.rgba
```
To convert image:
```
convert -size 512x512 -depth 8 rgba:img.rgba.new img.tiff
```


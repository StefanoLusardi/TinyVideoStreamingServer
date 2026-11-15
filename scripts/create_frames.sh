mkdir -p frames1 frames2
ffmpeg -y -f lavfi -i "testsrc=size=640x480:rate=15" -t 10 -q:v 5 frames1/%04d.jpg
ffmpeg -y -f lavfi -i "testsrc2=size=640x480:rate=15" -t 10 -q:v 5 frames2/%04d.jpg

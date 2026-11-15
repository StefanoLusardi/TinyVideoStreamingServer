python3 tools/fake_esp32.py --port 9005 --frames frames1 &
python3 tools/fake_esp32.py --port 9006 --frames frames2 &

./build/Debug/mjpeg-proxy --port 8080 \
  salotto=http://127.0.0.1:9005/stream \
  garage=http://127.0.0.1:9006/stream


# open http://127.0.0.1:8080

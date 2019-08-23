killall ittb_control
killall ittb_stream 
kill -9 `ps | grep control |head -1| awk '{print $1}'`;




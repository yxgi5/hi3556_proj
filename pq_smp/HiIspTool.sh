
stream="-s"
all="-a"
help="-h"
control="-c"
CtrlPID=0
StreamPID=0
timeout=0
exitflag=0
DLL_PATH=${LD_LIBRARY_PATH}:${PWD}/libs

if [ $# -gt 3 ];then
echo "To start only the ittb_stream and obtain initial configurations from the board, run ./HiIspTool.sh -s  sensortype.(sensortype: type of the sensor)"
echo "To start only the ittb_control, run ./HiIspTool.sh -c."
echo "To start full services and obtain initial configurations from the board, run ./HiIspTool.sh -a  sensortype.(sensortype: type of the sensor)"
echo "Help: ./HiIspTool.sh -h"
echo "Note ,All initial configuration files are stored in the release\configs directory."
exit

else
case $1 in
control|$control)
timeout=0
killall ittb_control
killall ittb_stream 
while true;do
        CtrlPID=`pidof ittb_control`
		StreamPID=`pidof ittb_stream`
		echo "control PID:" $CtrlPID
		echo "stream PID:" $StreamPID
        if [ -z $CtrlPID ] && [ -z $StreamPID ];then
			echo	">>>>no pid,run it"
			break
		else
			echo ">>>>program is exiting waiting"
			sleep 2
			timeout=`expr $timeout + 1`
		fi
		if [ $timeout -eq 5 ];then
			if [ -z $CtrlPID ];then
				:
			else
				wait $CtrlPID
				kill -9 $CtrlPID
			fi
			if [ -z $StreamPID ];then
				:
			else
				wait $StreamPID
				kill -9 $StreamPID
			fi
			break
		fi
done
export LD_LIBRARY_PATH=${DLL_PATH}
./ittb_control& exit;;


stream|$stream)
timeout=0
killall ittb_control
killall ittb_stream  
while true;do
        CtrlPID=`pidof ittb_control`
		StreamPID=`pidof ittb_stream`
		echo "control PID:" $CtrlPID
		echo "stream PID:" $StreamPID
        if [ -z $CtrlPID ] && [ -z $StreamPID ];then
			echo    ">>>>no pid,run it"
			break
		else
			echo ">>>>program is exiting waiting"
			sleep 2
			timeout=`expr $timeout + 1`
		fi
		if [ $timeout -eq 5 ];then
			if [ -z $CtrlPID ];then
				:
			else
				wait $CtrlPID
				kill -9 $CtrlPID
			fi
			if [ -z $StreamPID ];then
				:
			else
				wait $StreamPID
				kill -9 $StreamPID
			fi
			break
		fi
done
export LD_LIBRARY_PATH=${DLL_PATH}
./ittb_stream $2 $3&  exit;;

all|$all)
killall ittb_control
killall ittb_stream  
while true;do
        CtrlPID=`pidof ittb_control`
		StreamPID=`pidof ittb_stream`
		echo "control PID:" $CtrlPID
		echo "stream PID:" $StreamPID
        if [ -z $CtrlPID ] && [ -z $StreamPID ];then
			echo    ">>>>no pid,run it"
			break
		else
			echo ">>>>program is exiting waiting"
			sleep 2
			timeout=`expr $timeout + 1`
		fi
		if [ $timeout -eq 5 ];then
			if [ -z $CtrlPID ];then
				:
			else
				wait $CtrlPID
				kill -9 $CtrlPID
			fi
			if [ -z $StreamPID ];then
				:
			else
				wait $StreamPID
				kill -9 $StreamPID
			fi
			break
		fi
done
export LD_LIBRARY_PATH=${DLL_PATH}
./ittb_stream $2 $3 $4&
while true;do
	if [ $(ps -T| grep {RecvCfgProc}  |head -1| awk '{print $4}') = "{RecvCfgProc}" ];then
		break;
	fi
	sleep 1	
	if [ $(ps | grep ittb_stream |head -1| awk '{print $4}') = "grep" ];then
		exit;
	fi
done
./ittb_control & exit;;
usage|$help) 
echo "To start only the ittb_stream and obtain initial configurations from the board, run ./HiIspTool.sh -s  sensortype.(sensortype: type of the sensor)"
echo "To start only the ittb_control, run ./HiIspTool.sh -c."
echo "To start full services and obtain initial configurations from the board, run ./HiIspTool.sh -a  sensortype.(sensortype: type of the sensor)"
echo "Help: ./HiIspTool.sh -h"
echo "Note ,All initial configuration files are stored in the release\configs directory."
exit;;
esac

fi

echo "To start only the ittb_stream and obtain initial configurations from the board, run ./HiIspTool.sh -s  sensortype.(sensortype: type of the sensor)"
echo "To start only the ittb_control, run ./HiIspTool.sh -c."
echo "To start full services and obtain initial configurations from the board, run ./HiIspTool.sh -a  sensortype.(sensortype: type of the sensor)"
echo "Help: ./HiIspTool.sh -h"
echo "Note ,All initial configuration files are stored in the release\configs directory."


#! /bin/bash
HOST=praticap@100.113.57.92
HOST2=praticap@100.70.234.4

trap 'echo "Ending program"; kill $(jobs -p); exit' SIGINT SIGTERM

for i in {1..10}; do 
	for ((j=2;j<71;j+=2)); do 

		sensor_count=$((j / 2))

		echo "Starting sensor_node"
		ssh -o BatchMode=yes "${HOST}" "
			docker exec ros2_humble bash -lc '
				cd /root/ros2_ws &&
				. run_script.sh &&
				export ROS_DOMAIN_ID=31 &&
				ros2 run cassio_driver sensor_node --ros-args \
					-p sensor_count:=${sensor_count} \
					-p publish_rate_hz:=100 \
					-p payload_size_kb:=20 \
					-p hardware_id:=0 \
					-p instance_count:=1 \
					-p benchmark_duration_sec:=180
			'
		" &

		sleep 5

		echo "Starting sensor_node_2"
		ssh -o BatchMode=yes "${HOST2}" "
			docker exec ros2_humble bash -lc '
				cd /root/ros2_ws &&
				. run_script.sh &&
				export ROS_DOMAIN_ID=31 &&
				ros2 run cassio_driver sensor_node --ros-args \
					-p sensor_count:=${sensor_count} \
					-p publish_rate_hz:=100 \
					-p payload_size_kb:=20 \
					-p hardware_id:=1 \
					-p instance_count:=1 \
					-p benchmark_duration_sec:=180
			'
		" &

		sleep 5
		
		echo "Starting node_central"
		docker exec ros2_humble bash -lc "
			cd /root/ros2_ws &&
			. run_script.sh &&
			export ROS_DOMAIN_ID=31 &&
			ros2 run node_central node_central \
				--ros-args \
				-p instance_id:=0 \
				-p instance_count:=1 \
				-p benchmark_number:=\\\"${i}\\\" \
				-p sensor_count:=\\\"${j}\\\" \
				-p benchmark_duration_sec:=180 \
				-p start_signal_delay_sec:=15
		" & 

		wait

		sleep 10
	done
done

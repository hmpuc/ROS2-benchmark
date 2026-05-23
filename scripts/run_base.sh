#! /bin/bash
HOST=

for ((i=2;i<21;i+=2)); do 
	ssh "${HOST}" "docker exec -d ros2_humble bash -lc '
    cd /root/ros2_ws &&
    . run_script.sh &&
    export ROS_DOMAIN_ID=31 &&
    ros2 run cassio_driver sensor_node --ros-args \
      -p sensor_count:=${i} \
      -p publish_rate_hz:=100 \
      -p payload_size_kb:=20 \
      -p hardware_id:=0 \
      -p instance_count:=1 \
      -p benchmark_duration_sec:=180
  	'"

	sleep 5

	docker exec ros2_humble bash -lc "
	cd /root/ros2_ws &&
    . run_script.sh &&
	ros2 run node_central node_central \
		--ros-args \
		-p instance_id:=0 \
		-p instance_count:=1\
		-p benchmark_number:=\\\"warmup_${i}\\\" \
		-p sensor_count:=\\\"${i}\\\" \
		-p benchmark_duration_sec:=180 \
		-p start_signal_delay_sec:=3
	"
	sleep 5
done

rm -rf benchmarks

for i in {1..10}; do 
	for ((j=2; j<71;j+=2)); do 
		ssh "${HOST}" "docker exec -d ros2_humble bash -lc '
		cd /root/ros2_ws &&
		. run_script.sh &&
		export ROS_DOMAIN_ID=31 &&
		ros2 run cassio_driver sensor_node --ros-args \
		-p sensor_count:=${j} \
		-p publish_rate_hz:=100 \
		-p payload_size_kb:=20 \
		-p hardware_id:=0 \
		-p instance_count:=1 \
		-p benchmark_duration_sec:=180 
		'"

		sleep 5

		docker exec ros2_humble bash -lc "
		cd /root/ros2_ws &&
    	. run_script.sh &&
		ros2 run node_central node_central \
			--ros-args \
			-p instance_id:=0 \
			-p instance_count:=1\
			-p benchmark_number:=\\\"${i}\\\" \
			-p sensor_count:=\\\"${j}\\\" \
			-p benchmark_duration_sec:=180 \
			-p start_signal_delay_sec:=3 
		"

		sleep 5
	done
done

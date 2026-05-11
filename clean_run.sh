rm -rf build/ install/ log
conda deactivate
colcon build --packages-select cassio_interface
colcon build --packages-select cassio_driver
colcon build --packages-select node_central
source install/setup.bash
ros2 run node_central
ros2 run cassio_driver sensor_node
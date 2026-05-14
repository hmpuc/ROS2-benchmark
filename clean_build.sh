rm -rf build/ install/ log/

colcon build --packages-select cassio_interface
colcon build --packages-select cassio_driver
colcon build --packages-select node_central

source install/setup.bash
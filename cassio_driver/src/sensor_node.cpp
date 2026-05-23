#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp"
#include "std_msgs/msg/empty.hpp"

class SensorPublisher : public rclcpp::Node {
public:

    SensorPublisher()
        : Node("cassio_publisher_node")
    {
        //
        // Parameters
        //
        this->declare_parameter<int>(
            "sensor_count", 1);

        this->declare_parameter<int>(
            "publish_rate_hz", 100);

        this->declare_parameter<int>(
            "payload_size_kb", 20);

        this->declare_parameter<int>(
            "hardware_id", 1);

		this->declare_parameter<int>(
   			"instance_count", 1);

		instance_count_ =
    		this->get_parameter(
        		"instance_count").as_int();

		if (instance_count_ <= 0) {
		    throw std::runtime_error("instance_count must be > 0");
		}

        sensor_count_ =
            this->get_parameter(
                "sensor_count").as_int();

        publish_rate_hz_ =
            this->get_parameter(
                "publish_rate_hz").as_int();

        payload_size_bytes_ =
            this->get_parameter(
                "payload_size_kb").as_int()
            * 1024;

        hardware_id_ =
            this->get_parameter(
                "hardware_id").as_int();

		this->declare_parameter<int>(
			"benchmark_duration_sec",
			180);

		benchmark_duration_sec_ =
    		this->get_parameter(
        	"benchmark_duration_sec").as_int();
        
		
        // QoS
        auto qos =
            rclcpp::QoS(
                rclcpp::KeepLast(128))
                .best_effort();

		auto start_qos = rclcpp::QoS(rclcpp::KeepLast(1))
			.reliable()
			.transient_local();

        //
        // Publisher
        //

		for (int i = 0; i < instance_count_; i++) {

			std::string topic_name =
				"CassioData_" +
				std::to_string(i);

			publishers_.push_back(
				this->create_publisher<
					cassio_interface::msg::Cassio>(
						topic_name,
						qos));
		}

        //
        // Create sensors
        //
        sensors_.resize(sensor_count_);

        auto period =
            std::chrono::microseconds(
                static_cast<int>(
                    1e6 / publish_rate_hz_));

        for (int i = 0; i < sensor_count_; ++i)
        {
            auto &sensor = sensors_[i];

            sensor.name =
                "Cassio_x" +
                (std::to_string((hardware_id_ * sensor_count_) + i));

            sensor.sequence = 0;

			sensor.topic_index = 
				std::hash<std::string>{}( sensor.name) % instance_count_;

			sensor.msg.raw_data.resize(payload_size_bytes_, 0xAA);
			
            //
            // One timer per sensor
            //
			sensor.timer =
                this->create_wall_timer(
                    period,

                    [this, i]()
                    {
                        this->publish_sensor(i);
                    });
        }

		start_subscription_ =
			this->create_subscription<std_msgs::msg::Empty>(
				"/benchmark_start_signal",
				start_qos,
				std::bind(
					&SensorPublisher::start_signal_callback,
					this,
					std::placeholders::_1));

		benchmark_timer_ =
			this->create_wall_timer(
				std::chrono::milliseconds(200),
				std::bind(&SensorPublisher::check_benchmark, this));

        RCLCPP_INFO(
            this->get_logger(),
            "Started benchmark publisher");

        RCLCPP_INFO(
            this->get_logger(),
            "Sensors: %d",
            sensor_count_);

        RCLCPP_INFO(
            this->get_logger(),
            "Rate: %d Hz",
            publish_rate_hz_);

        RCLCPP_INFO(
            this->get_logger(),
            "Payload: %d bytes",
            payload_size_bytes_);

		RCLCPP_INFO(
			this->get_logger(),
			"Waiting benchmark start signal");
    }

private:

    struct SensorData
    {
        std::string name;

        uint64_t sequence;

		cassio_interface::msg::Cassio msg;

		size_t topic_index;

        rclcpp::TimerBase::SharedPtr timer;
    };

	void start_signal_callback(const std_msgs::msg::Empty::SharedPtr)
	{
		if (start_signal_received_) {
			return;
		}

		start_signal_received_ = true;
		benchmark_start_ns_ =
			(clock_.now() + rclcpp::Duration::from_seconds(5)).nanoseconds();

		RCLCPP_INFO(
			this->get_logger(),
			"Benchmark start signal received. Scheduled start ns: %ld",
			benchmark_start_ns_);
	}

    void publish_sensor(size_t index)
    {
		if (!start_signal_received_ || benchmark_finished_) {
			return;
		}

		auto now = clock_.now();

		if (now.nanoseconds() < benchmark_start_ns_) {
		    return;
		}

        auto &sensor = sensors_[index];

		auto &msg = sensor.msg;

        msg.sensor_name =
            sensor.name;

        msg.hardware_id =
            hardware_id_;

        msg.sequence =
            sensor.sequence++;

        msg.source_timestamp_ns = now.nanoseconds();

        //
        // Optional changing byte
        //
        msg.raw_data[0] = static_cast<uint8_t>( msg.sequence % 255);

        publishers_[sensor.topic_index] ->publish(msg);
    }

	void check_benchmark()
	{
		if (!start_signal_received_ || benchmark_finished_) {
			return;
		}

		auto now = clock_.now();

		if (now.nanoseconds() < benchmark_start_ns_) {
			return;
		}

		int64_t elapsed = now.nanoseconds() - benchmark_start_ns_;

		if (elapsed >= benchmark_duration_sec_ * 1'000'000'000LL) {

			if (!benchmark_finished_) {

				benchmark_finished_ = true;

				RCLCPP_INFO(
					get_logger(),
					"Benchmark finished");

				rclcpp::shutdown();
			}
		}
	}

private:

    struct SensorData;

    int sensor_count_;

    int publish_rate_hz_;

    int payload_size_bytes_;

    int hardware_id_;

	int instance_count_;

	int64_t benchmark_start_ns_ = 0;

	int benchmark_duration_sec_ = 180;

	bool benchmark_finished_ = false;

	bool start_signal_received_ = false;

    std::vector<SensorData> sensors_;

	std::vector<
    	rclcpp::Publisher<
        	cassio_interface::msg::Cassio>::SharedPtr>
           	 publishers_;

	rclcpp::TimerBase::SharedPtr benchmark_timer_;

	rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr start_subscription_;

	rclcpp::Clock clock_{RCL_SYSTEM_TIME};
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);

    auto node =
        std::make_shared<SensorPublisher>();

    rclcpp::executors::MultiThreadedExecutor executor;

    executor.add_node(node);

    executor.spin();

    rclcpp::shutdown();

    return 0;
}

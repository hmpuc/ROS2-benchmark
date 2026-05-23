#include <memory>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <thread>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp"
#include "std_msgs/msg/empty.hpp"

struct SensorStats {
    bool first_message = true;

    uint32_t last_sequence = 0;

    uint64_t received = 0;
    uint64_t lost = 0;

    double latency_sum_ms = 0.0;
    double max_latency_ms = 0.0;

    double jitter_sum_ms = 0.0;
    double max_jitter_ms = 0.0;
    double last_latency_ms = 0.0;
};

class CentralSubscriber : public rclcpp::Node {
public:

    CentralSubscriber()
        : Node("central_subscriber_"),
          benchmark_start_wall_time_(int64_t{0}, RCL_STEADY_TIME)
	{

        this->declare_parameter<int>(
            "benchmark_duration_sec",
            180);

        benchmark_duration_sec_ =
            this->get_parameter(
                "benchmark_duration_sec").as_int();

        this->declare_parameter<std::string>(
            "benchmark_number",
            "-1");

        benchmark_number =
            this->get_parameter(
                "benchmark_number").as_string();

        this->declare_parameter<std::string>(
            "sensor_count",
            "0");

        sensor_count =
            this->get_parameter(
                "sensor_count").as_string();

		this->declare_parameter<int>(
			"instance_id",
			0); 
			
		instance_id = 
			this->get_parameter(
				"instance_id").as_int();
		
		this->declare_parameter<int>(
			"instance_count",
			1);

		instance_count = 
			this->get_parameter(
				"instance_count").as_int();

		this->declare_parameter<int>(
			"start_signal_delay_sec",
			1);

		start_signal_delay_sec_ =
			this->get_parameter(
				"start_signal_delay_sec").as_int();

		if (start_signal_delay_sec_ < 0) {
			throw std::runtime_error("start_signal_delay_sec must be >= 0");
		}

		if (instance_id < 0 || instance_id >= instance_count) { 
			throw std::runtime_error( "Invalid instance_id"); 
		}

        auto qos = rclcpp::QoS(rclcpp::KeepLast(4096)).reliable();

		auto start_qos = rclcpp::QoS(rclcpp::KeepLast(1))
			.reliable()
			.transient_local();

		std::string topic_name = "CassioData_" + std::to_string(instance_id);

        //
        // Subscriber
        //
        subscription_ =
            this->create_subscription<
                cassio_interface::msg::Cassio>(
                    topic_name,
                    qos,

                    std::bind(
                        &CentralSubscriber::topic_callback,
                        this,
                        std::placeholders::_1));

		start_subscription_ =
			this->create_subscription<std_msgs::msg::Empty>(
				"/benchmark_start_signal",
				start_qos,
				std::bind(
					&CentralSubscriber::start_signal_callback,
					this,
					std::placeholders::_1));

		if (instance_id == 0) {
			start_publisher_ =
				this->create_publisher<std_msgs::msg::Empty>(
					"/benchmark_start_signal",
					start_qos);

			start_signal_publish_time_ =
				clock_.now() +
				rclcpp::Duration::from_seconds(start_signal_delay_sec_);

			start_signal_timer_ =
				this->create_wall_timer(
					std::chrono::milliseconds(200),
					std::bind(
						&CentralSubscriber::publish_start_signal,
						this));
		}

        RCLCPP_INFO(this->get_logger(), "Central benchmark subscriber started");

		if (instance_id == 0) {
			RCLCPP_INFO(
				this->get_logger(),
				"Instance 0 will publish benchmark start signal in %d seconds",
				start_signal_delay_sec_);
		} else {
			RCLCPP_INFO(
				this->get_logger(),
				"Waiting benchmark start signal");
		}

		benchmark_timer_ =
			this->create_wall_timer(
				std::chrono::milliseconds(200),
				std::bind(
					&CentralSubscriber::check_benchmark_timeout,
					this));
    }

	~CentralSubscriber() override = default;

private:

	void start_signal_callback(const std_msgs::msg::Empty::SharedPtr)
	{
		if (start_signal_received_) {
			return;
		}

		start_signal_received_ = true;
		benchmark_start_wall_time_ =
			clock_.now() + rclcpp::Duration::from_seconds(5);

		RCLCPP_INFO(
			this->get_logger(),
			"Benchmark start signal received. Scheduled start ns: %ld",
			benchmark_start_wall_time_.nanoseconds());
	}

	void publish_start_signal()
	{
		if (benchmark_finished_ || !start_publisher_) {
			return;
		}

		auto now = clock_.now();

		if (now < start_signal_publish_time_) {
			return;
		}

		if (start_signal_received_ && now >= benchmark_start_wall_time_) {
			start_signal_timer_->cancel();
			return;
		}

		std_msgs::msg::Empty msg;
		start_publisher_->publish(msg);
	}

    void topic_callback(const cassio_interface::msg::Cassio::SharedPtr msg) {
      
		if (benchmark_finished_) {
			return;
		}

		if (!start_signal_received_) {
			return;
		}

		auto now = clock_.now();

		if (now < benchmark_start_wall_time_) {
			return;
		}
        
        //
        // Current receive timestamp
        //
        int64_t now_ns = now.nanoseconds();

        //
        // Latency
        //
        double latency_ms = static_cast<double>(
                now_ns - msg->source_timestamp_ns)
            / 1e6;

        //
        // Sensor stats reference
        //
        auto &stats = sensor_stats_[msg->sensor_name];

        //
        // LOSS DETECTION
        //
        if (!stats.first_message) {
            uint32_t expected = stats.last_sequence + 1;

            if (msg->sequence > expected) {
                stats.lost += (msg->sequence - expected);
            }
        }

        stats.first_message = false;

        //
        // Update sequence
        //
        stats.last_sequence = msg->sequence;

        //
        // RECEIVED
        //
        stats.received++;

        //
        // LATENCY
        //
        stats.latency_sum_ms += latency_ms;

        if (latency_ms > stats.max_latency_ms) {
            stats.max_latency_ms = latency_ms;
        }

        //
        // JITTER
        //
        if (stats.received > 1) {
            double jitter_ms = std::abs(latency_ms - stats.last_latency_ms);

            stats.jitter_sum_ms += jitter_ms;

            if (jitter_ms > stats.max_jitter_ms) {
                stats.max_jitter_ms = jitter_ms;
            }
        }

        stats.last_latency_ms = latency_ms;

    }

    void save_temp_csv() {

        std::filesystem::create_directories("benchmarks");
        
		std::filesystem::create_directories("benchmarks/tmp");

		std::string filename =
			"benchmarks/tmp/benchmark_" +
			sensor_count + "_" +
			benchmark_number +
			"_instance_" +
			std::to_string(instance_id) +
			".tmp.csv";

		std::ofstream file(filename, std::ios::out);

        if (!file.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open CSV file");
            return;
        }

        //
        // CSV Header
        //
        file
            << "instance_id;"
			<< "sensor_name;"
            << "received;"
            << "lost;"
            << "loss_percent;"
            << "avg_latency_ms;"
            << "max_latency_ms;"
            << "avg_jitter_ms;"
            << "max_jitter_ms\n";

        //
        // Per sensor statistics
        //
        for (const auto &[sensor_name, stats]: sensor_stats_) {
            uint64_t total = stats.received + stats.lost;

            double loss_percent = 0.0;

            if (total > 0) {
                loss_percent = 100.0 * static_cast<double>(stats.lost)
                    / static_cast<double>(total);
            }

            double avg_latency_ms = 0.0;

            if (stats.received > 0) {
                avg_latency_ms = stats.latency_sum_ms 
                    / static_cast<double>(stats.received);
            }

            double avg_jitter_ms = 0.0;

            if (stats.received > 1) {
                avg_jitter_ms = stats.jitter_sum_ms
                    / static_cast<double>(stats.received - 1);
            }

            file
			    << instance_id << ";"
                << sensor_name << ";"
                << stats.received << ";"
                << stats.lost << ";"
                << loss_percent << ";"
                << avg_latency_ms << ";"
                << stats.max_latency_ms << ";"
                << avg_jitter_ms << ";"
                << stats.max_jitter_ms
                << "\n";
        }

        file.close();
		std::string done_file =
			filename + ".done";

		std::ofstream done(done_file);

		done << "done";

		done.close();
    }

	void finish_benchmark() {
		if (benchmark_finished_) {
			return;
		}

		benchmark_finished_ = true;

		RCLCPP_INFO(
			this->get_logger(),
			"Benchmark finished");

		save_temp_csv();

		if (instance_id == 0) {

			wait_for_all_instances();

			merge_csvs();

			cleanup_temp_files();
		}

		rclcpp::shutdown();
	}

	void wait_for_all_instances()
	{
		RCLCPP_INFO(
			this->get_logger(),
			"Waiting all instances...");

		auto start = std::chrono::steady_clock::now();

		while (rclcpp::ok()) {

			bool all_found = true;

			for (int i = 0; i < instance_count; i++) {

				std::string filename =
					"benchmarks/tmp/benchmark_" +
					sensor_count + "_" +
					benchmark_number +
					"_instance_" +
					std::to_string(i) +
					".tmp.csv.done";

				if (!std::filesystem::exists(filename)) {
					all_found = false;
					break;
				}
			}
		
			auto elapsed = std::chrono::steady_clock::now() - start;

			if (elapsed > std::chrono::seconds(30)) {

				RCLCPP_ERROR(
					this->get_logger(),
					"Timeout waiting instances");

				break;
			}

			if (all_found) {
				break;
			}

			std::this_thread::sleep_for(
				std::chrono::milliseconds(200));
		}
	}

	void merge_csvs()
	{
		std::string final_filename =
			"benchmarks/benchmark_results_" +
			sensor_count + "_" +
			benchmark_number +
			".csv";

		std::ofstream final_file(
			final_filename,
			std::ios::out);

		if (!final_file.is_open()) {

			RCLCPP_ERROR(
				this->get_logger(),
				"Failed to create final CSV");

			return;
		}

		bool first_file = true;

		for (int i = 0; i < instance_count; i++) {

			std::string filename =
				"benchmarks/tmp/benchmark_" +
				sensor_count + "_" +
				benchmark_number +
				"_instance_" +
				std::to_string(i) +
				".tmp.csv";

			std::ifstream input(filename);

			if (!input.is_open()) {
				continue;
			}

			std::string line;

			bool first_line = true;

			while (std::getline(input, line)) {

				//
				// Skip duplicated headers
				//
				if (!first_file && first_line) {
					first_line = false;
					continue;
				}

				final_file << line << "\n";

				first_line = false;
			}

			first_file = false;

			input.close();
		}

		final_file.flush();
		final_file.close();

		RCLCPP_INFO(
			this->get_logger(),
			"Final CSV merged");
	}

	void cleanup_temp_files()
	{
		for (int i = 0; i < instance_count; i++) {

			std::string filename =
				"benchmarks/tmp/benchmark_" +
				sensor_count + "_" +
				benchmark_number +
				"_instance_" +
				std::to_string(i) +
				".tmp.csv";

			std::filesystem::remove(filename);

			std::filesystem::remove(filename + ".done");
		}
	}

	void check_benchmark_timeout()
	{
		if (benchmark_finished_) {
			return;
		}

		if (!start_signal_received_) {
			return;
		}

		auto now = clock_.now();

		//
		// Wait benchmark start
		//
		if (now < benchmark_start_wall_time_) {
			return;
		}

		auto elapsed = now - benchmark_start_wall_time_;

		if (elapsed.seconds() >= benchmark_duration_sec_) {

			RCLCPP_INFO(
				this->get_logger(),
				"Benchmark timeout reached");

			finish_benchmark();
		}
	}

private:

    rclcpp::Subscription<cassio_interface::msg::Cassio>::SharedPtr subscription_;

	rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr start_subscription_;

	rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr start_publisher_;

    std::unordered_map<std::string,SensorStats> sensor_stats_;

    int benchmark_duration_sec_ = 180;

    std::string benchmark_number = "-1"; 

    std::string sensor_count = "0";

	int instance_count = 1;

	int instance_id = 0;

	bool benchmark_finished_ = false;

	bool start_signal_received_ = false;

	rclcpp::Time benchmark_start_wall_time_;

	rclcpp::TimerBase::SharedPtr benchmark_timer_;

	rclcpp::TimerBase::SharedPtr start_signal_timer_;

	rclcpp::Time start_signal_publish_time_{int64_t{0}, RCL_STEADY_TIME};

	int start_signal_delay_sec_ = 1;

	rclcpp::Clock clock_{RCL_STEADY_TIME};
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);

    auto node = std::make_shared<CentralSubscriber>();

    //
    // IMPORTANT:
    // Single threaded benchmark
    //
    rclcpp::executors::SingleThreadedExecutor executor;

    executor.add_node(node);

    executor.spin();

    if (rclcpp::ok()) {
		rclcpp::shutdown();
	}

    return 0;
}

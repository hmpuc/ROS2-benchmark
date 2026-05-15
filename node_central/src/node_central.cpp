#include <memory>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp"

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
        : Node("central_subscriber") {
        //
        // QoS
        //

        this->declare_parameter<int>(
            "benchmark_duration_sec",
            180);

        benchmark_duration_sec_ =
            this->get_parameter(
                "benchmark_duration_sec").as_int();


        auto qos = rclcpp::QoS(rclcpp::KeepLast(1000)).reliable();

        //
        // Subscriber
        //
        subscription_ =
            this->create_subscription<
                cassio_interface::msg::Cassio>(
                    "CassioData",
                    qos,

                    std::bind(
                        &CentralSubscriber::topic_callback,
                        this,
                        std::placeholders::_1));

        start_time_ns_ = this->get_clock()->now().nanoseconds();

        RCLCPP_INFO(this->get_logger(), "Central benchmark subscriber started");
    }

    ~CentralSubscriber() override {
        save_csv();

        RCLCPP_INFO(this->get_logger(), "Benchmark CSV saved");
    }

private:

    void topic_callback(const cassio_interface::msg::Cassio::SharedPtr msg) {
      
        //
        // Start benchmark on first message
        //
        if (!benchmark_started_) {
            benchmark_started_ = true;

            benchmark_start_time_ =
                this->get_clock()->now();

            RCLCPP_INFO(
                this->get_logger(),
                "Benchmark started");
        }


        
        //
        // Current receive timestamp
        //
        int64_t now_ns = this->get_clock()->now().nanoseconds();

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

        //
        // Benchmark duration check
        //
        auto elapsed =
            this->get_clock()->now()
            - benchmark_start_time_;

        if (elapsed.seconds() >= benchmark_duration_sec_) {

            RCLCPP_INFO(
                this->get_logger(),
                "Benchmark finished");

            save_csv();

            rclcpp::shutdown();

            return;
        }

    }

    void save_csv() {
        std::ofstream file("benchmark_results.csv", std::ios::out);

        if (!file.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Failed to open CSV file");
            return;
        }

        //
        // CSV Header
        //
        file
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
    }

private:

    rclcpp::Subscription<cassio_interface::msg::Cassio>::SharedPtr subscription_;

    std::unordered_map<std::string,SensorStats> sensor_stats_;

    //
    // Benchmark timing
    //
    bool benchmark_started_ = false;

    int benchmark_duration_sec_ = 180;

    rclcpp::Time benchmark_start_time_;

    int64_t start_time_ns_{0};


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

    rclcpp::shutdown();

    return 0;
}
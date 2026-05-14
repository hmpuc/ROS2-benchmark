#include <vector>
#include <string>
#include <memory>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp"

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

        //
        // QoS
        //
        auto qos =
            rclcpp::QoS(
                rclcpp::KeepLast(1000))
                .reliable();

        //
        // Publisher
        //
        publisher_ =
            this->create_publisher<
                cassio_interface::msg::Cassio>(
                    "CassioData",
                    qos);

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
                std::to_string(i);

            sensor.sequence = 0;

            sensor.payload.resize(
                payload_size_bytes_,
                0xAA);

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
    }

private:

    struct SensorData
    {
        std::string name;

        uint64_t sequence;

        std::vector<uint8_t> payload;

        rclcpp::TimerBase::SharedPtr timer;
    };

    void publish_sensor(size_t index)
    {
        auto &sensor = sensors_[index];

        cassio_interface::msg::Cassio msg;

        msg.sensor_name =
            sensor.name;

        msg.hardware_id =
            hardware_id_;

        msg.sequence =
            sensor.sequence++;

        msg.source_timestamp_ns =
            this->get_clock()
                ->now()
                .nanoseconds();

        //
        // Reuse payload
        //
        msg.raw_data =
            sensor.payload;

        //
        // Optional changing byte
        //
        msg.raw_data[0] =
            static_cast<uint8_t>(
                msg.sequence % 255);

        publisher_->publish(msg);
    }

private:

    struct SensorData;

    int sensor_count_;

    int publish_rate_hz_;

    int payload_size_bytes_;

    int hardware_id_;

    std::vector<SensorData> sensors_;

    rclcpp::Publisher<
        cassio_interface::msg::Cassio>::SharedPtr
            publisher_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);

    auto node =
        std::make_shared<SensorPublisher>();

    //
    // REAL concurrent publishers
    //
    rclcpp::executors::MultiThreadedExecutor executor;

    executor.add_node(node);

    executor.spin();

    rclcpp::shutdown();

    return 0;
}

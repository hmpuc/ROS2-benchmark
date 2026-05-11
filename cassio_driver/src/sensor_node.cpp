#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp" // Header gerado

class SensorPublisher : public rclcpp::Node {
public:
    SensorPublisher() : Node("cassio_publisher_node") {
        publisher_ = this->create_publisher<cassio_interface::msg::Cassio>("CassioData", rclcpp::QoS(1000).reliable());
        timer_ = this->create_wall_timer(std::chrono::seconds(1), std::bind(&SensorPublisher::publish_data, this));
    }

private:
    void publish_data() {
        auto message = cassio_interface::msg::Cassio();
        static int counter = 0;
        message.sensor_name = "Cassio_x1";
        message.hardware_id = 42;
        message.timestamp = this->now().seconds();

        std::vector<uint8_t> bytes = {0x00}; 
        
        message.raw_data = bytes;
        message.raw_data[0] = counter++;

        RCLCPP_INFO(this->get_logger(), "Publicando %zu bytes | valor: %d", message.raw_data.size(), message.raw_data[0]);
        publisher_->publish(message);
    }

    rclcpp::Publisher<cassio_interface::msg::Cassio>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(void)
{
    rclcpp::init(0, nullptr);
    rclcpp::spin(std::make_shared<SensorPublisher>());
    rclcpp::shutdown();
    return 0;
}
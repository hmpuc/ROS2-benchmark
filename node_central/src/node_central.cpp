#include <memory>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include "rclcpp/rclcpp.hpp"
#include "cassio_interface/msg/cassio.hpp"

class CentralSubscriber : public rclcpp::Node {
public:
    CentralSubscriber() : Node("central_subscriber") {
        this->declare_parameter<int>("sensor_num", 1);
        this->declare_parameter<int>("run_num", 1);

        int sensor_num = this->get_parameter("sensor_num").as_int();
        if (sensor_num <= 0) {
            RCLCPP_ERROR(this->get_logger(), "Erro: sensor_num should be bigger than zero! (sensor_num: %d)", sensor_num);
            throw std::runtime_error("sensor_num less or equal zero");
        }

        int run_num = this->get_parameter("run_num").as_int();
        if (run_num <= 0) {
            RCLCPP_ERROR(this->get_logger(), "Erro: run_num should be bigger than zero! (run_num: %d)", run_num);
            throw std::runtime_error("run_num less or equal zero");
        }

        log_file_.open("log.csv", std::ios::out | std::ios::app);
        if (!log_file_.is_open()) {
            RCLCPP_ERROR(this->get_logger(), "Falha crítica: Não foi possível criar o arquivo.");
            throw std::runtime_error("Erro ao abrir arquivo");
        }

        start_time_ns_ = this->get_clock()->now().nanoseconds();

        subscription_ = this->create_subscription<cassio_interface::msg::Cassio>(
            "CassioData", rclcpp::QoS(1000).reliable(),
            [this](const cassio_interface::msg::Cassio::SharedPtr msg) {
                this->topic_callback(msg);
            }
        );
        RCLCPP_INFO(this->get_logger(), "Subscribe: CassioData");
    }

    ~CentralSubscriber() override {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

private:
    void topic_callback(const cassio_interface::msg::Cassio::SharedPtr msg) {
        if (msg->raw_data.empty()) {
            return;
        }

        uint8_t last_byte = msg->raw_data.back();
        int64_t now_ns = this->get_clock()->now().nanoseconds();
        int64_t diff_ms = (now_ns - start_time_ns_) / 1000000LL;

        log_file_ << msg->sensor_name << "; " << msg->hardware_id << "; "
                << static_cast<int>(last_byte) << "; "
                << diff_ms << std::endl;

        // RCLCPP_INFO(
        //     this->get_logger(),
        //     "Salvo byte %d do Sensor %d",
        //     static_cast<int>(last_byte),
        //     msg->hardware_id
        // );
    }

    std::ofstream log_file_;
    int64_t start_time_ns_{0};
    rclcpp::Subscription<cassio_interface::msg::Cassio>::SharedPtr subscription_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CentralSubscriber>());
    rclcpp::shutdown();
    return 0;
}
